#include "ae_ovl1a0.h"

#include "../core/ae_pose.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../bodies/ae_bodymove.h"
#include "../player/ae_player_spawn.h"
#include "../player/ae_player2.h"
#include "../world/ae_zako_init.h"
#include "../world/ae_zako2.h"
#include "../world/ae_misc.h"
#include "../world/ae_gimmick_b.h"

#include <limits.h>

/* The overlay frequently stores a signed halfword through a word register. */
static int32_t ovl1a0_s16(uint32_t value) {
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) ? (int32_t)(low | 0xFFFF0000u) : (int32_t)low;
}

static int32_t ovl1a0_asr(int32_t value, uint32_t shift) {
    uint32_t bits = (uint32_t)value;
    if (shift == 0u) return value;
    if (shift >= 32u) return value < 0 ? -1 : 0;
    if (value >= 0) return (int32_t)(bits >> shift);
    return (int32_t)~((~bits) >> shift);
}

static int32_t ovl1a0_call(AeRam *r, const AeOvl1A0Hooks *h,
                           const char *name, uint32_t address,
                           uint32_t count, const uint32_t *args) {
    if (h == NULL || h->original_call == NULL) {
        ae_ram_fail(r, "Area46 overlay call hook missing: %s", name);
        return 0;
    }
    return h->original_call(h->user, r, name, address, count, args);
}

static int32_t ovl1a0_render(AeRam *r, const AeOvl1A0Hooks *h,
                             const char *name, uint32_t address,
                             uint32_t count, const uint32_t *args) {
    if (h == NULL || h->renderer == NULL) {
        ae_ram_fail(r, "Area46 renderer hook missing: %s", name);
        return 0;
    }
    return h->renderer(h->user, r, name, address, count, args);
}

static uint32_t ovl1a0_actor_record(uint32_t index) {
    return AE_OVL1A0_ACTOR_RECORDS + index * AE_OVL1A0_ACTOR_RECORD_SIZE;
}

static uint32_t ovl1a0_model_record(uint32_t index) {
    return AE_OVL1A0_MODEL_RECORDS + index * AE_OVL1A0_MODEL_RECORD_SIZE;
}

static uint32_t ovl1a0_shockwave_record(uint32_t index) {
    return AE_OVL1A0_SHOCKWAVE_RECORDS + index * AE_OVL1A0_SHOCKWAVE_SIZE;
}

static uint32_t ovl1a0_debris_record(uint32_t index) {
    return AE_OVL1A0_DEBRIS_RECORDS + index * AE_OVL1A0_DEBRIS_SIZE;
}

static uint32_t ovl1a0_random16(AeRam *r) {
    uint32_t next = (ae_rd16(r, AE_OVL1A0_RAND_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_OVL1A0_RAND_SEED, next);
    return next;
}

static int32_t ovl1a0_div_toward_zero(int32_t numerator, int32_t denominator) {
    if (numerator >= 0) return numerator / denominator;
    return -(int32_t)((uint32_t)(-(int64_t)numerator) /
                      (uint32_t)denominator);
}

static int32_t ovl1a0_clamp(int32_t value, int32_t low, int32_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

/* 8013D440: bind a model descriptor and initialize its instance. The two
 * loops are GTE lighting/placement work, delegated to the renderer. */
void ae_Area46_Boss_InitializeModelEntry(AeRam *r,
                                          const AeOvl1A0Hooks *h,
                                          uint32_t index, uint32_t model,
                                          uint32_t output, uint32_t mode) {
    (void)h;
    (void)mode;
    uint32_t model_data = ae_rd32(r, AE_OVL1A0_AREA_MODEL_DATA_POINTER);
    uint32_t model_layout = model_data +
        ae_rd32(r, model_data + AE_OVL1A0_MODEL_DATA_LAYOUT_OFFSET) +
        index * AE_OVL1A0_MODEL_LAYOUT_STRIDE;
    uint32_t instance = model_data +
        ae_rd32(r, model_layout + AE_OVL1A0_MODEL_LAYOUT_INSTANCE);
    uint32_t header = model_data +
        ae_rd32(r, model_layout + AE_OVL1A0_MODEL_LAYOUT_HEADER);
    uint32_t pointer_slot = index * AE_OVL1A0_MODEL_POINTER_SLOT_SIZE;

    ae_wr32(r, AE_OVL1A0_MODEL_INSTANCE_POINTERS + pointer_slot, instance);
    ae_wr32(r, AE_OVL1A0_MODEL_HEADER_POINTERS + pointer_slot, header);
    ae_wr32(r, output, header + AE_OVL1A0_MODEL_HEADER_FIRST_POINTER);
    ae_wr32(r, model + AE_OVL1A0_MODEL_NEXT_HEADER,
            header + AE_OVL1A0_MODEL_HEADER_FIRST_POINTER +
            ae_rd16(r, header + AE_OVL1A0_MODEL_HEADER_COUNT) *
                         AE_OVL1A0_MODEL_POINTER_SLOT_SIZE);

    uint32_t instance_header = ae_rd32(
        r, AE_OVL1A0_MODEL_INSTANCE_POINTERS + pointer_slot);
    ae_ModelInitAuto(r, model, instance_header);
}

/* 8013D000: register this model's actor index in the per-area actor table. */
int32_t ae_Area46_Boss_RegisterEntry(AeRam *r, uint32_t actor) {
    uint32_t source_id = ae_rd16(r, actor + AE_OVL1A0_ACTOR_SOURCE_ID);
    int32_t actor_index = ae_rds16(r, actor + AE_OVL1A0_ACTOR_INDEX);
    int32_t actor_type = ae_rds16(r, actor + AE_OVL1A0_ACTOR_TYPE);
    uint32_t table = ae_rd32(r, AE_OVL1A0_ACTOR_TABLE_POINTER);
    uint32_t actor_record = table + (uint32_t)actor_index *
                            AE_OVL1A0_ACTOR_TABLE_STRIDE;

    ae_wr16(r, actor + AE_OVL1A0_ACTOR_CLEAR_FIELD, 0);
    ae_wr16(r, actor + AE_OVL1A0_ACTOR_COPY_FIELD, source_id);
    ae_wr16(r, actor_record + AE_OVL1A0_ACTOR_TABLE_FLAGS,
            ae_rd16(r, actor_record + AE_OVL1A0_ACTOR_TABLE_FLAGS) | 0x8000u);
    if (actor_type >= 0) {
        uint32_t flag = ae_rd32(r, AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER) +
                        (uint32_t)actor_type * 32u;
        ae_wr16(r, flag + AE_OVL1A0_ACTOR_FLAG_FIELD, 0);
    }
    return 0;
}

/* 8013D06C: advance the encounter actor's intro / feature-30 state. */
void ae_Area46_Boss_UpdateEncounterActor(AeRam *r,
                                          const AeOvl1A0Hooks *h,
                                          uint32_t actor) {
    int32_t state = ae_rds16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE);
    uint32_t actor_index = ae_rd16(r, actor + AE_OVL1A0_ACTOR_INDEX);
    int32_t actor_type = ae_rds16(r, actor + AE_OVL1A0_ACTOR_TYPE);
    uint32_t progress = AE_OVL1A0_BOSS_CLEAR_PROGRESS_HALFWORD;
    uint32_t sound_mode_args[2];

    if (state == 0) {
        if (ae_rds32(r, AE_OVL1A0_PLAYER_STATE) < -447) return;
        ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 1);
        uint32_t table = ae_rd32(r, AE_OVL1A0_ACTOR_TABLE_POINTER);
        uint32_t entry = table + actor_index * AE_OVL1A0_ACTOR_TABLE_STRIDE;
        uint32_t flags = ae_rd16(r, entry + AE_OVL1A0_ACTOR_TABLE_FLAGS) &
                         ~0x8000u;
        ae_wr16(r, entry + AE_OVL1A0_ACTOR_TABLE_FLAGS, flags);
        if (actor_type >= 0) {
            uint32_t flag = ae_rd32(r, AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER) +
                            (uint32_t)actor_type * 32u;
            ae_wr16(r, flag + AE_OVL1A0_ACTOR_FLAG_FIELD, UINT16_MAX);
        }
        const uint32_t volume_args[2] = {2u, 40u};
        (void)ovl1a0_call(r, h, "BgmSetVolumeMode",
                          AE_OVL1A0_EXE_BGM_SET_VOLUME_MODE, 2,
                          volume_args);
        const uint32_t feature_args[1] = {7u};
        (void)ovl1a0_call(r, h, "AreaFeature30Request",
                          AE_OVL1A0_EXE_AREA_FEATURE_30_REQUEST, 1,
                          feature_args);
        return;
    }

    if (state == 1) {
        if (ae_AreaFeature30GetState(r) != 0) return;
        sound_mode_args[0] = 0;
        sound_mode_args[1] = 0;
        (void)ovl1a0_call(r, h, "BgmSetVolumeMode",
                          AE_OVL1A0_EXE_BGM_SET_VOLUME_MODE, 2,
                          sound_mode_args);
        ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 3);
        ae_Area46_Boss_OnBossStart(r, h);
        return;
    }

    if (state == 2) {
        if (ae_rds32(r, AE_OVL1A0_PLAYER_STATE) < -447) return;
        ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 3);
        uint32_t table = ae_rd32(r, AE_OVL1A0_ACTOR_TABLE_POINTER);
        uint32_t entry = table + actor_index * AE_OVL1A0_ACTOR_TABLE_STRIDE;
        uint32_t flags = ae_rd16(r, entry + AE_OVL1A0_ACTOR_TABLE_FLAGS) &
                         ~0x8000u;
        ae_wr16(r, entry + AE_OVL1A0_ACTOR_TABLE_FLAGS, flags);
        if (actor_type >= 0) {
            uint32_t flag = ae_rd32(r, AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER) +
                            (uint32_t)actor_type * 32u;
            ae_wr16(r, flag + AE_OVL1A0_ACTOR_FLAG_FIELD, UINT16_MAX);
        }
        ae_Area46_Boss_OnBossStart(r, h);
        return;
    }

    if (state == 3) {
        if (ae_rd16(r, progress) != 1u) return;
        ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 4);
        uint32_t table = ae_rd32(r, AE_OVL1A0_ACTOR_TABLE_POINTER);
        uint32_t entry = table + actor_index * AE_OVL1A0_ACTOR_TABLE_STRIDE;
        uint32_t flags = ae_rd16(r, entry + AE_OVL1A0_ACTOR_TABLE_FLAGS) |
                         0x8000u;
        ae_wr16(r, entry + AE_OVL1A0_ACTOR_TABLE_FLAGS, flags);
        if (actor_type >= 0) {
        uint32_t flag = ae_rd32(r, AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER) +
                        (uint32_t)actor_type * 32u;
            ae_wr16(r, flag + AE_OVL1A0_ACTOR_FLAG_FIELD, 0);
        }
        if (h == NULL || h->stagefade_hooks == NULL) {
            ae_ram_fail(r, "Area46 stage-fade hooks not provided");
            return;
        }
        (void)ae_SoundStop(r, h->stagefade_hooks, 2);
        const uint32_t feature_args[1] = {8u};
        (void)ovl1a0_call(r, h, "AreaFeature30Request",
                          AE_OVL1A0_EXE_AREA_FEATURE_30_REQUEST, 1,
                          feature_args);
        return;
    }

    if (state == 4 && ae_AreaFeature30GetState(r) == 0)
        ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 5);
}

/* 8013D2A0: enter/leave the boss trigger volume and update the actor mesh. */
int32_t ae_Area46_Boss_UpdateEntryProximity(AeRam *r,
                                             const AeOvl1A0Hooks *h,
                                             uint32_t actor) {
    int32_t state = ae_rds16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE);
    if (state != 0 && state != 1) return 0;

    uint32_t player = AE_OVL1A0_PLAYER_OBJECT;
    int32_t dx = ae_rds32(r, player + AE_OBJ_POS_X) - 1568;
    int32_t dy = ae_rds32(r, player + AE_OBJ_POS_Y) + 2560;
    int32_t dz = ae_rds32(r, player + AE_OBJ_POS_Z) + 3520;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dz < 0) dz = -dz;
    int in_range = dx < 928 && dy < 256 && dz < 448;
    if (state == 0) {
        if (!in_range) return 0;
        ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 1);
        int32_t mesh = ae_rds16(r, actor + AE_OVL1A0_ACTOR_MESH_ENTER);
        uint32_t actor_type = (uint32_t)ae_rd16(r, actor + AE_OVL1A0_ACTOR_TYPE);
        uint32_t table = ae_rd32(r, AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER) +
                         actor_type * 32u;
        const uint32_t args[2] = {table, (uint32_t)mesh};
        (void)ovl1a0_call(r, h, "MeshInitFromAreaTable",
                          AE_OVL1A0_EXE_MESH_INIT_FROM_AREA,
                          2, args);
        return 0;
    }

    if (in_range) return 0;
    int32_t mesh = ae_rds16(r, actor + AE_OVL1A0_ACTOR_MESH_EXIT);
    ae_wr16(r, actor + AE_OVL1A0_ACTOR_ENCOUNTER_STATE, 0);
    uint32_t actor_type = (uint32_t)ae_rd16(r, actor + AE_OVL1A0_ACTOR_TYPE);
    uint32_t table = ae_rd32(r, AE_OVL1A0_ACTOR_FLAG_TABLE_POINTER) +
                     actor_type * 32u;
    const uint32_t args[2] = {table, (uint32_t)mesh};
    (void)ovl1a0_call(r, h, "MeshInitFromAreaTable",
                      AE_OVL1A0_EXE_MESH_INIT_FROM_AREA,
                      2, args);
    return 0;
}

/* 8013D63C: project the actor, cull it, then prepare and emit its model. */
int32_t ae_Area46_Boss_ProjectAndEmit(AeRam *r, const AeOvl1A0Hooks *h,
                                      uint32_t model, uint32_t matrix,
                                      uint32_t mode) {
    const uint32_t view_matrix = AE_OVL1A0_CAMERA_MATRIX;
    const uint32_t set_matrix_args[1] = {view_matrix};
    (void)ovl1a0_render(r, h, "GTE_SetRotMatrix",
                        AE_OVL1A0_EXE_GTE_SET_ROT_MATRIX, 1,
                        set_matrix_args);
    (void)ovl1a0_render(r, h, "GTE_SetTransMatrix",
                        AE_OVL1A0_EXE_GTE_SET_TRANS_MATRIX, 1,
                        set_matrix_args);

    uint32_t vector = AE_OVL1A0_STACK_VECTOR;
    uint32_t screen = AE_OVL1A0_STACK_PROJECTED_SCREEN;
    uint32_t depth = AE_OVL1A0_STACK_PROJECTED_SCREEN +
                     AE_OVL1A0_STACK_DEPTH;
    uint32_t flag = AE_OVL1A0_STACK_PROJECTED_SCREEN +
                    AE_OVL1A0_STACK_FLAGS;
    ae_wr16(r, vector + AE_OVL1A0_VECTOR16_X,
            ae_rd16(r, model + AE_OBJ_POS_X));
    ae_wr16(r, vector + AE_OVL1A0_VECTOR16_Y,
            ae_rd16(r, model + AE_OBJ_POS_Y));
    ae_wr16(r, vector + AE_OVL1A0_VECTOR16_Z,
            ae_rd16(r, model + AE_OBJ_POS_Z));
    const uint32_t project_args[4] = {vector, screen, depth, flag};
    int32_t projected_depth = ovl1a0_render(
        r, h, "RotTransPers", AE_OVL1A0_EXE_ROT_TRANS_PERS, 4,
        project_args);
    if (ae_ram_failed(r)) return 1;

    uint32_t screen_x = ae_rd16(r, screen);
    int32_t screen_y = ae_rds16(r, screen + AE_OVL1A0_SCREEN_RESULT_Y);
    if (((screen_x + 128u) & 0xFFFFu) >= 641u || screen_y < -128 ||
        screen_y >= 441 || projected_depth >= 4097 || projected_depth <= 0)
        return 1;

    (void)ae_sdk_MatrixLookDir(r, model + AE_OVL1A0_MODEL_LIGHT_DIRECTION,
                               ae_rd32(r, model + AE_OVL1A0_MODEL_MATRIX_POINTER));
    (void)ae_sdk_ScaleMatrix(r,
                             ae_rd32(r, model + AE_OVL1A0_MODEL_MATRIX_POINTER),
                             matrix);
    ae_ModelPoseBones(r, model, 3u);

    const uint32_t transform_args[1] = {model};
    (void)ovl1a0_render(r, h, "ModelTransformSkinned",
                        AE_OVL1A0_EXE_MODEL_TRANSFORM_SKINNED,
                        1, transform_args);
    if (ae_ram_failed(r)) return 0;

    uint32_t model_selector = ae_rd32(r, AE_OVL1A0_OT_SELECTOR);
    uint32_t lighting_model = AE_OVL1A0_OT_DISPLAY_RECORDS +
        model_selector * AE_OVL1A0_MODEL_INSTANCE_STRIDE;
    const uint32_t lighting_args[4] = {
        lighting_model, AE_OVL1A0_MODEL_LIGHTING_DATA, model, mode
    };
    (void)ovl1a0_render(r, h, "Area46_Boss_ModelApplyLighting",
                        AE_OVL1A0_MODEL_LIGHTING_RENDER_HOOK, 4,
                        lighting_args);
    return 0;
}

/* 8013D77C: make a collision body for a nearby effect object. */
void ae_Area46_Boss_SpawnEffectNear(AeRam *r, const AeOvl1A0Hooks *h,
                                    uint32_t effect, uint32_t owner,
                                    int32_t body_param, int32_t body_flags,
                                    int32_t body_anim) {
    int32_t dx = ae_rds32(r, AE_OVL1A0_PLAYER_OBJECT + AE_OVL1A0_PLAYER_X) -
                 ae_rds16(r, effect + AE_OVL1A0_EFFECT_POS_X);
    int32_t dz = ae_rds32(r, AE_OVL1A0_PLAYER_OBJECT + AE_OVL1A0_PLAYER_Z) -
                 ae_rds16(r, effect + AE_OVL1A0_EFFECT_POS_Z);
    int32_t distance_squared = (int32_t)((uint32_t)((int64_t)dx * dx) +
                                         (uint32_t)((int64_t)dz * dz));
    if (distance_squared > 0x24000) {
        ae_wr32(r, effect + AE_OVL1A0_EFFECT_BODY_POINTER, 0);
        return;
    }

    uint32_t body = ae_CollisionBodyAlloc(r);
    ae_wr32(r, effect + AE_OVL1A0_EFFECT_BODY_POINTER, body);
    if (body == 0u) return;

    ae_wr32(r, body + AE_OVL1A0_BODY_POSITION_X,
            (uint32_t)ae_rds16(r, effect + AE_OVL1A0_EFFECT_POS_X));
    ae_wr32(r, body + AE_OVL1A0_BODY_POSITION_Y,
            (uint32_t)ae_rds16(r, effect + AE_OVL1A0_EFFECT_POS_Y));
    ae_wr32(r, body + AE_OVL1A0_BODY_POSITION_Z,
            (uint32_t)ae_rds16(r, effect + AE_OVL1A0_EFFECT_POS_Z));
    ae_wr16(r, body + AE_OVL1A0_BODY_PARAM, (uint32_t)body_param);
    ae_wr16(r, body + AE_OVL1A0_BODY_FLAGS,
            (uint32_t)body_flags | 1u);
    ae_wr16(r, body + AE_OVL1A0_BODY_ANIM, (uint32_t)body_anim);
    (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL, 7, body,
                                   owner);
}

/* 8013D890: the fourth argument is a signed 16-bit fifth argument to D77C. */
void ae_Area46_Boss_SpawnEffectNearWrap(AeRam *r, const AeOvl1A0Hooks *h,
                                        uint32_t effect, uint32_t owner,
                                        int32_t body_param,
                                        int32_t body_anim) {
    ae_Area46_Boss_SpawnEffectNear(r, h, effect, owner, body_param, 0,
                                   ovl1a0_s16((uint32_t)body_anim));
}

/* 8013D8BC: clear the 80 effect lifetime bytes in reverse table order. */
void ae_Area46_Boss_ClearDebris(AeRam *r) {
    for (uint32_t index = AE_OVL1A0_DEBRIS_COUNT; index != 0u; --index)
        ae_wr8(r, ovl1a0_debris_record(index - 1u) +
                AE_OVL1A0_DEBRIS_LIFETIME, 0);
}

/* 8013D8E4: scatter 80 fragments around the supplied world-space position. */
void ae_Area46_Boss_SpawnDebris(AeRam *r, uint32_t source) {
    int32_t source_x = ae_rds16(r, source);
        int32_t source_y = ae_rds16(r, source + AE_OVL1A0_VECTOR16_Y);
        int32_t source_z = ae_rds16(r, source + AE_OVL1A0_VECTOR16_Z);

    for (uint32_t remaining = AE_OVL1A0_DEBRIS_COUNT; remaining != 0u;
         --remaining) {
        uint32_t record = ovl1a0_debris_record(remaining - 1u);
        uint32_t life_random = ovl1a0_random16(r);
        ae_wr8(r, record + AE_OVL1A0_DEBRIS_LIFETIME,
               ((life_random >> 8) & 0x1Fu) + 40u);

        uint32_t x_random = ovl1a0_random16(r);
        uint32_t y_random = ovl1a0_random16(r);
        int32_t x_offset = ((int32_t)(x_random >> 8) - 128) * 2;
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_POS_X,
                (uint32_t)(source_x + x_offset));

        uint32_t z_random = ovl1a0_random16(r);
        int32_t y_offset = ((int32_t)(y_random >> 8) - 128) * 2;
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_POS_Y,
                (uint32_t)(source_y - y_offset));

        uint32_t y_velocity_random = ovl1a0_random16(r);
        int32_t z_offset = ((int32_t)(z_random >> 8) - 128) * 2;
        int32_t x_velocity = ((int32_t)(y_velocity_random >> 8) - 128) / 2;
        ae_wr8(r, record + AE_OVL1A0_DEBRIS_VELOCITY_X,
               (uint32_t)x_velocity);
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_POS_Z,
                (uint32_t)(source_z + z_offset));

        uint32_t x_velocity_random = ovl1a0_random16(r);
        int32_t y_velocity = -((int32_t)(x_velocity_random >> 8) & 0x3F) - 32;
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_VELOCITY_Y,
                (uint32_t)y_velocity);

        uint32_t z_velocity_random = ovl1a0_random16(r);
        int32_t z_velocity = ((int32_t)(z_velocity_random >> 8) - 128) / 2;
        ae_wr8(r, record + AE_OVL1A0_DEBRIS_VELOCITY_Z,
               (uint32_t)z_velocity);
    }
}

/* 8013DA4C: advance fragment lifetimes and hand each surviving draw to the renderer. */
void ae_Area46_Boss_UpdateDebris(AeRam *r, const AeOvl1A0Hooks *h) {
    uint32_t camera_copy = AE_OVL1A0_GTE_CAMERA_COPY;
    for (uint32_t word = 0; word < 8u; ++word)
        ae_wr32(r, camera_copy + word * 4u,
                ae_rd32(r, AE_OVL1A0_CAMERA_MATRIX + word * 4u));

    static const int16_t vertices[4][9] = {
        {-24, -24, 0, 24, -24, 0, 0, 24, 0},
        {-48, -48, 0, 48, -48, 0, 0, 48, 0},
        {-8, -48, 0, 8, -48, 0, 0, 72, 0},
        {-12, -12, 0, 12, -12, 0, 0, 12, 0},
    };
    static const uint32_t quad_offsets[4] = {
        AE_OVL1A0_DEBRIS_GTE_QUAD_0, AE_OVL1A0_DEBRIS_GTE_QUAD_1,
        AE_OVL1A0_DEBRIS_GTE_QUAD_2, AE_OVL1A0_DEBRIS_GTE_QUAD_3,
    };
    for (uint32_t quad = 0; quad < 4u; ++quad) {
        for (uint32_t point = 0; point < 9u; ++point)
            ae_wr16(r, AE_OVL1A0_GTE_SCRATCH_BASE + quad_offsets[quad] +
                       (point / 3u) * 8u + (point % 3u) * 2u,
                    (uint32_t)vertices[quad][point]);
    }

    for (uint32_t remaining = AE_OVL1A0_DEBRIS_COUNT; remaining != 0u;
         --remaining) {
        uint32_t record = ovl1a0_debris_record(remaining - 1u);
        uint32_t lifetime = ae_rd8(r, record + AE_OVL1A0_DEBRIS_LIFETIME);
        if (lifetime == 0u) continue;
        --lifetime;
        ae_wr8(r, record + AE_OVL1A0_DEBRIS_LIFETIME, lifetime);

        int32_t vel_x = ae_rds8(r, record + AE_OVL1A0_DEBRIS_VELOCITY_X);
        int32_t vel_z = ae_rds8(r, record + AE_OVL1A0_DEBRIS_VELOCITY_Z);
        int32_t vel_y = ae_rds16(r, record + AE_OVL1A0_DEBRIS_VELOCITY_Y);
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_POS_X,
                ae_rd16(r, record + AE_OVL1A0_DEBRIS_POS_X) + vel_x);
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_POS_Y,
                ae_rd16(r, record + AE_OVL1A0_DEBRIS_POS_Y) + vel_y);
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_POS_Z,
                ae_rd16(r, record + AE_OVL1A0_DEBRIS_POS_Z) + vel_z);
        ae_wr16(r, record + AE_OVL1A0_DEBRIS_VELOCITY_Y, vel_y + 3);

        uint32_t angles = AE_OVL1A0_GTE_DEBRIS_ANGLES;
        ae_wr16(r, angles + AE_OVL1A0_VECTOR16_X, (lifetime << 7));
        ae_wr16(r, angles + AE_OVL1A0_VECTOR16_Y, (lifetime << 7));
        ae_wr16(r, angles + AE_OVL1A0_VECTOR16_Z, (lifetime << 7));
        uint32_t matrix = AE_OVL1A0_GTE_SCRATCH_BASE;
        (void)ae_sdk_RotMatrixZYX(r, angles, matrix);
        ae_wr32(r, matrix + AE_OVL1A0_MATRIX_TRANSLATION_X,
                (uint32_t)ae_rds16(r, record + AE_OVL1A0_DEBRIS_POS_X));
        ae_wr32(r, matrix + AE_OVL1A0_MATRIX_TRANSLATION_Y,
                (uint32_t)ae_rds16(r, record + AE_OVL1A0_DEBRIS_POS_Y));
        ae_wr32(r, matrix + AE_OVL1A0_MATRIX_TRANSLATION_Z,
                (uint32_t)ae_rds16(r, record + AE_OVL1A0_DEBRIS_POS_Z));
        (void)ae_sdk_CompMatrix(r, camera_copy, matrix, matrix);
        const uint32_t set_matrix_args[1] = {matrix};
        (void)ovl1a0_render(r, h, "GTE_SetRotMatrix",
                            AE_OVL1A0_EXE_GTE_SET_ROT_MATRIX,
                            1, set_matrix_args);
        (void)ovl1a0_render(r, h, "GTE_SetTransMatrix",
                            AE_OVL1A0_EXE_GTE_SET_TRANS_MATRIX,
                            1, set_matrix_args);
        uint32_t quad = (remaining - 1u) & 3u;
        uint32_t vertex_address = matrix + quad_offsets[quad];
        const uint32_t project_args[4] = {
            matrix, vertex_address, AE_OVL1A0_DEBRIS_GTE_VERTEX_COUNT,
            lifetime
        };
        int32_t depth = ovl1a0_render(r, h, "Area46_Boss_DebrisProject",
                                      AE_OVL1A0_DEBRIS_RENDER_HOOK, 4,
                                      project_args);
        ae_wr32(r, AE_OVL1A0_GTE_SCRATCH_BASE + AE_OVL1A0_DEBRIS_GTE_DEPTH,
                (uint32_t)depth);
        if (ae_ram_failed(r)) return;
        if (depth > 0 && depth < 4096) {
            uint32_t display_record = AE_OVL1A0_OT_DISPLAY_RECORDS +
                ae_rd32(r, AE_OVL1A0_OT_SELECTOR) *
                AE_OVL1A0_MODEL_INSTANCE_STRIDE;
            const uint32_t args[7] = {
                ae_rd32(r, AE_OVL1A0_OT_CURSOR), display_record,
                ae_rd32(r, AE_OVL1A0_GTE_SCRATCH_BASE +
                           AE_OVL1A0_DEBRIS_GTE_SCREEN_X),
                ae_rd32(r, AE_OVL1A0_GTE_SCRATCH_BASE +
                           AE_OVL1A0_DEBRIS_GTE_SCREEN_Y),
                ae_rd32(r, AE_OVL1A0_GTE_SCRATCH_BASE +
                           AE_OVL1A0_DEBRIS_GTE_SCREEN_Z),
                0x32808080u,
                ae_rd32(r, AE_OVL1A0_GTE_SCRATCH_BASE +
                           AE_OVL1A0_DEBRIS_GTE_DEPTH)
            };
            (void)ovl1a0_render(r, h, "Area46_Boss_DebrisRender",
                                AE_OVL1A0_DEBRIS_RENDER_HOOK, 7, args);
            ae_wr32(r, AE_OVL1A0_OT_CURSOR,
                    ae_rd32(r, AE_OVL1A0_OT_CURSOR) + 28u);
        }
    }
    uint32_t display_record = AE_OVL1A0_OT_DISPLAY_RECORDS +
        ae_rd32(r, AE_OVL1A0_OT_SELECTOR) *
        AE_OVL1A0_MODEL_INSTANCE_STRIDE;
    const uint32_t finish_args[2] = {
        ae_rd32(r, AE_OVL1A0_OT_CURSOR), display_record
    };
    (void)ovl1a0_render(r, h, "Area46_Boss_DebrisFinalize",
                        AE_OVL1A0_DEBRIS_RENDER_HOOK, 2, finish_args);
    ae_wr32(r, AE_OVL1A0_OT_CURSOR,
            ae_rd32(r, AE_OVL1A0_OT_CURSOR) + 8u);
}

/* 8013DE1C/DE44/DE6C: named haptic requests remain platform callbacks. */
void ae_Area46_Boss_RumbleLong(AeRam *r, const AeOvl1A0Hooks *h) {
    ae_PadRumble(r, h ? h->camupd_hooks : NULL, 1, 200u, 10u);
}

void ae_Area46_Boss_RumbleShort(AeRam *r, const AeOvl1A0Hooks *h) {
    ae_PadRumble(r, h ? h->camupd_hooks : NULL, 1, 60u, 2u);
}

void ae_Area46_Boss_RumbleMedium(AeRam *r, const AeOvl1A0Hooks *h) {
    ae_PadRumble(r, h ? h->camupd_hooks : NULL, 1, 120u, 2u);
}

/* 8013DE94: select a pickup kind from difficulty, the LCG and boss group. */
void ae_Area46_Boss_SpawnDropPickups(AeRam *r, const AeOvl1A0Hooks *h,
                                     uint32_t position) {
    int32_t pickup_kind = 1;
    if (ae_rds16(r, AE_OVL1A0_PICKUP_DIFFICULTY) >= 3) {
        uint32_t seed = (ae_rd16(r, AE_OVL1A0_RAND_SEED) * 5u + 1u) &
                        0xFFFFu;
        ae_wr16(r, AE_OVL1A0_RAND_SEED, seed);
        if (((seed >> 8) & 1u) != 0u) {
            uint32_t group = AE_OVL1A0_AREA_ACTOR_MATCH_RECORD;
            pickup_kind = 6;
            if (ae_rd16(r, group + AE_OVL1A0_ACTOR_MATCH_FIELD) ==
                ae_rd8(r, AE_OVL1A0_DROP_MATCH_VALUE))
                pickup_kind = 3;
        }
    }
    ae_SpawnPickups(r, h ? h->zako2_hooks : NULL,
                    (uint32_t)pickup_kind, position, 3, 0);
}

/* 8013DF28: invalidate the boss's looping-sound handle. */
void ae_Area46_Boss_ResetLoopHandle(AeRam *r) {
    ae_wr32(r, AE_OVL1A0_BOSS_LOOP_HANDLE, UINT32_MAX);
}

/* 8013DF38: start the selected looping sound and remember its voice handle. */
int32_t ae_Area46_Boss_StartLoop(AeRam *r, const AeOvl1A0Hooks *h,
                                 int32_t sound_id) {
    int32_t handle = ae_SoundLoopStart(r, h ? h->sound_hw : NULL, 0,
                                       sound_id);
    ae_wr32(r, AE_OVL1A0_BOSS_LOOP_HANDLE, (uint32_t)handle);
    return handle;
}

/* 8013DF60: stop the remembered handle when one has been allocated. */
void ae_Area46_Boss_LoopStop(AeRam *r, const AeOvl1A0Hooks *h) {
    int32_t handle = ae_rds32(r, AE_OVL1A0_BOSS_LOOP_HANDLE);
    if (handle != -1) {
        (void)ae_SoundLoopStop(r, h ? h->sound_hw : NULL, 0, handle);
    }
}

/* 8013DF90: initialize the boss model set, collision bodies and work tables. */
void ae_Area46_BossInit(AeRam *r, const AeOvl1A0Hooks *h) {
    if (ae_ProgressGetByte(r, 8u) == 5u) {
        ae_wr8(r, AE_OVL1A0_BOSS_INIT_PROGRESS_FLAG, 1);
        return;
    }

    ae_wr16(r, AE_OVL1A0_BOSS_CLEAR_PROGRESS_HALFWORD, 0);
    /* The original initializes record zero once: its signed counter starts at
     * zero, increments, then the BLEZ loop test falls through. */
    for (uint32_t index = 0; index < 1u; ++index) {
        uint32_t model = ovl1a0_model_record(index);
        uint32_t body_fields = model + AE_OVL1A0_MODEL_FIELD_BODY_REGISTER;
        ae_Area46_Boss_InitializeModelEntry(
            r, h, index, model, model + AE_OVL1A0_MODEL_BODY_OUTPUT_POINTER, 1u);

        ae_wr8(r, model + AE_OVL1A0_MODEL_INIT_STATE, 1);
        ae_wr8(r, model + AE_OVL1A0_MODEL_INIT_FLAGS, 5);
        ae_wr8(r, model + AE_OVL1A0_MODEL_INIT_TYPE, 5);
        ae_wr32(r, body_fields, 1536);
        ae_wr32(r, body_fields + AE_OVL1A0_VECTOR32_Y, (uint32_t)-2560);
        ae_wr32(r, body_fields + AE_OVL1A0_VECTOR32_Z, (uint32_t)-6976);
        ae_wr16(r, model + AE_OVL1A0_MODEL_ROTATION_X, 0);
        ae_wr16(r, model + AE_OVL1A0_MODEL_ROTATION_Y, 0);
        ae_wr16(r, model + AE_OVL1A0_MODEL_ROTATION_Z, 4096);
        ae_Area46_Boss_SetAnim(r, model, 0);
        ae_wr32(r, model + AE_OVL1A0_MODEL_ANIMATION_POINTER, 0);
        ae_wr8(r, model + AE_OVL1A0_MODEL_BODY_STATE_A, 0);
        ae_wr8(r, model + AE_OVL1A0_MODEL_BODY_STATE_C, 0);
        ae_wr8(r, model + AE_OVL1A0_MODEL_BODY_STATE_B, 0);
        (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL,
                                       5, body_fields, model);

        ae_wr16(r, model + AE_OVL1A0_MODEL_COLLISION_RADIUS, (uint32_t)-15872);
        ae_wr16(r, model + AE_OVL1A0_MODEL_COLLISION_WIDTH, 450);
        ae_wr16(r, model + AE_OVL1A0_MODEL_COLLISION_ACTIVE, 1);
        ae_wr16(r, model + AE_OVL1A0_MODEL_COLLISION_HEIGHT, 360);
    }

    ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_STATUS, 1);
    ae_Area46_Boss_ClearFlag(r, 0);

    uint32_t registered_body = AE_OVL1A0_MODEL_RECORDS +
                               AE_OVL1A0_REGISTERED_BODY_OFFSET;
    uint32_t registered_owner = registered_body -
                                AE_OVL1A0_REGISTERED_OWNER_BACK_OFFSET;
    ae_wr32(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_FRAME, 0);
    ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_PICKUP_GUARD, 0);
    ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_PICKUP_GUARD_EXTRA, 0);
    ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_TRANSITION, 0);
    (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL, 5,
                                   registered_body, registered_owner);
    ae_wr16(r, registered_owner + AE_OVL1A0_MODEL_COLLISION_RADIUS, 0x4000u);
    ae_wr16(r, registered_owner + AE_OVL1A0_MODEL_COLLISION_WIDTH, 80);
    ae_wr16(r, registered_owner + AE_OVL1A0_MODEL_COLLISION_ACTIVE, 0);
    ae_wr16(r, registered_owner + AE_OVL1A0_MODEL_COLLISION_HEIGHT, 128);

    ae_Area46_Boss_ClearShockwaves(r, h, 1u);
    ae_Area46_Boss_InitializeActorRecords(r, h, 1u);
    ae_Area46_Boss_InitializeBackdrop(r, h, 1u);
    ae_Area46_Boss_ClearDebris(r);
    ae_Area46_Boss_ResetLoopHandle(r);
}

/* 8013E14C: create the eight boss actor model headers and clear their states. */
void ae_Area46_Boss_InitializeActorRecords(AeRam *r,
                                            const AeOvl1A0Hooks *h,
                                            uint32_t incoming_model_mode) {
    for (uint32_t index = 0; index < AE_OVL1A0_ACTOR_RECORD_COUNT; ++index) {
        uint32_t record = ovl1a0_actor_record(index);
        ae_Area46_Boss_InitializeModelEntry(
            r, h, 2u, record, record + AE_OVL1A0_ACTOR_RECORD_MODEL_HEADER,
            incoming_model_mode);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_STATE, 0);
    }
}

/* 8013E1AC: clear the active byte on each of the eight actor records. */
void ae_Area46_Boss_ClearEntries(AeRam *r) {
    for (uint32_t index = AE_OVL1A0_ACTOR_RECORD_COUNT; index != 0u; --index) {
        uint32_t record = ovl1a0_actor_record(index - 1u);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_STATE, 0);
    }
}

/* 8013E1D0: claim the first free actor record and seed its state fields. */
void ae_Area46_Boss_AddEntry(AeRam *r, uint32_t position, uint32_t type) {
    for (uint32_t index = 0; index < AE_OVL1A0_ACTOR_RECORD_COUNT; ++index) {
        uint32_t record = ovl1a0_actor_record(index);
        if (ae_rd8(r, record + AE_OVL1A0_ACTOR_RECORD_STATE) != 0u) continue;

        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_STATE, 1);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_INDEX, index << 5);
        ae_wr32(r, record + AE_OVL1A0_ACTOR_ENTRY_BODY_OBJECT, 0);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_FLAGS, 0);

        ae_wr32(r, record + AE_OVL1A0_ACTOR_POS_X,
                (uint32_t)ae_rds16(r, position));
        ae_wr32(r, record + AE_OVL1A0_ACTOR_POS_Y,
                (uint32_t)ae_rds16(r, position + AE_OVL1A0_VECTOR16_Y));
        ae_wr32(r, record + AE_OVL1A0_ACTOR_POS_Z,
                (uint32_t)ae_rds16(r, position + AE_OVL1A0_VECTOR16_Z));
        ae_wr16(r, record + AE_OVL1A0_ACTOR_RECORD_VALUE_A, 0);
        ae_wr16(r, record + AE_OVL1A0_ACTOR_RECORD_VALUE_B, 0);
        ae_wr16(r, record + AE_OVL1A0_ACTOR_RECORD_VALUE_C, 0);
        ae_wr16(r, record + AE_OVL1A0_ACTOR_RECORD_PHASE, UINT16_MAX);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_ANIM_ACTIVE, 0);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_ANIM_LOOP, 0);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_KIND, 2);
        ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_KNOWN_TYPE, type);

        if (type == 4u) {
            ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_EXTRA_BYTE_3, 0);
            uint32_t seed = ovl1a0_random16(r);
            ae_wr8(r, record + AE_OVL1A0_ACTOR_RECORD_RANDOM_SEED,
                   ((seed >> 8) & 0x0Fu) + 8u);
        }
        ae_wr16(r, record + AE_OVL1A0_ACTOR_RECORD_BODY_STATE, 0);
        return;
    }
}

/* 8013E2BC: update the eight active model records in array order. */
void ae_Area46_Boss_UpdateEntries(AeRam *r, const AeOvl1A0Hooks *h) {
    const uint32_t player = AE_OVL1A0_PLAYER_OBJECT;
    const uint32_t boss = AE_OVL1A0_MODEL_RECORDS;
    const uint32_t vector = AE_OVL1A0_ENTRY_VECTOR;
    const uint32_t direction = AE_OVL1A0_ENTRY_DIRECTION_VECTOR;
    const uint32_t scale = AE_OVL1A0_ENTRY_RENDER_SCALE;

    for (uint32_t index = 0; index < AE_OVL1A0_ACTOR_RECORD_COUNT; ++index) {
        uint32_t record = ovl1a0_actor_record(index);
        uint32_t active = ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE);
        if (active == 0u)
            continue;

        uint32_t countdown = ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_COUNTDOWN);
        if (countdown != 0u) {
            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_COUNTDOWN, countdown - 1u);
            continue;
        }

        uint32_t world_position = record + AE_OVL1A0_ACTOR_POS_X;
        ae_wr16(r, vector, ae_rd16(r, world_position));
        ae_wr16(r, vector + AE_OVL1A0_VECTOR16_Y,
                ae_rd16(r, world_position + AE_OVL1A0_VECTOR32_Y));
        ae_wr16(r, vector + AE_OVL1A0_VECTOR16_Z,
                ae_rd16(r, world_position + AE_OVL1A0_VECTOR32_Z));
        ae_wr32(r, scale, 6144u);
        ae_wr32(r, scale + AE_OVL1A0_VECTOR32_Y, 6144u);
        ae_wr32(r, scale + AE_OVL1A0_VECTOR32_Z, 6144u);

        int update_motion = 1;
        if (active == 1u) {
            ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 275, vector);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 24);
            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE, 2);
            (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL, 4, world_position, boss);
            ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_COLLISION_RADIUS, 512);
            ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_COLLISION_WIDTH, 96);
            ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_COLLISION_ACTIVE, 1);
            ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_COLLISION_HEIGHT, 64);
            (void)ae_TargetListAdd(r, record + AE_OVL1A0_ACTOR_RENDER_PARAMS);
            update_motion = 0;
        } else if (active == 3u) {
            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE, 0);
            (void)ae_CollisionBodyUnregister(r, h ? h->body_hooks : NULL, world_position);
            ae_TargetListRemove(r, record + AE_OVL1A0_ACTOR_RENDER_PARAMS);
            update_motion = 0;
        }

        uint32_t tick = ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_TICK);
        if (update_motion) {
            int32_t pos_x = ae_rds32(r, world_position);
            int32_t pos_y = ae_rds32(r, world_position + AE_OVL1A0_VECTOR32_Y);
            int32_t pos_z = ae_rds32(r, world_position + AE_OVL1A0_VECTOR32_Z);
            int32_t vel_x = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X);
            int32_t vel_y = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Y);
            int32_t vel_z = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z);
            pos_x = (int32_t)((uint32_t)pos_x + (uint32_t)vel_x);
            pos_y = (int32_t)((uint32_t)pos_y + (uint32_t)vel_y);
            pos_z = (int32_t)((uint32_t)pos_z + (uint32_t)vel_z);
            ae_wr32(r, world_position, (uint32_t)pos_x);
            ae_wr32(r, world_position + AE_OVL1A0_VECTOR32_Y, (uint32_t)pos_y);
            ae_wr32(r, world_position + AE_OVL1A0_VECTOR32_Z, (uint32_t)pos_z);

            if ((tick & 1u) != 0u)
                ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 9, vector);

            if (pos_z >= -3135 || ae_rd16(r, record + AE_OVL1A0_ACTOR_ENTRY_TURN) < tick) {
                ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 258, vector);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 77);
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_TICK, 0);
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE, AE_OVL1A0_ACTOR_ENTRY_RETREAT);
            }

            int activate_spin = 0;
            if (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE) == 2u &&
                (ae_rd32(r, record + AE_OVL1A0_ACTOR_ENTRY_CONTACT) >> 28) == 11u) {
                uint32_t cooldown =
                    (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_COOLDOWN) - 1u) & 0xFFu;
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_COOLDOWN, cooldown);
                if (cooldown == 0u) {
                    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 258, vector);
                    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 77);
                    ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_TICK, 0);
                    ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE, AE_OVL1A0_ACTOR_ENTRY_RETREAT);
                    if (ae_rd8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_PICKUP_GUARD) == 0u) {
                        if ((ovl1a0_random16(r) & 0x100u) == 0u)
                            ae_Area46_Boss_SpawnDropPickups(r, h, vector);
                        ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_PICKUP_GUARD, 1);
                    }
                } else {
                    activate_spin = 1;
                }
            }

            if (activate_spin != 0) {
                ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 275, vector);
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE, AE_OVL1A0_ACTOR_ENTRY_SPIN);
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_SPIN_TIMER, 32);
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Y, 0);
                int32_t delta_x = ae_rds16(r, world_position) - ae_rds16(r, player + AE_OBJ_POS_X);
                int32_t delta_z = ae_rds16(r, world_position + AE_OVL1A0_VECTOR32_Z) -
                                  ae_rds16(r, player + AE_OBJ_POS_Z);
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X, (uint32_t)delta_x);
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z, (uint32_t)delta_z);
                (void)ae_sdk_VectorNormalSS(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X,
                                            record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X);
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X,
                        ovl1a0_asr(ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X), 5u));
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z,
                        ovl1a0_asr(ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z), 5u));
                uint32_t seed = ovl1a0_random16(r);
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE, (seed >> 8) & 1u);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 103);
            }

            if (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE) == AE_OVL1A0_ACTOR_ENTRY_SPIN) {
                int32_t spin_velocity_x = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X);
                int32_t spin_velocity_z = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z);
                spin_velocity_x = ovl1a0_div_toward_zero(spin_velocity_x * 3, 4);
                spin_velocity_z = ovl1a0_div_toward_zero(spin_velocity_z * 3, 4);
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X, (uint32_t)spin_velocity_x);
                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z, (uint32_t)spin_velocity_z);

                uint32_t spin_timer =
                    (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_SPIN_TIMER) - 1u) & 0xFFu;
                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_SPIN_TIMER, spin_timer);
                if (spin_timer == 0u)
                    ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE,
                           AE_OVL1A0_ACTOR_ENTRY_ACTIVE_MOVING);
                if ((tick & 3u) == 0u)
                    ae_SpawnRandomParticle(r, record + AE_OVL1A0_ACTOR_ENTRY_RENDERED_X, 1u, 0u,
                                           32);
            }

            int32_t distance_squared = 0;
            uint32_t kind = ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE);
            if (kind < 5u) {
                int32_t to_player_x =
                    ae_rds16(r, player + AE_OBJ_POS_X) - ae_rds16(r, world_position);
                int32_t to_player_y = ae_rds16(r, player + AE_OBJ_POS_Y) -
                                      (ae_rds16(r, world_position + AE_OVL1A0_VECTOR32_Y) + 128);
                int32_t to_player_z = ae_rds16(r, player + AE_OBJ_POS_Z) -
                                      ae_rds16(r, world_position + AE_OVL1A0_VECTOR32_Z);
                ae_wr16(r, direction, (uint32_t)to_player_x);
                ae_wr16(r, direction + AE_OVL1A0_VECTOR16_Y, (uint32_t)to_player_y);
                ae_wr16(r, direction + AE_OVL1A0_VECTOR16_Z, (uint32_t)to_player_z);
                distance_squared = ae_sdk_VectorNormalSS(r, direction, direction);

                if (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_SPIN_TIMER) == 0u) {
                    int32_t vel_x = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X);
                    int32_t vel_z = ae_rds16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z);
                    uint32_t turn = ae_rd16(r, record + AE_OVL1A0_ACTOR_ENTRY_TURN);
                    if (kind == AE_OVL1A0_ACTOR_ENTRY_STATIONARY) {
                        if (distance_squared <= 0x3FFFF && turn == UINT16_MAX)
                            ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_TURN, 240);
                        vel_x =
                            ovl1a0_asr(vel_x * 31, 5u) + ovl1a0_asr(ae_rds16(r, direction), 10u);
                        vel_z = ovl1a0_asr(vel_z * 31, 5u) +
                                ovl1a0_asr(ae_rds16(r, direction + AE_OVL1A0_VECTOR16_Z), 10u);
                        vel_x = ovl1a0_clamp(vel_x, -28, 28);
                        vel_z = ovl1a0_clamp(vel_z, -28, 28);
                        if ((ovl1a0_random16(r) & 0x1F00u) == 0u)
                            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 151);
                    } else if (kind == AE_OVL1A0_ACTOR_ENTRY_APPROACH) {
                        vel_x = ovl1a0_asr(ae_rds16(r, direction), 9u);
                        vel_z = ovl1a0_asr(ae_rds16(r, direction + AE_OVL1A0_VECTOR16_Z), 9u);
                        uint32_t state_tick =
                            (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK) + 1u) & 0xFFu;
                        ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK, state_tick);
                        if (state_tick >= 17u) {
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK, 0);
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE,
                                   AE_OVL1A0_ACTOR_ENTRY_JUMP);
                            vel_x = ovl1a0_asr(ae_rds16(r, direction), 6u);
                            vel_z = ovl1a0_asr(ae_rds16(r, direction + AE_OVL1A0_VECTOR16_Z), 6u);
                            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 151);
                        }
                    } else if (kind == AE_OVL1A0_ACTOR_ENTRY_JUMP) {
                        if (distance_squared <= 0x3FFFF && turn == UINT16_MAX)
                            ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_TURN, 240);
                        uint32_t state_tick =
                            (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK) + 1u) & 0xFFu;
                        ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK, state_tick);
                        if (state_tick >= 31u || distance_squared <= 0x9C3F) {
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK, 10);
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE,
                                   AE_OVL1A0_ACTOR_ENTRY_RETREAT);
                        }
                    } else if (kind == AE_OVL1A0_ACTOR_ENTRY_RETREAT) {
                        vel_x = ovl1a0_div_toward_zero(vel_x * 15, 16);
                        vel_z = ovl1a0_div_toward_zero(vel_z * 15, 16);
                        uint32_t state_tick =
                            (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK) - 1u) & 0xFFu;
                        ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE_TICK, state_tick);
                        if (state_tick == 0u)
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_STATE,
                                   AE_OVL1A0_ACTOR_ENTRY_APPROACH);
                    } else {
                        uint32_t effect_timer =
                            ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_TIMER);
                        uint32_t effect_delay =
                            ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_DELAY);
                        if (effect_timer == 0u && effect_delay != 0u) {
                            --effect_delay;
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_DELAY, effect_delay);
                            if (effect_delay == 0u) {
                                uint32_t first_random = ovl1a0_random16(r);
                                uint32_t second_random = ovl1a0_random16(r);
                                int32_t random_velocity_x =
                                    ((first_random >> 8) & 2u) != 0u ? 1 : -1;
                                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_TIMER,
                                       ((second_random >> 8) & 0xFu) + 20u);
                                ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X,
                                        (uint32_t)random_velocity_x);
                                vel_x = random_velocity_x;
                                uint32_t delay_random = ovl1a0_random16(r);
                                ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_DELAY,
                                       ((delay_random >> 8) & 0xFu) + 8u);
                            }
                        }

                        effect_timer = ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_TIMER);
                        if (effect_timer < 5u) {
                            vel_x = ovl1a0_div_toward_zero(vel_x * 3, 4);
                            vel_z += 3;
                        }
                        if (effect_timer != 0u) {
                            effect_timer = (effect_timer - 1u) & 0xFFu;
                            ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_EFFECT_TIMER, effect_timer);
                            if (effect_timer != 0u) {
                                vel_x += vel_x < 0 ? -3 : 3;
                                vel_z = ovl1a0_div_toward_zero(vel_z * 3, 4);
                            }
                        }
                        vel_x = ovl1a0_clamp(vel_x, -32, 32);
                        vel_z = ovl1a0_clamp(vel_z, -64, 64);
                        pos_x = ovl1a0_clamp(pos_x, -384, 3456);
                        ae_wr32(r, world_position, (uint32_t)pos_x);
                    }
                    ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X, (uint32_t)vel_x);
                    ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_Z, (uint32_t)vel_z);
                }
            }
        }

        ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_RENDERED_X, ae_rd16(r, world_position));
        ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_RENDERED_Y,
                ae_rd16(r, world_position + AE_OVL1A0_VECTOR32_Y));
        ae_wr16(r, record + AE_OVL1A0_ACTOR_ENTRY_RENDERED_Z,
                ae_rd16(r, world_position + AE_OVL1A0_VECTOR32_Z));
        tick = (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_TICK) + 1u) & 0xFFu;
        ae_wr8(r, record + AE_OVL1A0_ACTOR_ENTRY_TICK, tick);

        if (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE) == 2u) {
            int32_t dx = ae_rds32(r, player + AE_OBJ_POS_X) - ae_rds32(r, world_position);
            int32_t dz = ae_rds32(r, player + AE_OBJ_POS_Z) -
                         ae_rds32(r, world_position + AE_OVL1A0_VECTOR32_Z);
            if (dx > -512 && dx < 512 && dz > -512 && dz < 512)
                ae_Area46_Boss_SpawnEffectNearWrap(r, h, record + AE_OVL1A0_ACTOR_ENTRY_BODY_OBJECT,
                                                   boss, 80, 512);
        }

        uint32_t animation = ae_rd32(r, record + AE_OVL1A0_ACTOR_RECORD_MODEL_HEADER);
        uint32_t frame = (ae_rd16(r, record + AE_OVL1A0_ACTOR_FRAME) + 1u) & 0xFFFFu;
        ae_wr16(r, record + AE_OVL1A0_ACTOR_FRAME, frame);
        if (ae_rd16(r, animation + AE_OVL1A0_ANIMATION_CLIP_END) < frame)
            ae_wr16(r, record + AE_OVL1A0_ACTOR_FRAME, ae_rd16(r, animation));

        uint32_t render_mode = 0;
        if (ae_rd8(r, record + AE_OVL1A0_ACTOR_ENTRY_ACTIVE) == AE_OVL1A0_ACTOR_ENTRY_SPIN) {
            if ((tick & 1u) != 0u) {
                render_mode = AE_OVL1A0_ACTOR_ENTRY_SPIN_RENDER_MODE;
            }
        } else {
            (void)ae_sdk_VectorNormalSS(r, record + AE_OVL1A0_ACTOR_ENTRY_VELOCITY_X,
                                        record + AE_OVL1A0_ACTOR_ENTRY_NORMAL_VECTOR);
        }
        (void)ae_Area46_Boss_ProjectAndEmit(r, h, record, scale, render_mode);
    }
}

/* 8013EC5C: initialize the backdrop model and clear its active counter. */
void ae_Area46_Boss_InitializeBackdrop(AeRam *r,
                                       const AeOvl1A0Hooks *h,
                                       uint32_t incoming_model_mode) {
    ae_Area46_Boss_InitializeModelEntry(
        r, h, 3u, AE_OVL1A0_BACKDROP,
        AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_POINTER,
        incoming_model_mode);
    ae_wr8(r, AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_ACTIVE, 0);
}

/* 8013EC94: set the backdrop lifetime, origin and initial angle. */
void ae_Area46_Boss_SetBackdrop(AeRam *r, int32_t x, int32_t y, int32_t z) {
    ae_wr8(r, AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_ACTIVE, 20);
    ae_wr32(r, AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_POSITION_X,
            (uint32_t)x);
    ae_wr32(r, AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_POSITION_Y,
            (uint32_t)y);
    ae_wr32(r, AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_POSITION_Z,
            (uint32_t)z);
    ae_wr16(r, AE_OVL1A0_BACKDROP + AE_OVL1A0_BACKDROP_BODY_ANGLE,
            (uint32_t)-90);
}

/* 8013EE74: reset the boss animation and its short state timers. */
void ae_Area46_Boss_ResetBoss(AeRam *r) {
    ae_Area46_Boss_ClearFlag(r, 1);
    ae_Area46_Boss_SetAnim(r, AE_OVL1A0_MODEL_RECORDS, 0);
    ae_wr32(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_TIMER, 30);
    ae_wr32(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_FRAME, 0);
}

/* 8013EEB8: begin the boss intro and start its requested looping sound. */
void ae_Area46_Boss_OnBossStart(AeRam *r, const AeOvl1A0Hooks *h) {
    ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_STATUS, 0);
    ae_Area46_Boss_ResetBoss(r);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 0xB1);
    ae_wr32(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_LOOP_STATE, 0);
    ae_wr32(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_TIMER, 90);
    ae_Area46_Boss_StartLoop(r, h, 0xB1);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 4);
}

/* 8013EF00: set the boss's event flag byte. */
void ae_Area46_Boss_ClearFlag(AeRam *r, uint32_t value) {
    ae_wr8(r, AE_OVL1A0_BOSS_STATE + AE_OVL1A0_BOSS_EVENT_FLAG, value);
}

/* 8013EF0C: select an animation only when it differs from the current one. */
void ae_Area46_Boss_SetAnim(AeRam *r, uint32_t model, uint32_t animation) {
    if (ae_rd16(r, model + AE_OVL1A0_ACTOR_ANIM) == animation) return;
    uint32_t clips = ae_rd32(r, model + AE_OVL1A0_ACTOR_RENDER_PARAMS);
    uint32_t clip = clips + animation * AE_OVL1A0_MODEL_POINTER_SLOT_SIZE;
    ae_wr16(r, model + AE_OVL1A0_ACTOR_ANIM, animation);
    ae_wr8(r, model + AE_OVL1A0_ACTOR_ANIM_ACTIVE, 1);
    ae_wr8(r, model + AE_OVL1A0_ACTOR_ANIM_LOOP, 0);
    ae_wr16(r, model + AE_OVL1A0_ACTOR_FRAME, ae_rd16(r, clip));
}

/* 8013EF44: advance one frame and apply the clip's end/loop rule. */
void ae_Area46_Boss_StepAnim(AeRam *r, uint32_t model) {
    uint32_t clips = ae_rd32(r, model + AE_OVL1A0_ACTOR_RENDER_PARAMS);
    uint32_t clip = clips + ae_rd16(r, model + AE_OVL1A0_ACTOR_ANIM) *
                            AE_OVL1A0_MODEL_POINTER_SLOT_SIZE;
    uint32_t frame = (ae_rd16(r, model + AE_OVL1A0_ACTOR_FRAME) + 1u) &
                     0xFFFFu;
    ae_wr16(r, model + AE_OVL1A0_ACTOR_FRAME, frame);
    uint32_t clip_end = ae_rd16(r, clip + AE_OVL1A0_ANIMATION_CLIP_END);
    if (frame == clip_end)
        ae_wr8(r, model + AE_OVL1A0_ACTOR_ANIM_ACTIVE, 0);

    if (frame > clip_end) {
        if (ae_rd8(r, model + AE_OVL1A0_ACTOR_ANIM_LOOP) == 1u)
            frame = (frame - 1u) & 0xFFFFu;
        else
            frame = ae_rd16(r, clip);
        ae_wr16(r, model + AE_OVL1A0_ACTOR_FRAME, frame);
    }
}

/* 8013EFCC: update the hop arc and choose the actor's render-layer phase. */
void ae_Area46_Boss_UpdateHop(AeRam *r, const AeOvl1A0Hooks *h,
                              uint32_t actor) {
    int32_t hop_timer = ae_rds8(r, actor + AE_OVL1A0_ACTOR_HOP_TIMER);
    if (hop_timer != 0) {
        uint32_t phase = ae_rd8(r, actor + AE_OVL1A0_ACTOR_HOP_PHASE);
        uint32_t next_timer = ((uint32_t)hop_timer + phase) & 0xFFu;
        ae_wr32(r, actor + AE_OVL1A0_ACTOR_POS_Z,
                ae_rd32(r, actor + AE_OVL1A0_ACTOR_POS_Z) -
                (uint32_t)hop_timer);
        ae_wr8(r, actor + AE_OVL1A0_ACTOR_HOP_TIMER, next_timer);
        ae_wr8(r, actor + AE_OVL1A0_ACTOR_HOP_PHASE, phase + 8u);
        if (ae_rds8(r, actor + AE_OVL1A0_ACTOR_HOP_TIMER) > 0)
            ae_wr8(r, actor + AE_OVL1A0_ACTOR_HOP_TIMER, 0);
    }

    uint32_t direction = ae_rd8(r, actor + AE_OVL1A0_ACTOR_PHASE);
    uint32_t render_angle;
    if (direction == 0u) {
        render_angle = (ae_rd32(r, AE_OVL1A0_OT_TABLE) & 1u) * 0xF0u;
    } else if (direction == 2u) {
        uint32_t turn = ae_rd32(r, AE_OVL1A0_HOP_RENDER_PHASE) & 0xFu;
        if (turn >= 8u) turn = 15u - turn;
        render_angle = turn << 4;
    } else {
        uint32_t turn = ae_rd32(r, AE_OVL1A0_HOP_RENDER_PHASE) & 7u;
        if (turn >= 4u) turn = 7u - turn;
        render_angle = turn << 5;
    }

    const uint32_t matrix = AE_OVL1A0_HOP_RENDER_MATRIX;
    const uint32_t args[3] = {actor, matrix, 0x10000u | render_angle};
    (void)ae_Area46_Boss_ProjectAndEmit(r, h, args[0], args[1], args[2]);
    uint32_t turn_counter = ae_rd32(r, AE_OVL1A0_HOP_RENDER_PHASE);
    ae_wr32(r, AE_OVL1A0_HOP_RENDER_PHASE, (turn_counter + 1u) & 0xFFu);
}

/* 8013F0E0: move the player away from the currently stored boss impact. */
void ae_Area46_Boss_UpdatePlayerKnockback(AeRam *r,
                                          const AeOvl1A0Hooks *h) {
    uint32_t state = AE_OVL1A0_BOSS_STATE;
    uint32_t player = AE_OVL1A0_PLAYER_OBJECT;
    uint32_t remaining = ae_rd16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_TICKS);
    if (remaining == 0u) return;

    ae_wr16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_TICKS, remaining - 1u);
    ae_wr32(r, player + AE_OVL1A0_PLAYER_VELOCITY_X,
            (uint32_t)ae_rds16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_X));
    ae_wr32(r, player + AE_OVL1A0_PLAYER_VELOCITY_Z,
            (uint32_t)ae_rds16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_Z));

    int32_t position_z = ae_rds32(r, player + AE_OBJ_POS_Z);
    if (position_z >= -3135) position_z = -3136;
    if (position_z < -8100) position_z = -8120;
    ae_wr32(r, player + AE_OBJ_POS_Z, (uint32_t)position_z);

    int32_t velocity_x = ovl1a0_div_toward_zero(
        ae_rds16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_X) * 7, 8);
    int32_t velocity_z = ovl1a0_div_toward_zero(
        ae_rds16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_Z) * 7, 8);
    ae_wr16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_X,
            (uint32_t)velocity_x);
    ae_wr16(r, state + AE_OVL1A0_PLAYER_KNOCKBACK_VELOCITY_Z,
            (uint32_t)velocity_z);

    const uint32_t effect_pos = AE_OVL1A0_KNOCKBACK_EFFECT_POSITION;
    ae_SpawnEffectGroup(r, h ? h->spawn_hooks : NULL, 206,
                        effect_pos, AE_OVL1A0_EFFECT_GROUP_ASSET, 0);
}

/* 8013ECD4: animate, rotate, draw, and age the fading backdrop model. */
void ae_Area46_Boss_UpdateBackdrop(AeRam *r, const AeOvl1A0Hooks *h) {
    uint32_t backdrop = AE_OVL1A0_BACKDROP;
    uint32_t matrix = AE_OVL1A0_STACK_MATRIX;
    for (uint32_t word = 0; word < 4u; ++word)
        ae_wr32(r, matrix + word * 4u,
                ae_rd32(r, AE_OVL1A0_BACKDROP_MATRIX_TEMPLATE + word * 4u));

    uint32_t timer = ae_rd8(r, backdrop + AE_OVL1A0_BACKDROP_EVENT_TIMER);
    if (timer == 0u) return;

    uint32_t frame = (ae_rd16(r, backdrop + AE_OVL1A0_BACKDROP_ANIM_TIMER) +
                      1u) & 0xFFFFu;
    ae_wr16(r, backdrop + AE_OVL1A0_BACKDROP_ANIM_TIMER, frame);
    uint32_t anim = ae_rd32(r, backdrop + AE_OVL1A0_BACKDROP_ANIM_DESCRIPTOR);
    if (frame > ae_rd16(r, anim + AE_OVL1A0_ANIMATION_CLIP_END))
        ae_wr16(r, backdrop + AE_OVL1A0_BACKDROP_ANIM_TIMER,
                ae_rd16(r, anim));

    int32_t angle = (int32_t)(timer << 9);
    int32_t reduced_angle = angle - ovl1a0_asr(angle +
        (angle < 0 ? 4095 : 0), 12u) * 4096;
    ae_wr16(r, backdrop + AE_OVL1A0_BACKDROP_ROT_COS,
            ae_sdk_rcos(r, reduced_angle));
    ae_wr16(r, backdrop + AE_OVL1A0_BACKDROP_ROT_ZERO, 0);
    ae_wr16(r, backdrop + AE_OVL1A0_BACKDROP_ROT_SIN,
            ae_sdk_rsin(r, reduced_angle));
    (void)ae_Area46_Boss_ProjectAndEmit(r, h, backdrop, matrix, 0);

    const uint32_t clear_args[3] = {AE_OVL1A0_STACK_EFFECT_POINT, 0, 8};
    (void)ovl1a0_call(r, h, AE_OVL1A0_MEMSET_NAME,
                      AE_OVL1A0_MEMSET_HOOK, 3, clear_args);
    const uint32_t particle_pos = AE_OVL1A0_STACK_PARTICLE_POSITION;
    const uint32_t point = AE_OVL1A0_STACK_EFFECT_POINT;
    ae_wr16(r, point, ae_rd16(r, backdrop + AE_OVL1A0_BACKDROP_POSITION_X));
    ae_wr16(r, point + AE_OVL1A0_VECTOR16_Y,
            ae_rd16(r, backdrop + AE_OVL1A0_BACKDROP_POSITION_Y) - 64u);
    ae_wr16(r, point + AE_OVL1A0_VECTOR16_Z,
            ae_rd16(r, backdrop + AE_OVL1A0_BACKDROP_POSITION_Z));
    ae_wr16(r, particle_pos, ae_rd16(r, point));
    ae_wr16(r, particle_pos + AE_OVL1A0_VECTOR16_Y,
            ae_rd16(r, point + AE_OVL1A0_VECTOR16_Y));
    ae_wr16(r, particle_pos + AE_OVL1A0_VECTOR16_Z,
            ae_rd16(r, point + AE_OVL1A0_VECTOR16_Z));
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 5, particle_pos);
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 5, particle_pos);
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 40, particle_pos);
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 48, particle_pos);

    int32_t y = ae_rds32(r, backdrop + AE_OVL1A0_BACKDROP_POSITION_Y);
    ae_wr32(r, backdrop + AE_OVL1A0_BACKDROP_POSITION_Y,
            (uint32_t)(y + ae_rds16(r, backdrop +
                                    AE_OVL1A0_BACKDROP_ANGLE_STEP)));
    ae_wr16(r, backdrop + AE_OVL1A0_BACKDROP_ANGLE_STEP,
            ae_rd16(r, backdrop + AE_OVL1A0_BACKDROP_ANGLE_STEP) + 2u);
    ae_wr8(r, backdrop + AE_OVL1A0_BACKDROP_EVENT_TIMER, timer - 1u);
}

/* 8013F544: rebuild each shockwave model and clear its activity byte. */
void ae_Area46_Boss_ClearShockwaves(AeRam *r, const AeOvl1A0Hooks *h,
                                    uint32_t incoming_model_mode) {
    for (uint32_t index = 0; index < AE_OVL1A0_SHOCKWAVE_COUNT; ++index) {
        uint32_t record = ovl1a0_shockwave_record(index);
        ae_Area46_Boss_InitializeModelEntry(
            r, h, 1u, record, record + AE_OVL1A0_ACTOR_RECORD_MODEL_HEADER,
            incoming_model_mode);
        ae_wr8(r, record + AE_OVL1A0_SHOCK_ACTIVE, 0);
    }
}

/* 8013F5A8: allocate the first free shockwave and emit its eight sparks. */
void ae_Area46_Boss_AddShockwave(AeRam *r, const AeOvl1A0Hooks *h,
                                 uint32_t position) {
    uint32_t index;
    for (index = 0; index < AE_OVL1A0_SHOCKWAVE_COUNT; ++index) {
        uint32_t record = ovl1a0_shockwave_record(index);
        if (ae_rd8(r, record + AE_OVL1A0_SHOCK_ACTIVE) == 0u) break;
    }
    if (index == AE_OVL1A0_SHOCKWAVE_COUNT) return;

    uint32_t record = ovl1a0_shockwave_record(index);
    uint32_t x = (uint32_t)ae_rds16(r, position);
    uint32_t y = (uint32_t)ae_rds16(r, position + AE_OVL1A0_VECTOR16_Y);
    uint32_t z = (uint32_t)ae_rds16(r, position + AE_OVL1A0_VECTOR16_Z);
    ae_wr8(r, record + AE_OVL1A0_SHOCK_ACTIVE, 1);
    ae_wr32(r, record + AE_OVL1A0_SHOCK_BODY_POS_X, x);
    ae_wr32(r, record + AE_OVL1A0_SHOCK_BODY_POS_Y, y);
    ae_wr32(r, record + AE_OVL1A0_SHOCK_BODY_POS_Z, z);
    uint32_t random = ovl1a0_random16(r);
    int32_t player_dx = ae_rds32(r, AE_OVL1A0_PLAYER_OBJECT + AE_OVL1A0_PLAYER_X) -
                        ae_rds16(r, position);
    int32_t jitter = (int32_t)((random >> 8) & 0x1Fu);
    int32_t toward_player = ovl1a0_div_toward_zero(player_dx, 24);
    ae_wr16(r, record + AE_OVL1A0_SHOCK_TIMER,
            (uint32_t)(toward_player + jitter - 15));
    ae_wr16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Y, (uint32_t)-16);
    random = ovl1a0_random16(r);
    ae_wr16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Z,
            ((random >> 8) & 0xFu) + 30u);

    ae_SpawnEffectGroup(r, h ? h->spawn_hooks : NULL, 202, position,
                        AE_OVL1A0_EFFECT_GROUP_ASSET, 0);
    for (uint32_t spark = 0; spark < 8u; ++spark)
        ae_SpawnEffectGroup(r, h ? h->spawn_hooks : NULL, 203, position,
                            AE_OVL1A0_EFFECT_GROUP_ASSET, 0);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 75);
}

/* 8013F6F8: advance active shockwaves and emit the impact at the floor. */
void ae_Area46_Boss_UpdateShockwaves(AeRam *r, const AeOvl1A0Hooks *h) {
    for (uint32_t index = 0; index < AE_OVL1A0_SHOCKWAVE_COUNT; ++index) {
        uint32_t record = ovl1a0_shockwave_record(index);
        if (ae_rd8(r, record + AE_OVL1A0_SHOCK_ACTIVE) == 0u) continue;
        int32_t pos_x = ae_rds32(r, record + AE_OVL1A0_SHOCK_BODY_POS_X);
        int32_t pos_y = ae_rds32(r, record + AE_OVL1A0_SHOCK_BODY_POS_Y);
        int32_t pos_z = ae_rds32(r, record + AE_OVL1A0_SHOCK_BODY_POS_Z);
        int32_t player_z = ae_rds32(r, AE_OVL1A0_PLAYER_OBJECT + AE_OBJ_POS_Z);

        if (pos_z >= -6423 || player_z - 1024 < pos_z) {
            uint32_t random = ovl1a0_random16(r);
            ae_wr16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Y,
                    ae_rd16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Y) +
                    ((random >> 8) & 3u) + 5u);
            random = ovl1a0_random16(r);
            ae_wr16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Z,
                    ae_rd16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Z) +
                    ((random >> 8) & 1u) + 1u);
        }

        int32_t vel_x = ae_rds16(r, record + AE_OVL1A0_SHOCK_TIMER);
        int32_t vel_y = ae_rds16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Y);
        int32_t vel_z = ae_rds16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Z);
        int32_t dx = ae_rds32(r, AE_OVL1A0_PLAYER_OBJECT + AE_OVL1A0_PLAYER_X) -
                     pos_x;
        if (dx < 0) dx += 63;
        pos_x = (int32_t)((uint32_t)pos_x + (uint32_t)vel_x +
                          (uint32_t)ovl1a0_asr(dx, 6u));
        pos_y = (int32_t)((uint32_t)pos_y + (uint32_t)vel_y);
        pos_z = (int32_t)((uint32_t)pos_z + (uint32_t)vel_z);
        ae_wr32(r, record + AE_OVL1A0_SHOCK_BODY_POS_X, (uint32_t)pos_x);
        ae_wr32(r, record + AE_OVL1A0_SHOCK_BODY_POS_Y, (uint32_t)pos_y);
        ae_wr32(r, record + AE_OVL1A0_SHOCK_BODY_POS_Z, (uint32_t)pos_z);
        ae_wr16(r, record + AE_OVL1A0_SHOCK_RENDERED_X, (uint32_t)pos_x);
        ae_wr16(r, record + AE_OVL1A0_SHOCK_RENDERED_Y, (uint32_t)pos_y);
        ae_wr16(r, record + AE_OVL1A0_SHOCK_RENDERED_Z, (uint32_t)pos_z);
        ae_wr16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Y, vel_y + 1);
        ae_wr16(r, record + AE_OVL1A0_SHOCK_VELOCITY_Z, vel_z + 3);

        if (pos_y >= -2592) {
            uint32_t particle_pos = AE_OVL1A0_STACK_PARTICLE_POSITION;
            ae_wr16(r, particle_pos + AE_OVL1A0_VECTOR16_X, (uint32_t)pos_x);
            ae_wr16(r, particle_pos + AE_OVL1A0_VECTOR16_Y, (uint32_t)-2624);
            ae_wr16(r, particle_pos + AE_OVL1A0_VECTOR16_Z, (uint32_t)pos_z);
            ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 258, particle_pos);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 77);
            ae_CameraShake(r, 0);
            ae_wr8(r, record + AE_OVL1A0_SHOCK_ACTIVE, 0);
        }

        uint32_t effect_pos = AE_OVL1A0_STACK_EFFECT_POINT;
        ae_wr16(r, effect_pos + AE_OVL1A0_VECTOR16_X, (uint32_t)pos_x);
        ae_wr16(r, effect_pos + AE_OVL1A0_VECTOR16_Y, (uint32_t)pos_y);
        ae_wr16(r, effect_pos + AE_OVL1A0_VECTOR16_Z,
                (uint32_t)(pos_z + 32));
        ae_SpawnEffectGroup(r, h ? h->spawn_hooks : NULL, 204,
                            effect_pos, AE_OVL1A0_EFFECT_GROUP_ASSET, 0);
        ae_Area46_Boss_SpawnEffectNearWrap(
            r, h, record + AE_OVL1A0_SHOCK_RENDER_MODEL,
            AE_OVL1A0_MODEL_RECORDS, 80, 512);
        (void)ae_sdk_VectorNormalSS(r,
                                    record + AE_OVL1A0_SHOCK_DIRECTION_OUTPUT,
                                    record + AE_OVL1A0_SHOCK_DIRECTION);
        uint32_t draw_matrix = AE_OVL1A0_STACK_MATRIX;
        for (uint32_t word = 0; word < 4u; ++word)
            ae_wr32(r, draw_matrix + word * 4u,
                    ae_rd32(r, AE_OVL1A0_SHOCKWAVE_MATRIX_TEMPLATE + word * 4u));
        (void)ae_Area46_Boss_ProjectAndEmit(r, h, record, draw_matrix, 0);
        ae_wr16(r, record + AE_OVL1A0_SHOCK_ANIMATION_STATE, 0);
    }
}

/* 8013F1AC: update the boss encounter camera and choose its phase framing. */
int32_t ae_Area46_BossUpdate(AeRam *r, const AeOvl1A0Hooks *h,
                             uint32_t subject) {
    uint32_t state = AE_OVL1A0_BOSS_STATE;
    uint32_t boss = AE_OVL1A0_MODEL_RECORDS;
    uint32_t player = AE_OVL1A0_PLAYER_OBJECT;
    uint32_t scratch = AE_OVL1A0_GTE_SCRATCH_BASE;

    if (ae_rd8(r, state + AE_OVL1A0_BOSS_STATUS) != 0u) return 0;
    if (ae_rd8(r, boss + AE_OVL1A0_ACTOR_ENTRY_ACTIVE) == 0u) {
        if (ae_rd8(r, state + AE_OVL1A0_BOSS_TRANSITION) != 0u) return 0;
        if (ae_rds32(r, AE_OVL1A0_PLAYER_STATE) < -384) {
            ae_wr8(r, state + AE_OVL1A0_BOSS_TRANSITION, 1);
            ae_CameraSlotSnapshotEye(r, h ? h->camera_hooks : NULL);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 4);
        }
    }
    if (ae_rd8(r, state + AE_OVL1A0_BOSS_TRANSITION) != 0u) return 0;

    int32_t player_x = ae_rds16(r, player + AE_OBJ_POS_X);
    int32_t player_y = ae_rds16(r, player + AE_OBJ_POS_Y);
    int32_t player_z = ae_rds16(r, player + AE_OBJ_POS_Z);
    int32_t boss_z = ae_rds32(r, boss + AE_OVL1A0_ACTOR_POS_Z);
    int32_t camera_x = ae_rds32(r, player + AE_OBJ_POS_X);

    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_PLAYER_X, (uint32_t)player_x);
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_PLAYER_Y,
            (uint32_t)(player_y - 128));
    int32_t target_z = boss_z < player_z ? boss_z : player_z;
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_PLAYER_Z,
            (uint32_t)target_z);
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_EYE_Y, (uint32_t)-2560);

    int32_t relative_x = camera_x - 1536;
    int32_t weighted_x = relative_x * 3;
    if (weighted_x < 0) weighted_x++;
    int32_t center_x = ovl1a0_asr(weighted_x, 1u) + 1536;
    int32_t half_x = relative_x;
    if (half_x < 0) half_x++;
    half_x = ovl1a0_asr(half_x, 1u) + 1536;
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_CENTER_X,
            (uint32_t)center_x);
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Y,
            (uint32_t)(ae_rds32(r, boss + AE_OVL1A0_ACTOR_POS_Y) - 1100));
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
            (uint32_t)(target_z - 1600));
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_HALF_X, (uint32_t)half_x);
    ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_FAR_Z,
            (uint32_t)(target_z + 1400));

    uint32_t phase = ae_rd8(r, state + AE_OVL1A0_BOSS_STATE_PHASE);
    if (phase == 13u) {
        ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
                ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z) + 800u);
    } else if (phase == 4u || phase == 8u) {
        if (ae_rds32(r, boss + AE_OVL1A0_ACTOR_POS_Z) <
            ae_rds32(r, player + AE_OBJ_POS_Z)) {
            ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
                    ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z) + 1024u);
            ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_EYE_Y,
                    ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_EYE_Y) + 600u);
            ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_FAR_Z,
                    ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_FAR_Z) + 1024u);
        }
    } else if (phase == 20u) {
        ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
                ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z) - 1024u);
    } else if (phase == 24u) {
        int32_t spread = ae_rds32(r, boss + AE_OVL1A0_ACTOR_POS_X) <
                         ae_rds32(r, AE_OVL1A0_PLAYER_STATE) ? 600 : -600;
        int32_t wave = (int32_t)((ovl1a0_random16(r) >> 8) - 128u);
        int32_t bias = ae_rds16(r, boss + AE_OVL1A0_ACTOR_ENTRY_ANGLE) * 8;
        ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_HALF_X,
                ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_HALF_X) +
                spread - bias + wave);
        wave = (int32_t)((ovl1a0_random16(r) >> 8) - 128u);
        ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_CENTER_X,
                ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_CENTER_X) -
                spread - bias + wave);
        wave = (int32_t)(ovl1a0_random16(r) >> 8);
        ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
                ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z) + wave);
    } else if (phase == 25u) {
        if (ae_rd32(r, state + AE_OVL1A0_BOSS_PHASE_METRIC) >= 73u) {
            ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
                    ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z) + 1024u);
            ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_EYE_Y,
                    ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_EYE_Y) - 256u);
        } else {
            ae_wr16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z,
                    ae_rd16(r, scratch + AE_OVL1A0_GTE_CAMERA_TARGET_Z) - 512u);
        }
    }

    ae_wr16(r, scratch, ovl1a0_clamp(ae_rds16(r, scratch), 128, 2944));
    if (ae_rds8(r, AE_OVL1A0_AREA_STATE) == 3 &&
        ae_rd8(r, AE_OVL1A0_PLAYER_OBJECT_FLAGS) != 1u)
        ae_CameraFollowGadgetVariant(r, h ? h->camera_hooks : NULL, subject);
    ae_CameraSlotSmoothTarget(r, h ? h->camera_hooks : NULL, subject, 8, 24, 16);
    ae_CameraSlotSmoothEye(r, h ? h->camera_hooks : NULL, subject, 8, 24, 8);
    return 1;
}

