#include "ae_ovl1a8.h"

#include "../bodies/ae_bodies.h"
#include "../camera/ae_camupd.h"
#include "../camera/ae_camera.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../core/ae_pose.h"
#include "../effects/ae_spawn.h"
#include "../world/ae_stage.h"
#include "../world/ae_zako_init.h"
#include "../world/ae_zako2.h"
#include "../world/ae_progress.h"

#include <limits.h>

/* The sound module's public header also declares legacy camera enum names that
 * collide with ae_camera.h. These are the already verified ports used here. */
int32_t ae_SoundLoopStart(AeRam *r, const AeSoundHw *hw,
                          int32_t mode, int32_t id);
int32_t ae_SoundLoopStop(AeRam *r, const AeSoundHw *hw,
                         int32_t id, int32_t voice);
uint32_t ae_ProgressGetByte(AeRam *r, uint32_t offset);
int32_t ae_PlaySfx(AeRam *r, const AeSoundHw *hw, int32_t id);
void ae_EventCamStartRandom(AeRam *r, const AePlayer1Hooks *h);
void ae_GadgetFlashSet(AeRam *r, uint32_t position);
void ae_SpawnRandomParticle(AeRam *r, uint32_t position, uint32_t variant,
                            uint32_t kind, int32_t speed);

#define BOSS_FLAGS                 (AE_OVL1A8_BOSS + 0x00u)
#define BOSS_PHASE                 (AE_OVL1A8_BOSS + 0x01u)
#define BOSS_WAVE_SFX_FLAG         (AE_OVL1A8_BOSS + 0x02u)
#define BOSS_MODE                  (AE_OVL1A8_BOSS + 0x03u)
#define BOSS_PARAMETER             (AE_OVL1A8_BOSS + 0x04u)
#define BOSS_SEQUENCE_COUNTER      (AE_OVL1A8_BOSS + 0x04u)
#define BOSS_FRAME_COUNTER         0x80141D00u
#define BOSS_TIMER                 (AE_OVL1A8_BOSS + 0x08u)
#define BOSS_STATE                 (AE_OVL1A8_BOSS + 0x09u)
#define BOSS_SUBSTATE              (AE_OVL1A8_BOSS + 0x0Au)
#define BOSS_CAMERA_MOVE_COUNT     (AE_OVL1A8_BOSS + BOSS_CAMERA_MOVE_COUNT_OFFSET)
#define BOSS_CAMERA_MOVE_COUNT_OFFSET 0x0Eu
#define BOSS_FRAME_COUNT_OFFSET    0x0Cu
#define BOSS_SEQUENCE_POSITION     0x80142310u
#define BOSS_DIRECTION_TABLE       0x80141CC0u
#define BOSS_SEQUENCE_EFFECTS_A    0x80142100u
#define BOSS_SEQUENCE_EFFECTS_B    0x801420ACu
#define BOSS_SEQUENCE_EFFECTS_C    0x80142154u
#define BOSS_SEQUENCE_EFFECTS_D    0x801421A8u
#define ACTOR_SPECIAL_TYPE         0x50u
#define ACTOR_SEQUENCE_ACTIVE      0x7Au
#define ACTOR_SEQUENCE_SUBSTATE    0x7Bu
#define ACTOR_SEQUENCE_HIT_COUNT   0x7Eu
#define ACTOR_SEQUENCE_VARIANT     0x88u
#define ACTOR_SEQUENCE_ANIM_BUSY   0x89u
#define ACTOR_SEQUENCE_ANIM_TIMER  0x8Au
#define ACTOR_SEQUENCE_VELOCITY_X  0x80u
#define ACTOR_SEQUENCE_VELOCITY_Y  0x82u
#define ACTOR_SEQUENCE_VELOCITY_Z  0x84u
#define ACTOR_SEQUENCE_ANIM_TIMER2 0x8Eu
#define ACTOR_SEQUENCE_EFFECT_TIMER 0x90u
#define ACTOR_SEQUENCE_TURN_X      0x9Cu
#define ACTOR_SEQUENCE_TURN_Z      0x9Eu
#define ACTOR_SEQUENCE_TARGET_X    0x94u
#define ACTOR_SEQUENCE_TARGET_Z    0x98u
#define ACTOR_SEQUENCE_COUNTER     0x92u
#define ACTOR_SEQUENCE_EFFECT_AGE  0x8Cu
#define ACTOR_SEQUENCE_TURN_TIMER  0x8Eu
#define ACTOR_SEQUENCE_TARGET_COOLDOWN 0x96u
#define ACTOR_SEQUENCE_MODE        0x7Cu
#define ACTOR_SEQUENCE_BODY_TAG    0x50u
#define ACTOR_SEQUENCE_HEADING     0x60u
#define ACTOR_SEQUENCE_BODY_FLAGS  0x4Au
#define ACTOR_SEQUENCE_BODY_MODE   0x48u
#define ACTOR_SEQUENCE_MODEL_SCALE 0x86u
#define ACTOR_ANIMATION_FRAME      0x5Cu
#define ACTOR_MODEL_ANIMATION      0x18u
#define MODEL_RECORD_RESOURCE      0x18u
#define MODEL_RECORD_HEADER        0x1Cu
#define MODEL_TABLE_RECORD_OFFSET  0x08u
#define MODEL_RECORD_SIZE          0x20u
#define MODEL_HEADER_BONES_OFFSET  0x04u
#define ANIMATION_FRAME_STRIDE     0x04u
#define ANIMATION_FRAME_START      0x00u
#define ANIMATION_FRAME_END        0x02u
#define ENTRY_EFFECT_STATE         0x1Au
#define ENTRY_EXTRA_DATA           0x0Cu
#define ENTRY_POSITION             0x04u
#define SPARK_RENDER_STATUS        0x07u
#define SPARK_DELAY                0x08u
#define RIPPLE_VELOCITY_X          0x08u
#define RIPPLE_VELOCITY_Y          0x09u
#define RIPPLE_VELOCITY_Z          0x0Au
#define POSITION16_X               0x00u
#define POSITION16_Y               0x02u
#define POSITION16_Z               0x04u
#define POSITION16_PADDING         0x06u
#define POSITION32_Y               0x04u
#define POSITION32_Z               0x08u
#define POSITION32_X               0x00u
#define VECTOR16_X                 0x00u
#define VECTOR16_Y                 0x02u
#define VECTOR16_Z                 0x04u
#define VECTOR16_PADDING           0x06u
#define NEAR_RECORD_POSITION_X     0x04u
#define NEAR_RECORD_POSITION_Y     0x06u
#define NEAR_RECORD_POSITION_Z     0x08u
#define MODEL_DIRECTION_X          0x08u
#define MODEL_DIRECTION_Y          0x0Au
#define MODEL_DIRECTION_Z          0x0Cu
#define WAKE_DIRECTION_X           0x00u
#define WAKE_DIRECTION_Y           0x02u
#define WAKE_DIRECTION_Z           0x04u
#define PLAYER_FACING_COMPONENT_X  0x60u
#define PLAYER_FACING_COMPONENT_Z  0x64u
#define WAKE_EFFECT_DEFINITION     (WAKE_EFFECTS + 0x54u)
#define AE_OVL1A8_PROJECT_AND_EMIT 0x8013DB94u
#define MODEL_TABLE_POINTER        0x800F4594u
#define PLAYER_QUERY_X             0x800EC204u
#define PLAYER_QUERY_Z             0x800EC20Cu
#define DROP_COUNT                 0x800EC2C8u
#define DROP_ITEM_TYPE             0x801044A4u
#define DROP_ITEM_KIND             0x800F51C1u
#define BOSS_SPARK_MATRIX          0x800E78C0u
#define BOSS_EFFECT_GROUPS         0x80141F88u
#define BOSS_PHASE_EFFECTS         0x80142004u
#define BOSS_SPARK_PROJECT_HOOK    0x8013F228u
#define ENTRY_UPDATE_TEMP_POSITION 0x801FF180u
#define BOSS_ENTRY_EFFECT_DEF      0x80141FB0u
#define BOSS_WAVE_DROP_DEF         0x801421FCu
#define WAVE_VECTOR_TEMP           0x801FFEB0u
#define GLOBAL_EVENT_FLAG          0x800E69BEu
#define BOSS_INIT_EVENT_FLAG       0x800AF840u
#define STAGE_IDENTIFIER_A         0x800F4476u
#define STAGE_IDENTIFIER_B         0x800F447Au
#define SDK_SET_ROT_MATRIX         0x8002FB00u
#define SDK_SET_TRANS_MATRIX       0x8002FB30u
#define RENDER_EFFECT_RING         0x8006E5B4u
#define RENDER_SHADOW              0x8001F604u
#define BOSS_RIPPLE_SETUP_HOOK     0x80140A7Cu
#define BOSS_RIPPLE_PROJECT_HOOK   0x80140C60u
#define BOSS_DEBRIS_PROJECT_HOOK   0x8013D6ACu
#define BOSS_DEBRIS_HEADER_HOOK    0x8013D818u
#define ORIGINAL_UPDATE_ENTRIES    0x8013E120u
#define CAMERA_CURRENT_X           0x0Au
#define CAMERA_CURRENT_Y           0x0Cu
#define CAMERA_CURRENT_Z           0x0Eu
#define CAMERA_TARGET_X            0x12u
#define CAMERA_TARGET_Y            0x14u
#define CAMERA_TARGET_Z            0x16u
#define LATE_POSITION              0x80142318u
#define LATE_EFFECT_GROUPS         0x80141F88u
#define LATE_EFFECT_DEFS           0x80141F88u
#define LATE_WORLD_MODE            0x800EC350u
#define LATE_VECTOR_NORMAL_TEMP    0x801FFF90u
#define LATE_VECTOR_EFFECT_TEMP    0x801FFFA0u
#define LATE_VECTOR_PHYSICS_TEMP   0x801FFFA8u
#define LATE_ROTATION_MATRIX       0x801FFFB0u
#define OVL1A8_SEQUENCE_VECTOR     0x801FFFC8u
#define OVL1A8_SEQUENCE_EFFECT_POS 0x801FFFE0u
#define OVL1A8_SEQUENCE_IMPACT_POS 0x801FFF70u
#define BOSS_ACTOR_SIZE            0xA0u
#define ACTOR_MODEL_DATA           0x244u
#define ACTOR_ANIMATION_TABLE      0x74u
#define ACTOR_INIT_BODY_Y          0x4Au
#define ACTOR_INIT_COLLISION_RADIUS 0x46u
#define ACTOR_INIT_COLLISION_FLAGS 0x48u
#define ACTOR_INIT_COLLISION_HEIGHT 0x44u
#define ACTOR_COLLISION_HEIGHT     0x44u
#define ACTOR_COLLISION_RADIUS     0x46u
#define ACTOR_INIT_ANIM_FLAGS      0x89u
#define ACTOR_INIT_ANIM_HEIGHT     0x8Au
#define ACTOR_INIT_MODEL_FLAGS     0x88u
#define ACTOR_INIT_MODEL_HEIGHT    0x86u
#define ACTOR_INIT_MODEL_TYPE      0x7Eu
#define ACTOR_INIT_MODEL_STATE     0x7Cu
#define ACTOR_INIT_ANGLE_X         0x60u
#define ACTOR_INIT_ANGLE_Y         0x62u
#define ACTOR_INIT_ANGLE_Z         0x64u
#define ACTOR_ANIMATION            0x78u
#define ACTOR_ANIMATION_ACTIVE     0x7Au
#define ACTOR_ANIMATION_TIMER      0x7Bu
#define ACTOR_POSITION_X           0x24u
#define ACTOR_POSITION_Y           0x28u
#define ACTOR_POSITION_Z           0x2Cu
#define ACTOR_RENDER_SCALE         ACTOR_POSITION_Y
#define ACTOR_BODY                 0x24u
#define BODY_POSITION_X            0x00u
#define BODY_POSITION_Y            0x04u
#define BODY_POSITION_Z            0x08u
#define BODY_RADIUS                0x20u
#define BODY_HEIGHT                0x22u
#define BODY_PARAM                 0x24u
#define BODY_FLAGS                 0x26u
#define DEBRIS_POS_X               0x00u
#define DEBRIS_POS_Y               0x02u
#define DEBRIS_POS_Z               0x04u
#define DEBRIS_VELOCITY_Y          0x06u
#define DEBRIS_VELOCITY_X          0x08u
#define DEBRIS_VELOCITY_Z          0x09u
#define DEBRIS_LIFE                0x0Au
#define ORDERING_TABLE_CURSOR      0x800EC354u
#define BOSS_PLAYER                0x800EC1E0u
#define ENTRY_ACTIVE               0x0Au
#define ENTRY_STATE                0x0Au
#define ENTRY_TIMER                0x0Bu
#define ENTRY_POSITION_X           0x04u
#define ENTRY_POSITION_Y           0x06u
#define ENTRY_POSITION_Z           0x08u
#define ENTRY_VECTOR_X             0x14u
#define ENTRY_VECTOR_Y             0x16u
#define ENTRY_VECTOR_Z             0x18u
#define SPARK_POS_X                0x00u
#define SPARK_POS_Y                0x02u
#define SPARK_POS_Z                0x04u
#define SPARK_COLOR                0x08u
#define SPARK_ACTIVE               0x06u
#define SPARK_LIFE                 0x0Au
#define WAVE_POS_X                 0x04u
#define WAVE_POS_Y                 0x06u
#define WAVE_POS_Z                 0x08u
#define WAVE_AGE                   0x0Au
#define WAVE_STATE                 0x0Bu
#define WAVE_VELOCITY_X            0x0Cu
#define WAVE_VELOCITY_Y            0x0Eu
#define WAVE_VELOCITY_Z            0x10u
#define WAVE_RING_ANGLE            0x14u
#define WAVE_RING_PHASE            0x18u
#define WAVE_RING_STEP             0x1Cu
#define WAVE_RADIUS                0x12u
#define WAVE_COLOR                 0x1Eu
#define WAVE_RANDOM_A              WAVE_RING_ANGLE
#define WAVE_RANDOM_B              WAVE_RING_PHASE
#define WAVE_RANDOM_C              WAVE_RING_STEP
#define WAVE_ACTIVE                WAVE_STATE
#define RIPPLE_POS_X               0x00u
#define RIPPLE_POS_Y               0x02u
#define RIPPLE_POS_Z               0x04u
#define RIPPLE_GROUND_Y            0x06u
#define RIPPLE_VELOCITY_X          0x08u
#define RIPPLE_VELOCITY_Y          0x09u
#define RIPPLE_VELOCITY_Z          0x0Au
#define RIPPLE_LIFE                0x0Bu
#define CAMERA_TEMP_VECTOR         0x1F800000u
#define WAKE_POSITION              0x801FFEC0u
#define WAKE_FLASH_POSITION        0x801FFEC8u
#define WAKE_DIRECTION_TABLE       0x801422F8u
#define WAKE_EFFECTS               0x80142250u
#define DRAW_SCALE_VECTOR          0x801FFEE0u
#define WAVE_SHADOW_POSITION       0x801FFEB8u
#define WAVE_SHADOW_MATRIX         0x801FFEC8u
#define CAMERA_TRANSITION_MODE     0x800AF130u
#define CAMERA_AREA_STATE          0x800EC2D2u
#define CAMERA_ACTIVE_STATE        0x800E6916u
#define BOSS_UPDATE_VECTOR_TEMP    0x801FFED8u
#define LATE_WORLD_MODE            0x800EC350u

static int ovl1a8_set_position_padding(AeRam *r,
                                       const AeOvl1a8Hooks *h,
                                       uint32_t position,
                                       uint32_t original_function);

static int32_t ovl1a8_asr(int32_t value, unsigned shift) {
    uint32_t bits = (uint32_t)value;
    if (shift == 0) return value;
    if (shift >= 32) return value < 0 ? -1 : 0;
    if (value >= 0) return (int32_t)(bits >> shift);
    return (int32_t)~((~bits) >> shift);
}

static int32_t ovl1a8_call(AeRam *r, const AeOvl1a8Hooks *h,
                           const char *name, uint32_t address,
                           uint32_t count, const uint32_t *args) {
    if (!h || !h->original_call) {
        ae_ram_fail(r, "Area57 overlay call hook missing: %s", name);
        return 0;
    }
    return h->original_call(h->user, r, name, address, count, args);
}

static int32_t ovl1a8_render(AeRam *r, const AeOvl1a8Hooks *h,
                             const char *name, uint32_t address,
                             uint32_t count, const uint32_t *args) {
    if (!h || !h->renderer) {
        ae_ram_fail(r, "Area57 renderer hook missing: %s", name);
        return 0;
    }
    return h->renderer(h->user, r, name, address, count, args);
}

static uint32_t ovl1a8_debris(uint32_t index) {
    return AE_OVL1A8_DEBRIS - index * AE_OVL1A8_DEBRIS_SIZE;
}

static uint32_t ovl1a8_entry(uint32_t index) {
    return AE_OVL1A8_ENTRIES + index * AE_OVL1A8_ENTRY_SIZE;
}

static uint32_t ovl1a8_spark(uint32_t index) {
    return AE_OVL1A8_SPARKS + index * AE_OVL1A8_SPARK_SIZE;
}

static uint32_t ovl1a8_wave(uint32_t index) {
    return AE_OVL1A8_WAVES + index * AE_OVL1A8_WAVE_SIZE;
}

static uint32_t ovl1a8_ripple(uint32_t index) {
    return AE_OVL1A8_RIPPLES_LAST - index * AE_OVL1A8_RIPPLE_SIZE;
}

void ae_ovl1a8_BossModelInit(AeRam *r, const AeOvl1a8Hooks *h,
                             uint32_t model_index, uint32_t actor,
                             uint32_t model_out) {
    (void)h;
    uint32_t model_table = ae_rd32(r, MODEL_TABLE_POINTER);
    uint32_t model_record = model_table + ae_rd32(r, model_table + MODEL_TABLE_RECORD_OFFSET) +
                            (model_index & 0xFFFFFFFFu) * MODEL_RECORD_SIZE;
    uint32_t model_resource = model_table +
                              ae_rd32(r, model_record + MODEL_RECORD_RESOURCE);
    ae_wr32(r, AE_OVL1A8_MODEL_HEADERS + model_index * 4u, model_resource);

    uint32_t model_header = model_table +
                            ae_rd32(r, model_record + MODEL_RECORD_HEADER);
    ae_wr32(r, AE_OVL1A8_MODEL_HEADER_PTRS + model_index * 4u, model_header);
    ae_wr32(r, model_out, model_header + MODEL_HEADER_BONES_OFFSET);
    uint32_t bone_count = ae_rd16(r, model_header);
    ae_wr32(r, actor + ACTOR_MODEL_ANIMATION,
            model_header + MODEL_HEADER_BONES_OFFSET + bone_count * 4u);

    ae_ModelInitAuto(r, actor, model_resource);
}

void ae_ovl1a8_BossSpawnEffectNear(AeRam *r, const AeOvl1a8Hooks *h,
                                   uint32_t record, uint32_t owner,
                                   uint32_t body_value, uint32_t flags,
                                   uint32_t body_flags) {
    int32_t x = ae_rds16(r, record + NEAR_RECORD_POSITION_X);
    int32_t z = ae_rds16(r, record + NEAR_RECORD_POSITION_Z);
    int32_t dx = ae_rds32(r, PLAYER_QUERY_X) - x;
    int32_t dz = ae_rds32(r, PLAYER_QUERY_Z) - z;
    uint32_t distance2 = (uint32_t)((int64_t)dx * dx + (int64_t)dz * dz);
    if (distance2 > 0x24000u) {
        ae_wr32(r, record, 0);
        return;
    }

    uint32_t body = ae_CollisionBodyAlloc(r);
    ae_wr32(r, record, body);
    if (body == 0) return;
    ae_wr32(r, body + BODY_POSITION_X, (uint32_t)x);
    ae_wr32(r, body + BODY_POSITION_Y, (uint32_t)ae_rds16(r, record + NEAR_RECORD_POSITION_Y));
    ae_wr32(r, body + BODY_POSITION_Z, (uint32_t)z);
    ae_wr16(r, body + BODY_RADIUS, body_value);
    ae_wr16(r, body + BODY_PARAM, flags | 1u);
    ae_wr16(r, body + BODY_FLAGS, body_flags);
    ae_CollisionBodyRegister(r, h ? h->body : NULL, 7, body, owner);
}

void ae_ovl1a8_BossSpawnEffectNearWrap(AeRam *r, const AeOvl1a8Hooks *h,
                                       uint32_t record, uint32_t owner,
                                       uint32_t body_value,
                                       int32_t body_flags) {
    ae_ovl1a8_BossSpawnEffectNear(r, h, record, owner, body_value, 0,
                                  (uint32_t)(int16_t)body_flags);
}

void ae_ovl1a8_BossSpawnDebris(AeRam *r, uint32_t position) {
    int32_t x = ae_rds16(r, position);
    int32_t y = ae_rds16(r, position + POSITION16_Y);
    int32_t z = ae_rds16(r, position + POSITION16_Z);
    uint32_t seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
    for (uint32_t i = 0; i < AE_OVL1A8_DEBRIS_COUNT; ++i) {
        uint32_t record = AE_OVL1A8_DEBRIS - i * AE_OVL1A8_DEBRIS_SIZE;
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, record + DEBRIS_LIFE, 40u + ((seed >> 8) & 31u));

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        int32_t random_x = (int32_t)(seed >> 8) - 128;
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        int32_t random_y = (int32_t)(seed >> 8) - 128;
        ae_wr16(r, record + DEBRIS_POS_X, (uint32_t)(x + random_x * 2));
        ae_wr16(r, record + DEBRIS_POS_Y, (uint32_t)(y - random_y * 2));

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        int32_t random_z = (int32_t)(seed >> 8) - 128;
        ae_wr16(r, record + DEBRIS_POS_Z, (uint32_t)(z + random_z * 2));

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        int32_t angle = (int32_t)(seed >> 8) - 128;
        ae_wr8(r, record + DEBRIS_VELOCITY_X,
               (uint32_t)ovl1a8_asr(angle + (angle < 0), 1));

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr16(r, record + DEBRIS_VELOCITY_Y,
                (uint32_t)(-((int32_t)((seed >> 8) & 0x3Fu)) - 32));
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        angle = (int32_t)(seed >> 8) - 128;
        ae_wr8(r, record + DEBRIS_VELOCITY_Z,
               (uint32_t)ovl1a8_asr(angle + (angle < 0), 1));
    }
}

void ae_ovl1a8_BossUpdateDebris(AeRam *r, const AeOvl1a8Hooks *h) {
    uint32_t cursor = ae_rd32(r, ORDERING_TABLE_CURSOR);

    for (uint32_t index = 0; index < AE_OVL1A8_DEBRIS_COUNT; ++index) {
        uint32_t debris = ovl1a8_debris(index);
        uint32_t life = ae_rd8(r, debris + DEBRIS_LIFE);
        if (life == 0) continue;

        life = (life - 1u) & 0xFFu;
        ae_wr8(r, debris + DEBRIS_LIFE, life);

        int32_t velocity_x = ae_rds8(r, debris + DEBRIS_VELOCITY_X);
        int32_t velocity_z = ae_rds8(r, debris + DEBRIS_VELOCITY_Z);
        uint32_t velocity_y = ae_rd16(r, debris + DEBRIS_VELOCITY_Y);
        ae_wr16(r, debris + DEBRIS_POS_X,
                ae_rd16(r, debris + DEBRIS_POS_X) + (uint32_t)velocity_x);
        ae_wr16(r, debris + DEBRIS_POS_Y,
                ae_rd16(r, debris + DEBRIS_POS_Y) + velocity_y);
        ae_wr16(r, debris + DEBRIS_POS_Z,
                ae_rd16(r, debris + DEBRIS_POS_Z) + (uint32_t)velocity_z);

        velocity_y = (velocity_y + 3u) & 0xFFFFu;
        ae_wr16(r, debris + DEBRIS_VELOCITY_Y, velocity_y);

        uint32_t draw_args[10] = {
            debris,
            ae_rd16(r, debris + DEBRIS_POS_X),
            ae_rd16(r, debris + DEBRIS_POS_Y),
            ae_rd16(r, debris + DEBRIS_POS_Z),
            (uint32_t)velocity_x,
            velocity_y,
            (uint32_t)velocity_z,
            life,
            79u - index,
            cursor
        };
        (void)ovl1a8_render(r, h, "Area57_Boss_DebrisProjectAndEmit",
                            BOSS_DEBRIS_PROJECT_HOOK, 10, draw_args);
        if (ae_ram_failed(r)) return;
    }

    uint32_t end_args[1] = {cursor};
    (void)ovl1a8_render(r, h, "Area57_Boss_DebrisBatchHeader",
                        BOSS_DEBRIS_HEADER_HOOK, 1, end_args);
    ae_wr32(r, ORDERING_TABLE_CURSOR, cursor);
}

void ae_ovl1a8_BossDrawScaledModel(AeRam *r, const AeOvl1a8Hooks *h,
                                   uint32_t actor) {
    uint32_t phase = ae_rd8(r, BOSS_PHASE);
    uint32_t original_scale = ae_rd32(r, actor + ACTOR_RENDER_SCALE);
    uint32_t base_scale = phase < 20u ? 0x1CCCu : 0x1000u;
    uint32_t scale_x = base_scale;
    uint32_t scale_y = base_scale;
    uint32_t scale_z = base_scale;

    if (phase == 19u) {
        scale_x = 0x1800u;
        scale_y = 0x1800u;
        scale_z = 0x1800u;
    } else if (phase == 2u) {
        uint32_t amount = ae_rd32(r, BOSS_PARAMETER) << 8;
        scale_x = 0x2000u - amount;
        scale_y = amount + 0x2000u;
        scale_z = scale_x;
    } else if (phase == 3u) {
        uint32_t amount = ae_rd32(r, BOSS_PARAMETER);
        if (amount >= 8u) return;
        scale_x = 0x2000u - (amount << 10);
        scale_y = (amount << 12) + 0x2000u;
        scale_z = scale_x;
    } else if (phase == 4u) {
        uint32_t amount = ae_rd32(r, BOSS_PARAMETER);
        scale_x = (amount << 8) + 0x400u;
        scale_y = 0x8000u - (amount << 10);
        scale_z = scale_x;
    }

    ae_wr32(r, DRAW_SCALE_VECTOR, scale_x);
    ae_wr32(r, DRAW_SCALE_VECTOR + 4u, scale_y);
    ae_wr32(r, DRAW_SCALE_VECTOR + 8u, scale_z);

    if (phase < 20u) {
        int32_t source_angle = ae_rds16(r, actor + ACTOR_INIT_MODEL_HEIGHT);
        int32_t angle = source_angle + 68;
        int32_t quotient_input = angle;
        if (angle < 0) quotient_input = source_angle + 4163;
        int32_t quotient = ovl1a8_asr(quotient_input, 12);
        int32_t wrapped_angle = angle - quotient * 4096;
        ae_wr16(r, actor + ACTOR_INIT_MODEL_HEIGHT,
                (uint16_t)wrapped_angle);
        int32_t scale_adjustment = ovl1a8_asr(
            ae_sdk_rsin(r, (int16_t)wrapped_angle), 6);
        ae_wr32(r, actor + ACTOR_RENDER_SCALE,
                original_scale + (uint32_t)scale_adjustment);
    }

    uint32_t color_flags = 0;
    uint32_t model_type = ae_rd8(r, actor + ACTOR_INIT_MODEL_TYPE);
    uint32_t frame = ae_rd32(r, BOSS_FRAME_COUNTER);
    if (model_type == 2u) {
        uint32_t index = frame & 0x0Fu;
        if (index >= 8u) index = 15u - index;
        color_flags = (index << 4) | 0x10000u;
    } else if (model_type < 2u) {
        uint32_t index = frame & 7u;
        if (index >= 4u) index = 7u - index;
        color_flags = (index << 5) | 0x10000u;
    }

    uint32_t args[5] = {actor, scale_x, scale_y, scale_z, color_flags};
    (void)ovl1a8_render(r, h, "Area57_Boss_ProjectAndEmit",
                        AE_OVL1A8_PROJECT_AND_EMIT, 5, args);
    ae_wr32(r, actor + ACTOR_RENDER_SCALE, original_scale);
    ae_wr32(r, BOSS_FRAME_COUNTER, (frame + 1u) & 0xFFu);
}

void ae_ovl1a8_BossRumbleLong(AeRam *r, const AeOvl1a8Hooks *h) {
    ae_PadRumble(r, h ? h->camupd : NULL, 1, 200, 10);
}

void ae_ovl1a8_BossRumbleShort(AeRam *r, const AeOvl1a8Hooks *h) {
    ae_PadRumble(r, h ? h->camupd : NULL, 1, 60, 2);
}

void ae_ovl1a8_BossRumbleMedium(AeRam *r, const AeOvl1a8Hooks *h) {
    ae_PadRumble(r, h ? h->camupd : NULL, 1, 120, 2);
}

void ae_ovl1a8_BossSpawnDropPickups(AeRam *r, const AeOvl1a8Hooks *h,
                                    uint32_t position) {
    uint32_t kind = 1;
    if (ae_rds16(r, DROP_COUNT) >= 3) {
        uint32_t seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
        seed = seed * 5u + 1u;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        if (((seed >> 8) & 1u) != 0) {
            kind = ae_rd16(r, DROP_ITEM_TYPE) == ae_rd8(r, DROP_ITEM_KIND) ? 3u : 6u;
        }
    }
    ae_SpawnPickups(r, h ? h->zako2 : NULL, kind, position, 3, 0);
}

void ae_ovl1a8_BossResetLoopHandle(AeRam *r) {
    ae_wr32(r, AE_OVL1A8_LOOP_HANDLE, 0xFFFFFFFFu);
}

void ae_ovl1a8_BossLoopStart(AeRam *r, const AeOvl1a8Hooks *h,
                              int32_t sound_id) {
    ae_wr32(r, AE_OVL1A8_LOOP_HANDLE,
            (uint32_t)ae_SoundLoopStart(r, h ? h->sound_hw : NULL, 0, sound_id));
}

void ae_ovl1a8_BossLoopStop(AeRam *r, const AeOvl1a8Hooks *h,
                             int32_t sound_id) {
    uint32_t handle = ae_rd32(r, AE_OVL1A8_LOOP_HANDLE);
    if (handle == 0xFFFFFFFFu) return;
    (void)ae_SoundLoopStop(r, h ? h->sound_hw : NULL,
                           sound_id, (int32_t)handle);
}

void ae_ovl1a8_BossClearEntries(AeRam *r) {
    ae_wr8(r, AE_OVL1A8_ENTRIES + ENTRY_ACTIVE, 0);
}

void ae_ovl1a8_BossAddEntry(AeRam *r, uint32_t source) {
    for (uint32_t i = 0; i < AE_OVL1A8_ENTRY_COUNT; ++i) {
        uint32_t entry = ovl1a8_entry(i);
        if (ae_rd8(r, entry + ENTRY_ACTIVE) != 0) continue;
        ae_wr8(r, entry + ENTRY_ACTIVE, 1);
        ae_wr8(r, entry + ENTRY_EFFECT_STATE, 0);
        ae_wr16(r, entry + ENTRY_POSITION_X, ae_rd16(r, source));
        ae_wr16(r, entry + ENTRY_POSITION_Y, ae_rd16(r, source + POSITION16_Y));
        ae_wr16(r, entry + ENTRY_POSITION_Z, ae_rd16(r, source + POSITION16_Z));
        for (uint32_t b = 0; b < 8; ++b)
            ae_wr8(r, entry + ENTRY_EXTRA_DATA + b, ae_rd8(r, source + b));
        ae_wr16(r, entry + ENTRY_VECTOR_X,
                (uint32_t)(-256 - ae_rd16(r, source)));
        ae_wr16(r, entry + ENTRY_VECTOR_Y,
                (uint32_t)(-2560 - ae_rd16(r, source + POSITION16_Y)));
        ae_wr16(r, entry + ENTRY_VECTOR_Z,
                (uint32_t)(-256 - ae_rd16(r, source + POSITION16_Z)));
        ae_sdk_VectorNormalSS(r, entry + ENTRY_VECTOR_X, entry + ENTRY_VECTOR_X);
        return;
    }
}

void ae_ovl1a8_BossClearSparks(AeRam *r) {
    for (uint32_t i = 0; i < AE_OVL1A8_SPARK_COUNT; ++i)
        ae_wr8(r, ovl1a8_spark(i) + SPARK_ACTIVE, 0);
}

void ae_ovl1a8_BossAddSparks(AeRam *r, uint32_t actor, uint32_t spread) {
    uint32_t seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
    for (uint32_t i = 0; i < AE_OVL1A8_SPARK_COUNT; ++i) {
        uint32_t spark = ovl1a8_spark(i);
        ae_wr8(r, spark + SPARK_ACTIVE, 32);

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, spark + SPARK_RENDER_STATUS, (seed >> 8) & 1u);
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, spark + SPARK_DELAY, (seed >> 8) & 0x0Fu);

        if (spread != 0) {
            seed = (seed * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            int32_t x_offset = ((int32_t)(seed >> 8) - 128) * 16;
            ae_wr16(r, spark + SPARK_POS_X,
                    (uint32_t)(ae_rd16(r, actor + ACTOR_POSITION_X) + x_offset));
            seed = (seed * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            int32_t y_offset = (int32_t)((seed >> 4) & 0xFF0u) - 2638;
            ae_wr16(r, spark + SPARK_POS_Y,
                    (uint32_t)(ae_rd16(r, actor + ACTOR_POSITION_Y) + y_offset));
            ae_wr16(r, spark + SPARK_LIFE, (uint32_t)-590);
            seed = (seed * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            int32_t z_offset = ((int32_t)(seed >> 8) - 128) * 16;
            ae_wr16(r, spark + SPARK_POS_Z,
                    (uint32_t)(ae_rd16(r, actor + ACTOR_POSITION_Z) + z_offset));
        } else {
            seed = (seed * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            int32_t x_offset = ((int32_t)(seed >> 8) - 128) * 8;
            ae_wr16(r, spark + SPARK_POS_X,
                    (uint32_t)(ae_rd16(r, actor + ACTOR_POSITION_X) + x_offset));
            seed = (seed * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            int32_t y_offset = (int32_t)((seed >> 6) & 0x3FCu);
            ae_wr16(r, spark + SPARK_POS_Y,
                    (uint32_t)(ae_rd16(r, actor + ACTOR_POSITION_Y) - y_offset));
            ae_wr16(r, spark + SPARK_LIFE, (uint32_t)-64);
            seed = (seed * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            int32_t z_offset = ((int32_t)(seed >> 8) - 128) * 8;
            ae_wr16(r, spark + SPARK_POS_Z,
                    (uint32_t)(ae_rd16(r, actor + ACTOR_POSITION_Z) + z_offset));
        }
    }
}

void ae_ovl1a8_BossUpdateSparks(AeRam *r, const AeOvl1a8Hooks *h,
                                uint32_t actor) {
    for (uint32_t i = 0; i < AE_OVL1A8_SPARK_COUNT; ++i) {
        uint32_t spark = ovl1a8_spark(i);
        uint32_t active = ae_rd8(r, spark + SPARK_ACTIVE);
        if (active == 0) continue;

        uint32_t delay = ae_rd8(r, spark + SPARK_DELAY);
        if (delay != 0) {
            ae_wr8(r, spark + SPARK_DELAY, delay - 1u);
            continue;
        }
        ae_wr8(r, spark + SPARK_ACTIVE, active - 1u);

        int32_t x = ae_rds16(r, spark + SPARK_POS_X);
        int32_t y = ae_rds16(r, spark + SPARK_POS_Y);
        int32_t z = ae_rds16(r, spark + SPARK_POS_Z);
        int32_t life = ae_rds16(r, spark + SPARK_LIFE);
        int32_t next_x = ae_rds32(r, actor + ACTOR_POSITION_X) + 7 * x;
        int32_t next_y = ae_rds32(r, actor + ACTOR_POSITION_Y) +
                         life + 7 * y;
        int32_t next_z = ae_rds32(r, actor + ACTOR_POSITION_Z) + 7 * z;
        if (next_x < 0) next_x += 7;
        if (next_y < 0) next_y += 7;
        if (next_z < 0) next_z += 7;
        ae_wr16(r, spark + SPARK_POS_X, (uint32_t)ovl1a8_asr(next_x, 3));
        ae_wr16(r, spark + SPARK_POS_Y, (uint32_t)ovl1a8_asr(next_y, 3));
        ae_wr16(r, spark + SPARK_POS_Z, (uint32_t)ovl1a8_asr(next_z, 3));
    }

    uint32_t matrix = BOSS_SPARK_MATRIX;
    uint32_t matrix_args[1] = {matrix};
    (void)ovl1a8_call(r, h, "SetRotMatrix", SDK_SET_ROT_MATRIX, 1, matrix_args);
    (void)ovl1a8_call(r, h, "SetTransMatrix", SDK_SET_TRANS_MATRIX, 1, matrix_args);
    if (ae_ram_failed(r)) return;

    const uint32_t effect_groups = BOSS_EFFECT_GROUPS;
    uint32_t cursor = ae_rd32(r, ORDERING_TABLE_CURSOR);
    for (uint32_t i = 0; i < AE_OVL1A8_SPARK_COUNT; ++i) {
        uint32_t spark = ovl1a8_spark(i);
        if (ae_rd8(r, spark + SPARK_ACTIVE) == 0 || ae_rd8(r, spark + SPARK_DELAY) != 0)
            continue;

        uint32_t status = ae_rd8(r, spark + SPARK_RENDER_STATUS);
        uint32_t args[7] = {
            actor, spark,
            (uint32_t)ae_rds16(r, spark + SPARK_POS_X),
            (uint32_t)ae_rds16(r, spark + SPARK_POS_Y),
            (uint32_t)ae_rds16(r, spark + SPARK_POS_Z),
            cursor, status
        };
        int32_t next_cursor = ovl1a8_render(r, h, "Area57_Boss_SparkProject",
                                             BOSS_SPARK_PROJECT_HOOK, 7, args);
        if (next_cursor == 0) continue;
        if (status != 0) {
            ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 207, spark,
                                effect_groups, 0);
        } else {
            cursor = (uint32_t)next_cursor;
        }
    }
    ae_wr32(r, ORDERING_TABLE_CURSOR, cursor);
}

void ae_ovl1a8_BossUpdateEntries(AeRam *r, const AeOvl1a8Hooks *h) {
    const uint32_t effect_groups = BOSS_EFFECT_GROUPS;
    const uint32_t phase_effects = BOSS_PHASE_EFFECTS;
    const uint32_t body_owner = AE_OVL1A8_ACTORS;
    /* The original loop's post-test exits after processing slot zero. */
    for (uint32_t i = 0; i < 1u; ++i) {
        uint32_t entry = ovl1a8_entry(i);
        uint32_t mode = ae_rd8(r, entry + ENTRY_STATE);
        if (mode == 1u) {
            int32_t vx = ae_rds16(r, entry + ENTRY_VECTOR_X);
            int32_t vy = ae_rds16(r, entry + ENTRY_VECTOR_Y);
            int32_t vz = ae_rds16(r, entry + ENTRY_VECTOR_Z);
            uint32_t x = (uint32_t)ae_rd16(r, entry + ENTRY_POSITION_X) +
                         (uint32_t)ovl1a8_asr(vx + (vx < 0 ? 31 : 0), 5);
            uint32_t y = (uint32_t)ae_rd16(r, entry + ENTRY_POSITION_Y) +
                         (uint32_t)ovl1a8_asr(vy + (vy < 0 ? 31 : 0), 5);
            uint32_t z = (uint32_t)ae_rd16(r, entry + ENTRY_POSITION_Z) +
                         (uint32_t)ovl1a8_asr(vz + (vz < 0 ? 31 : 0), 5);
            ae_wr16(r, entry + ENTRY_POSITION_X, x);
            ae_wr16(r, entry + ENTRY_POSITION_Y, y);
            ae_wr16(r, entry + ENTRY_POSITION_Z, z);

            int32_t px = (int32_t)(int16_t)ae_rd16(r, entry + ENTRY_POSITION_X) +
                         ovl1a8_asr(vx + (vx < 0 ? 15 : 0), 4);
            int32_t py = (int32_t)(int16_t)ae_rd16(r, entry + ENTRY_POSITION_Y) +
                         ovl1a8_asr(vy + (vy < 0 ? 15 : 0), 4);
            int32_t pz = (int32_t)(int16_t)ae_rd16(r, entry + ENTRY_POSITION_Z) +
                         ovl1a8_asr(vz + (vz < 0 ? 15 : 0), 4);
            uint32_t position = ENTRY_UPDATE_TEMP_POSITION;
            ae_wr16(r, position, px);
            ae_wr16(r, position + POSITION16_Y, py);
            ae_wr16(r, position + POSITION16_Z, pz);
            if (!ovl1a8_set_position_padding(r, h, position, ORIGINAL_UPDATE_ENTRIES))
                return;
            for (uint32_t n = 0; n < 4u; ++n)
                ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 200, position,
                                    effect_groups, 0);

            uint32_t timer = (ae_rd8(r, entry + ENTRY_TIMER) + 1u) & 0xFFu;
            ae_wr8(r, entry + ENTRY_TIMER, timer);
            if ((int16_t)ae_rd16(r, entry + ENTRY_POSITION_Y) >= -2560) {
                ae_wr16(r, entry + ENTRY_POSITION_Y, (uint32_t)-2688);
                ae_wr8(r, entry + ENTRY_STATE, 2);
                ae_wr8(r, entry + ENTRY_TIMER, 65);
                ae_TrailSpawn(r, entry + ENTRY_POSITION, 1, 1024, 0);
                ae_CameraShake(r, 2);
                ae_EffectSpawnFromDef(r, h ? h->spawn : NULL, entry + ENTRY_POSITION,
                                      BOSS_ENTRY_EFFECT_DEF, 1, 0, 0);
                ae_ovl1a8_BossSpawnDebris(r, entry + ENTRY_POSITION);
                ae_ovl1a8_BossSpawnEffectGroupsN(r, h, entry + ENTRY_POSITION);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 177);
                ae_ovl1a8_BossRumbleLong(r, h);
            }
        } else if (mode == 2u) {
            uint32_t timer = ae_rd8(r, entry + ENTRY_TIMER);
            uint32_t scale = timer < 20u ? timer * 20u : 400u;
            uint32_t draw_args[4] = {
                entry + ENTRY_POSITION, scale + 112u, 5u, 0x00808080u
            };
            (void)ovl1a8_render(r, h, "EffectRingDraw", RENDER_EFFECT_RING, 4, draw_args);

            uint32_t seed = (ae_rd16(r, AE_SPAWN_RAND_SEED) * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
            draw_args[1] = scale + (seed >> 9);
            draw_args[2] = 1u;
            (void)ovl1a8_render(r, h, "EffectRingDraw", RENDER_EFFECT_RING, 4, draw_args);

            uint32_t body_args[5] = {AE_OVL1A8_ENTRIES, body_owner, scale, 1024u, 512u};
            ae_ovl1a8_BossSpawnEffectNear(r, h, body_args[0], body_args[1],
                                         body_args[2], body_args[3], body_args[4]);
            timer = (timer - 1u) & 0xFFu;
            ae_wr8(r, entry + ENTRY_TIMER, timer);
            if (timer == 0u) {
                ae_wr8(r, entry + ENTRY_STATE, 0);
                ae_SpawnParticle(r, h ? h->spawn : NULL, 274, entry + ENTRY_POSITION);
            } else if ((timer & 0x0Fu) == 0u) {
                for (uint32_t n = 0; n < 16u; ++n)
                    ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 202,
                                        AE_OVL1A8_ENTRIES + 4u, effect_groups, 0);
                uint32_t index = ae_rd8(r, entry + ENTRY_EFFECT_STATE);
                uint32_t definition = phase_effects + index * 84u;
                ae_EffectSpawnFromDef(r, h ? h->spawn : NULL, entry + ENTRY_POSITION,
                                      definition, 1, 0, 0);
                ae_wr8(r, entry + ENTRY_EFFECT_STATE, 1);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 177);
            }
            uint32_t random = (ae_rd16(r, AE_SPAWN_RAND_SEED) * 5u + 1u) & 0xFFFFu;
            ae_wr16(r, AE_SPAWN_RAND_SEED, random);
            if (((random >> 8) & 3u) == 0)
                ae_SpawnRandomParticle(r, entry + ENTRY_POSITION, 0, 0x101u, 4096);
            ae_ovl1a8_BossRumbleShort(r, h);
        }
    }
}

void ae_ovl1a8_BossClearRipples(AeRam *r) {
    for (uint32_t i = 0; i < AE_OVL1A8_RIPPLE_COUNT; ++i)
        ae_wr8(r, ovl1a8_ripple(i) + RIPPLE_LIFE, 0);
}

void ae_ovl1a8_BossResetFlag(AeRam *r) {
    ae_wr8(r, BOSS_FLAGS, 0);
    ae_ovl1a8_BossClearFlag(r, 1);
}

void ae_ovl1a8_BossClearFlag(AeRam *r, uint32_t value) {
    ae_wr8(r, BOSS_PHASE, value);
}

void ae_ovl1a8_BossSetAnim(AeRam *r, uint32_t actor, uint32_t animation) {
    if (ae_rd16(r, actor + ACTOR_ANIMATION) == (uint16_t)animation) return;
    ae_wr16(r, actor + ACTOR_ANIMATION, animation);
    uint32_t table = ae_rd32(r, actor + ACTOR_ANIMATION_TABLE);
    uint32_t first = ae_rd16(r, table + (animation & 0xFFFFu) * 4u);
    ae_wr8(r, actor + ACTOR_ANIMATION_ACTIVE, 1);
    ae_wr8(r, actor + ACTOR_ANIMATION_TIMER, 0);
    ae_wr16(r, actor + ACTOR_ANIMATION_FRAME, first);
}

void ae_ovl1a8_BossStepAnim(AeRam *r, uint32_t actor) {
    uint32_t animation = ae_rd16(r, actor + ACTOR_ANIMATION);
    uint32_t table = ae_rd32(r, actor + ACTOR_ANIMATION_TABLE);
    uint32_t frame = (ae_rd16(r, actor + ACTOR_ANIMATION_FRAME) + 1u) & 0xFFFFu;
    ae_wr16(r, actor + ACTOR_ANIMATION_FRAME, frame);
    uint32_t end = ae_rd16(r, table + animation * ANIMATION_FRAME_STRIDE + ANIMATION_FRAME_END);
    if (frame == end) ae_wr8(r, actor + ACTOR_ANIMATION_ACTIVE, 0);
    if (end < frame) {
        if (ae_rd8(r, actor + ACTOR_ANIMATION_TIMER) == 1u)
            ae_wr16(r, actor + ACTOR_ANIMATION_FRAME, frame - 1u);
        else
            ae_wr16(r, actor + ACTOR_ANIMATION_FRAME, ae_rd16(r, table + animation * ANIMATION_FRAME_STRIDE + ANIMATION_FRAME_START));
    }
}

void ae_ovl1a8_BossClearTable8(AeRam *r) {
    for (uint32_t i = 0; i < AE_OVL1A8_WAVE_COUNT; ++i)
        ae_wr8(r, ovl1a8_wave(i) + WAVE_ACTIVE, 0);
}

void ae_ovl1a8_BossRumbleBig(AeRam *r, const AeOvl1a8Hooks *h) {
    ae_PadRumble(r, h ? h->camupd : NULL, 1, 170, 6);
}

int32_t ae_ovl1a8_BossAddWaveEntry(AeRam *r, const AeOvl1a8Hooks *h) {
    for (uint32_t i = 0; i < AE_OVL1A8_WAVE_COUNT; ++i) {
        uint32_t wave = ovl1a8_wave(i);
        if (ae_rd8(r, wave + WAVE_ACTIVE) != 0) continue;
        ae_wr8(r, wave + WAVE_ACTIVE, 1);
        ae_wr8(r, wave + WAVE_AGE, 0);
        uint32_t seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
        seed = seed * 5u + 1u;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr32(r, wave + WAVE_RANDOM_B, (seed >> 4) & 0xFF0u);
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr16(r, wave + WAVE_RANDOM_C, ((seed >> 2) & 0xC0u) - 96u);
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr32(r, wave + WAVE_RANDOM_A, (seed >> 4) & 0xFF0u);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 0x8E);
        return 0;
    }
    return 1;
}

static int32_t ovl1a8_wave_div8(int32_t value) {
    return ovl1a8_asr(value + (value < 0 ? 7 : 0), 3);
}

static int32_t ovl1a8_wave_div256(int32_t value) {
    return ovl1a8_asr(value + (value < 0 ? 255 : 0), 8);
}

static int32_t ovl1a8_wave_wrap12(int32_t value) {
    int32_t quotient_input = value;
    if (value < 0) quotient_input += 4095;
    int32_t quotient = ovl1a8_asr(quotient_input, 12);
    return value - quotient * 4096;
}

static int32_t ovl1a8_wake_direction_component(AeRam *r, uint32_t offset) {
    int32_t component = ae_rds16(r, BOSS_PLAYER + offset);
    return ovl1a8_asr(component + (component < 0 ? 15 : 0), 4);
}

void ae_ovl1a8_BossUpdateWaves(AeRam *r, const AeOvl1a8Hooks *h) {
    uint32_t actor = AE_OVL1A8_ACTORS;
    uint32_t cursor = ae_rd32(r, ORDERING_TABLE_CURSOR);
    for (uint32_t index = 0; index < AE_OVL1A8_WAVE_COUNT; ++index) {
        uint32_t wave = ovl1a8_wave(index);
        uint32_t state = ae_rd8(r, wave + WAVE_STATE);
        int expanding_ring = state == 1u;
        uint32_t age = ae_rd8(r, wave + WAVE_AGE);
        if (state == 0) continue;

        if (state == 1) {
            int32_t ring_phase = ae_rds32(r, wave + WAVE_RING_PHASE);
            int32_t ring_step = ae_rds16(r, wave + WAVE_RING_STEP);
            ring_phase = ovl1a8_wave_wrap12(ring_phase + ring_step);
            ae_wr32(r, wave + WAVE_RING_PHASE, (uint32_t)ring_phase);

            int32_t old_ring_angle = ae_rds32(r, wave + WAVE_RING_ANGLE);
            int32_t ring_angle = old_ring_angle + 170;
            int32_t angle_wrap_input = ring_angle < 0
                                           ? old_ring_angle + 4265
                                           : ring_angle;
            ring_angle -= ovl1a8_asr(angle_wrap_input, 12) * 4096;
            ae_wr32(r, wave + WAVE_RING_ANGLE, (uint32_t)ring_angle);

            uint32_t triangle = age & 0x1Fu;
            if (triangle >= 16u) triangle = 31u - triangle;
            int32_t radius_factor = (int32_t)(triangle * 8u + 256u);
            int32_t x_offset = ovl1a8_asr(ae_sdk_rcos(r, ring_phase) * radius_factor, 12);
            int32_t y_offset = ovl1a8_asr(ae_sdk_rsin(r, ring_angle) * radius_factor, 13);
            int32_t z_offset = ovl1a8_asr(ae_sdk_rsin(r, ring_phase) * radius_factor, 12);
            ae_wr16(r, wave + WAVE_POS_X,
                    ae_rd16(r, actor + ACTOR_POSITION_X) + (uint32_t)x_offset);
            ae_wr16(r, wave + WAVE_POS_Y,
                    ae_rd16(r, actor + ACTOR_POSITION_Y) - (uint32_t)(y_offset + 256));
            ae_wr16(r, wave + WAVE_POS_Z,
                    ae_rd16(r, actor + ACTOR_POSITION_Z) + (uint32_t)z_offset);

            ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 203,
                                wave + WAVE_POS_X, BOSS_EFFECT_GROUPS, 0);
            if (age >= 129u) {
                ae_wr8(r, wave + WAVE_STATE, 2);
                int32_t dx = ae_rds16(r, wave + WAVE_POS_X) -
                             ae_rds16(r, actor + ACTOR_POSITION_X);
                int32_t dy = ae_rds16(r, wave + WAVE_POS_Y) -
                             ae_rds16(r, actor + ACTOR_POSITION_Y);
                int32_t dz = ae_rds16(r, wave + WAVE_POS_Z) -
                             ae_rds16(r, actor + ACTOR_POSITION_Z);
                ae_wr16(r, wave + WAVE_VELOCITY_X, (uint32_t)ovl1a8_wave_div8(dx));
                ae_wr16(r, wave + WAVE_VELOCITY_Y,
                        (uint32_t)(ovl1a8_wave_div8(dy) - 48));
                ae_wr16(r, wave + WAVE_VELOCITY_Z, (uint32_t)ovl1a8_wave_div8(dz));
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 143);
                if (ae_rd8(r, BOSS_WAVE_SFX_FLAG) != 0) {
                    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 178);
                    ae_wr8(r, BOSS_WAVE_SFX_FLAG, 0);
                }
            }
        } else if (state == 2) {
            uint32_t player = BOSS_PLAYER;
            int32_t velocity_x = ae_rds16(r, wave + WAVE_VELOCITY_X);
            int32_t velocity_y = ae_rds16(r, wave + WAVE_VELOCITY_Y);
            int32_t velocity_z = ae_rds16(r, wave + WAVE_VELOCITY_Z);

            ae_wr16(r, wave + WAVE_POS_X,
                    ae_rd16(r, wave + WAVE_POS_X) + (uint32_t)velocity_x);
            ae_wr16(r, wave + WAVE_POS_Y,
                    ae_rd16(r, wave + WAVE_POS_Y) + (uint32_t)velocity_y);
            ae_wr16(r, wave + WAVE_POS_Z,
                    ae_rd16(r, wave + WAVE_POS_Z) + (uint32_t)velocity_z);
            velocity_x = ovl1a8_wave_div8(velocity_x * 7);
            velocity_y = ovl1a8_wave_div8(velocity_y * 7);
            velocity_z = ovl1a8_wave_div8(velocity_z * 7);
            ae_wr16(r, wave + WAVE_VELOCITY_X, (uint32_t)velocity_x);
            ae_wr16(r, wave + WAVE_VELOCITY_Y, (uint32_t)velocity_y);
            ae_wr16(r, wave + WAVE_VELOCITY_Z, (uint32_t)velocity_z);

            uint32_t vector = WAVE_VECTOR_TEMP;
            ae_wr16(r, vector,
                    ae_rd16(r, player + ACTOR_POSITION_X) -
                    ae_rd16(r, wave + WAVE_POS_X));
            ae_wr16(r, vector + 2u,
                    ae_rd16(r, player + ACTOR_POSITION_Y) -
                    ae_rd16(r, wave + WAVE_POS_Y) - 16u);
            ae_wr16(r, vector + 4u,
                    ae_rd16(r, player + ACTOR_POSITION_Z) -
                    ae_rd16(r, wave + WAVE_POS_Z));
            (void)ae_sdk_VectorNormalSS(r, vector, vector);
            if (ae_ram_failed(r)) return;

            int32_t direction_x = ae_rds16(r, vector);
            int32_t direction_y = ae_rds16(r, vector + 2u);
            int32_t direction_z = ae_rds16(r, vector + 4u);
            velocity_x = ae_rds16(r, wave + WAVE_VELOCITY_X) +
                         ovl1a8_wave_div256(direction_x);
            velocity_z = ae_rds16(r, wave + WAVE_VELOCITY_Z) +
                         ovl1a8_wave_div256(direction_z);
            ae_wr16(r, wave + WAVE_VELOCITY_X, (uint32_t)velocity_x);
            ae_wr16(r, wave + WAVE_VELOCITY_Z, (uint32_t)velocity_z);
            velocity_x = ae_rds16(r, wave + WAVE_VELOCITY_X);
            velocity_z = ae_rds16(r, wave + WAVE_VELOCITY_Z);

            int32_t dx = ae_rds16(r, player + ACTOR_POSITION_X) -
                         ae_rds16(r, wave + WAVE_POS_X);
            int32_t dz = ae_rds16(r, player + ACTOR_POSITION_Z) -
                         ae_rds16(r, wave + WAVE_POS_Z);
            uint32_t distance_squared =
                (uint32_t)((int64_t)dx * dx) + (uint32_t)((int64_t)dz * dz);
            if (distance_squared <= 0xFFFFu || age >= 181u) {
                velocity_y = ae_rds16(r, wave + WAVE_VELOCITY_Y) + 8;
            } else {
                velocity_y = ae_rds16(r, wave + WAVE_VELOCITY_Y) +
                             ovl1a8_wave_div256(direction_y);
            }
            ae_wr16(r, wave + WAVE_VELOCITY_Y, (uint32_t)velocity_y);

            velocity_x = velocity_x < -48 ? -48 : (velocity_x > 48 ? 48 : velocity_x);
            velocity_z = velocity_z < -48 ? -48 : (velocity_z > 48 ? 48 : velocity_z);
            ae_wr16(r, wave + WAVE_VELOCITY_X, (uint32_t)velocity_x);
            ae_wr16(r, wave + WAVE_VELOCITY_Z, (uint32_t)velocity_z);
            ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 204,
                                wave + WAVE_POS_X, BOSS_EFFECT_GROUPS, 0);

            if (ae_rds16(r, wave + WAVE_POS_Y) >= -2592) {
                ae_wr8(r, wave + WAVE_STATE, 0);
                ae_wr16(r, wave + WAVE_POS_Y, (uint32_t)-2592);
                ae_CameraShake(r, 0);
                ae_SpawnParticle(r, h ? h->spawn : NULL, 274,
                                 wave + WAVE_POS_X);
                ae_TrailSpawn(r, wave + WAVE_POS_X, 5, 300, 0);
                ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                      wave + WAVE_POS_X, BOSS_WAVE_DROP_DEF,
                                      1, 0, 0);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 147);
                uint32_t drop_seed = (ae_rd16(r, AE_SPAWN_RAND_SEED) * 5u + 1u) & 0xFFFFu;
                ae_wr16(r, AE_SPAWN_RAND_SEED, drop_seed);
                if (((drop_seed >> 8) & 7u) == 0)
                    ae_ovl1a8_BossSpawnDropPickups(r, h, wave + WAVE_POS_X);
                continue;
            }

            if (ae_rds16(r, wave + WAVE_POS_Y) < -2592) {
                uint32_t impact_object = ae_rd32(r, wave);
                if (impact_object != 0 &&
                    (ae_rd32(r, impact_object + 44u) >> 28) == 6u) {
                    ae_wr8(r, wave + WAVE_STATE, 0);
                    ae_wr16(r, wave + WAVE_POS_Y, (uint32_t)-2592);
                    ae_CameraShake(r, 0);
                    ae_SpawnParticle(r, h ? h->spawn : NULL, 274,
                                     wave + WAVE_POS_X);
                    ae_TrailSpawn(r, wave + WAVE_POS_X, 5, 300, 0);
                    ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                          wave + WAVE_POS_X, BOSS_WAVE_DROP_DEF,
                                          1, 0, 0);
                    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 147);
                    uint32_t drop_seed = (ae_rd16(r, AE_SPAWN_RAND_SEED) * 5u + 1u) & 0xFFFFu;
                    ae_wr16(r, AE_SPAWN_RAND_SEED, drop_seed);
                    if (((drop_seed >> 8) & 7u) == 0)
                        ae_ovl1a8_BossSpawnDropPickups(r, h,
                                                       wave + WAVE_POS_X);
                    continue;
                } else {
                    ae_ovl1a8_BossSpawnEffectNearWrap(r, h, wave, actor,
                                                       80, 512);
                }
            }
        }

        age = ae_rd8(r, wave + WAVE_AGE);
        uint32_t color = age + 24u;
        if (color >= 97u) color = 96u;
        if (expanding_ring) ae_wr8(r, wave + WAVE_COLOR, color);

        uint32_t radius;
        if (age < 16u) radius = (16u - age) * 8u + 16u;
        else {
            radius = age + 16u;
            if (radius >= 97u) radius = 96u;
        }
        if (expanding_ring) ae_wr16(r, wave + WAVE_RADIUS, radius);
        if (age < 255u) ++age;
        ae_wr8(r, wave + WAVE_AGE, age);
        state = ae_rd8(r, wave + WAVE_STATE);
        uint32_t ring_color = ae_rd8(r, wave + WAVE_COLOR);
        uint32_t rgba = (ring_color << 16) | (ring_color << 8) | ring_color;
        uint32_t ring_args[4] = {
            wave + WAVE_POS_X,
            (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_RADIUS),
            state == 2 ? 5u : 3u,
            rgba
        };
        (void)ovl1a8_render(r, h, "EffectRingDraw", RENDER_EFFECT_RING,
                            4, ring_args);
        ring_args[1] = (uint32_t)(int32_t)(ae_rds16(r, wave + WAVE_RADIUS) / 2);
        ring_args[2] = 1;
        (void)ovl1a8_render(r, h, "EffectRingDraw", RENDER_EFFECT_RING,
                            4, ring_args);

        ae_wr32(r, WAVE_SHADOW_MATRIX, 0);
        ae_wr32(r, WAVE_SHADOW_MATRIX + 4u, 0);
        ae_wr32(r, WAVE_SHADOW_MATRIX + 8u, 0);
        ae_wr32(r, WAVE_SHADOW_MATRIX + 12u, 0);
        ae_wr32(r, WAVE_SHADOW_POSITION,
                (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_POS_X));
        ae_wr32(r, WAVE_SHADOW_POSITION + 4u,
                (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_POS_Y));
        ae_wr32(r, WAVE_SHADOW_POSITION + 8u,
                (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_POS_Z));
        ae_wr32(r, WAVE_SHADOW_POSITION + 12u, 0);
        uint32_t shadow_args[5] = {
            (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_POS_X),
            (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_POS_Y),
            (uint32_t)(int32_t)ae_rds16(r, wave + WAVE_POS_Z),
            (uint32_t)-2576, 64
        };
        (void)ovl1a8_render(r, h, "DrawShadow", RENDER_SHADOW,
                            5, shadow_args);
        if (ae_ram_failed(r)) return;
    }

    ae_wr32(r, ORDERING_TABLE_CURSOR, cursor);
}

void ae_ovl1a8_BossClearDebris(AeRam *r) {
    for (uint32_t i = 0; i < AE_OVL1A8_DEBRIS_COUNT; ++i)
        ae_wr8(r, ovl1a8_debris(i) + DEBRIS_LIFE, 0);
}

void ae_ovl1a8_BossSpawnEffectGroupsN(AeRam *r, const AeOvl1a8Hooks *h,
                                      uint32_t position) {
    const uint32_t groups = BOSS_EFFECT_GROUPS;
    for (uint32_t i = 0; i < 16u; ++i) {
        ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 201, position, groups, 0);
        ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 202, position, groups, 0);
    }
}

void ae_ovl1a8_BossInit(AeRam *r, const AeOvl1a8Hooks *h) {
    ae_wr16(r, GLOBAL_EVENT_FLAG, 0);
    if (ae_ProgressGetByte(r, 12u) == 5u) {
        ae_wr8(r, BOSS_INIT_EVENT_FLAG, 1);
        return;
    }

    const uint32_t positions = BOSS_DIRECTION_TABLE;
    for (uint32_t i = 0; i < 1u; ++i) {
        uint32_t actor = AE_OVL1A8_ACTORS + i * BOSS_ACTOR_SIZE;
        ae_ovl1a8_BossModelInit(r, h, i, actor, actor + ACTOR_ANIMATION_TABLE);

        uint32_t source = positions;
        ae_wr32(r, actor + ACTOR_POSITION_X, (uint32_t)ae_rds16(r, source + MODEL_DIRECTION_X));
        ae_wr32(r, actor + ACTOR_POSITION_Y, (uint32_t)ae_rds16(r, source + MODEL_DIRECTION_Y));
        ae_wr32(r, actor + ACTOR_POSITION_Z, (uint32_t)ae_rds16(r, source + MODEL_DIRECTION_Z));
        ae_wr8(r, actor + ACTOR_INIT_ANIM_FLAGS, 2);
        ae_wr16(r, actor + ACTOR_INIT_ANIM_HEIGHT, 90);
        ae_wr8(r, actor + ACTOR_INIT_MODEL_FLAGS, 1);
        ae_wr16(r, actor + ACTOR_INIT_MODEL_HEIGHT, 0);
        ae_wr8(r, actor + ACTOR_INIT_MODEL_TYPE, 4);
        ae_wr8(r, actor + ACTOR_INIT_MODEL_STATE, 1);
        ae_wr16(r, actor + ACTOR_INIT_ANGLE_X, 0);
        ae_wr16(r, actor + ACTOR_INIT_ANGLE_Y, 0);
        ae_wr16(r, actor + ACTOR_INIT_ANGLE_Z, 0x1000);
        (void)ae_ovl1a8_BossSetAnim(r, actor, 0);

        (void)ae_CollisionBodyRegister(r, h ? h->body : NULL, 5,
                                       actor + ACTOR_BODY, actor);
        ae_wr16(r, actor + ACTOR_INIT_BODY_Y, (uint32_t)-15872);
        ae_wr16(r, actor + ACTOR_INIT_COLLISION_HEIGHT, 180);
        ae_wr16(r, actor + ACTOR_INIT_COLLISION_FLAGS, 1);
        ae_wr16(r, actor + ACTOR_INIT_COLLISION_RADIUS, 800);
    }

    ae_ovl1a8_BossClearFlag(r, 1);
    ae_ovl1a8_BossClearEntries(r);
    ae_ovl1a8_BossClearDebris(r);
    ae_ovl1a8_BossClearRipples(r);
    ae_ovl1a8_BossResetLoopHandle(r);
    ae_ovl1a8_BossClearSparks(r);
    ae_wr8(r, BOSS_FLAGS, 1);
    ae_wr8(r, BOSS_TIMER, 0);
    ae_wr8(r, BOSS_MODE, 1);
    ae_wr16(r, BOSS_CAMERA_MOVE_COUNT, 100);
}

void ae_ovl1a8_BossUpdateCameraSequence(AeRam *r, const AeOvl1a8Hooks *h,
                                        uint32_t camera_slot) {
    if (ae_rd8(r, BOSS_TIMER) != 0u) {
        ae_ovl1a8_BossUpdateWakeSequence(r, h, camera_slot);
        return;
    }

    uint32_t actor = AE_OVL1A8_ACTORS;
    uint32_t player = BOSS_PLAYER;
    int32_t dx = (int32_t)(uint16_t)ae_rd16(r, actor + ACTOR_POSITION_X) -
                 (int32_t)(uint16_t)ae_rd16(r, player + ACTOR_POSITION_X);
    int32_t dy = (int32_t)(uint16_t)ae_rd16(r, actor + ACTOR_POSITION_Y) -
                 (int32_t)(uint16_t)ae_rd16(r, player + ACTOR_POSITION_Y);
    int32_t dz = (int32_t)(uint16_t)ae_rd16(r, actor + ACTOR_POSITION_Z) -
                 (int32_t)(uint16_t)ae_rd16(r, player + ACTOR_POSITION_Z);
    ae_wr16(r, CAMERA_TEMP_VECTOR, (uint32_t)dx);
    ae_wr16(r, CAMERA_TEMP_VECTOR + 2u, (uint32_t)dy);
    ae_wr16(r, CAMERA_TEMP_VECTOR + 4u, (uint32_t)dz);
    (void)ae_sdk_VectorNormalSS(r, CAMERA_TEMP_VECTOR, CAMERA_TEMP_VECTOR);

    dx = ae_rds16(r, CAMERA_TEMP_VECTOR);
    dz = ae_rds16(r, CAMERA_TEMP_VECTOR + 4u);
    int32_t eye_x = (int32_t)ae_rd16(r, player + ACTOR_POSITION_X) -
                    ovl1a8_asr(dx * 381, 10);
    int32_t eye_y = (int32_t)ae_rd16(r, player + ACTOR_POSITION_Y) - 1324;
    int32_t eye_z = (int32_t)ae_rd16(r, player + ACTOR_POSITION_Z) -
                    ovl1a8_asr(dz * 381, 10);
    uint32_t state = ae_rd8(r, BOSS_PHASE);
    if (state >= 22u && state <= 26u) {
        eye_x = (int32_t)ae_rd16(r, player + ACTOR_POSITION_X) - ovl1a8_asr(dx, 3);
        eye_y = (int32_t)ae_rd16(r, player + ACTOR_POSITION_Y) - 300;
        eye_z = (int32_t)ae_rd16(r, player + ACTOR_POSITION_Z) - ovl1a8_asr(dz, 3);
    }

    ae_wr16(r, AE_SCRATCH_BASE, (uint32_t)eye_x);
    ae_wr16(r, AE_SCRATCH_BASE + 2u, (uint32_t)eye_y);
    ae_wr16(r, AE_SCRATCH_BASE + 4u, (uint32_t)eye_z);
    ae_wr16(r, AE_SCRATCH_BASE + 8u,
            ae_rd16(r, actor + ACTOR_POSITION_X));
    ae_wr16(r, AE_SCRATCH_BASE + 10u,
            ae_rd16(r, actor + ACTOR_POSITION_Y));
    ae_wr16(r, AE_SCRATCH_BASE + 12u,
            ae_rd16(r, actor + ACTOR_POSITION_Z));
    ae_CameraSlotSmoothTarget(r, h ? h->camera : NULL, camera_slot,
                              12, 16, 12);
    ae_CameraSlotSmoothEye(r, h ? h->camera : NULL, camera_slot,
                           12, 16, 12);
}

int32_t ae_ovl1a8_BossUpdate(AeRam *r, const AeOvl1a8Hooks *h,
                             uint32_t camera_slot) {
    uint32_t phase = ae_rd8(r, BOSS_PHASE);
    if (phase >= 20u) {
        ae_ovl1a8_BossUpdateCameraSequence(r, h, camera_slot);
        return 1;
    }

    uint32_t boss_actor = AE_OVL1A8_ACTORS;
    uint32_t player_actor = BOSS_PLAYER;
    if (ae_rd8(r, BOSS_FLAGS) != 0u &&
        ae_rds32(r, player_actor + ACTOR_POSITION_Y) < -2592) {
        uint32_t move_count = ae_rd16(r, BOSS_CAMERA_MOVE_COUNT);
        int32_t angle = (int32_t)(move_count * 48u) + 1024;
        int32_t quotient = ovl1a8_asr(angle, 12);
        angle -= quotient * 4096;

        int32_t radius = (int32_t)(move_count * 32u) + 800;
        int32_t player_x = (int32_t)ae_rd16(r, player_actor + ACTOR_POSITION_X);
        int32_t player_y = (int32_t)ae_rd16(r, player_actor + ACTOR_POSITION_Y);
        int32_t eye_x = player_x +
            ovl1a8_asr(ae_sdk_rcos(r, (int16_t)angle) * radius, 12);
        int32_t eye_y = player_y - 64 - (radius - 300);
        int32_t eye_z = player_x +
            ovl1a8_asr(ae_sdk_rsin(r, (int16_t)angle) * radius, 12);

        ae_wr16(r, CAMERA_TEMP_VECTOR, (uint32_t)eye_x);
        ae_wr16(r, CAMERA_TEMP_VECTOR + 2u, (uint32_t)eye_y);
        ae_wr16(r, CAMERA_TEMP_VECTOR + 4u, (uint32_t)eye_z);
        ae_wr16(r, CAMERA_TEMP_VECTOR + 8u, (uint32_t)player_x);
        ae_wr16(r, CAMERA_TEMP_VECTOR + 10u, (uint32_t)(player_y - 64));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 12u, (uint32_t)player_x);
        ae_CameraSlotSmoothTarget(r, h ? h->camera : NULL, camera_slot,
                                  2, 2, 2);
        ae_CameraSlotSmoothEye(r, h ? h->camera : NULL, camera_slot,
                               2, 2, 2);

        if (move_count != 0u) {
            --move_count;
            ae_wr16(r, BOSS_CAMERA_MOVE_COUNT, move_count);
        }
        if (move_count < 51u) ae_wr8(r, CAMERA_TRANSITION_MODE, 4);
        return 1;
    }

    int32_t dx = (int32_t)(uint16_t)ae_rd16(r, boss_actor + ACTOR_POSITION_X) -
                 (int32_t)(uint16_t)ae_rd16(r, player_actor + ACTOR_POSITION_X);
    int32_t dy = (int32_t)(uint16_t)ae_rd16(r, boss_actor + ACTOR_POSITION_Y) -
                 (int32_t)(uint16_t)ae_rd16(r, player_actor + ACTOR_POSITION_Y);
    int32_t dz = (int32_t)(uint16_t)ae_rd16(r, boss_actor + ACTOR_POSITION_Z) -
                 (int32_t)(uint16_t)ae_rd16(r, player_actor + ACTOR_POSITION_Z);
    ae_wr16(r, BOSS_UPDATE_VECTOR_TEMP, (uint32_t)dx);
    ae_wr16(r, BOSS_UPDATE_VECTOR_TEMP + 2u, (uint32_t)dy);
    ae_wr16(r, BOSS_UPDATE_VECTOR_TEMP + 4u, (uint32_t)dz);
    (void)ae_sdk_VectorNormalSS(r, BOSS_UPDATE_VECTOR_TEMP,
                                BOSS_UPDATE_VECTOR_TEMP);

    dx = ae_rds16(r, BOSS_UPDATE_VECTOR_TEMP);
    dy = ae_rds16(r, BOSS_UPDATE_VECTOR_TEMP + 2u);
    dz = ae_rds16(r, BOSS_UPDATE_VECTOR_TEMP + 4u);
    int32_t target_x = (int32_t)(uint16_t)ae_rd16(
        r, player_actor + ACTOR_POSITION_X) - ovl1a8_asr(dx * 45, 8);
    int32_t target_y = (int32_t)(uint16_t)ae_rd16(
        r, player_actor + ACTOR_POSITION_Y) - 260;
    int32_t target_z = (int32_t)(uint16_t)ae_rd16(
        r, player_actor + ACTOR_POSITION_Z) - ovl1a8_asr(dz * 45, 8);
    ae_wr16(r, CAMERA_TEMP_VECTOR, (uint32_t)target_x);
    ae_wr16(r, CAMERA_TEMP_VECTOR + 2u, (uint32_t)target_y);
    ae_wr16(r, CAMERA_TEMP_VECTOR + 4u, (uint32_t)target_z);
    ae_wr16(r, CAMERA_TEMP_VECTOR + 8u,
            ae_rd16(r, boss_actor + ACTOR_POSITION_X));
    ae_wr16(r, CAMERA_TEMP_VECTOR + 10u,
            ae_rd16(r, boss_actor + ACTOR_POSITION_Y));
    ae_wr16(r, CAMERA_TEMP_VECTOR + 12u,
            ae_rd16(r, boss_actor + ACTOR_POSITION_Z));

    if (phase == 12u) {
        int32_t offset_x = dx * 225;
        int32_t offset_z = dz * 225;
        int32_t adjust_x = ovl1a8_asr(offset_x, 10);
        int32_t adjust_z = ovl1a8_asr(offset_z, 10);
        ae_wr16(r, CAMERA_TEMP_VECTOR,
                (uint32_t)((int32_t)(uint16_t)ae_rd16(
                    r, boss_actor + ACTOR_POSITION_X) - adjust_x));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 2u,
                (uint32_t)((int32_t)(uint16_t)ae_rd16(
                    r, boss_actor + ACTOR_POSITION_Y) - 1524));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 4u,
                (uint32_t)((int32_t)(uint16_t)ae_rd16(
                    r, boss_actor + ACTOR_POSITION_Z) - adjust_z));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 8u,
                ae_rd16(r, boss_actor + ACTOR_POSITION_X));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 10u,
                ae_rd16(r, boss_actor + ACTOR_POSITION_Y) - 300u);
        ae_wr16(r, CAMERA_TEMP_VECTOR + 12u,
                ae_rd16(r, boss_actor + ACTOR_POSITION_Z));
    } else if (phase == 13u) {
        int32_t x_step = dx < 0 ? ovl1a8_asr(dx + 3, 2) : ovl1a8_asr(dx, 2);
        int32_t z_step = dz < 0 ? ovl1a8_asr(dz + 3, 2) : ovl1a8_asr(dz, 2);
        ae_wr16(r, CAMERA_TEMP_VECTOR,
                (uint32_t)((int32_t)(uint16_t)ae_rd16(
                    r, player_actor + ACTOR_POSITION_X) - x_step));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 4u,
                (uint32_t)((int32_t)(uint16_t)ae_rd16(
                    r, player_actor + ACTOR_POSITION_Z) - z_step));
        int32_t eye_x = (3 * (int32_t)ae_rd32(r, player_actor + ACTOR_POSITION_X) +
                         (int32_t)ae_rd32(r, boss_actor + ACTOR_POSITION_X));
        int32_t eye_z = (3 * (int32_t)ae_rd32(r, player_actor + ACTOR_POSITION_Z) +
                         (int32_t)ae_rd32(r, boss_actor + ACTOR_POSITION_Z));
        if (eye_x < 0) eye_x += 3;
        if (eye_z < 0) eye_z += 3;
        ae_wr16(r, CAMERA_TEMP_VECTOR + 2u,
                (uint32_t)(ae_rds16(r, CAMERA_TEMP_VECTOR + 2u) - 2424));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 8u, (uint32_t)ovl1a8_asr(eye_x, 2));
        ae_wr16(r, CAMERA_TEMP_VECTOR + 10u,
                ae_rd16(r, boss_actor + ACTOR_POSITION_Y) + 2400u);
        ae_wr16(r, CAMERA_TEMP_VECTOR + 12u, (uint32_t)ovl1a8_asr(eye_z, 2));
    }

    if (ae_rds8(r, CAMERA_AREA_STATE) == 3 &&
        ae_rd8(r, CAMERA_ACTIVE_STATE) != 1u) {
        ae_CameraFollowGadgetVariant(r, h ? h->camera : NULL, camera_slot);
    }
    ae_CameraSlotSmoothTarget(r, h ? h->camera : NULL, camera_slot,
                              8, 16, 8);
    ae_CameraSlotSmoothEye(r, h ? h->camera : NULL, camera_slot,
                           8, 16, 8);
    return 1;
}

void ae_ovl1a8_BossWake(AeRam *r, const AeOvl1a8Hooks *h) {
    if (ae_rd8(r, BOSS_TIMER) != 0u) return;

    uint32_t actor_body = AE_OVL1A8_ACTORS + ACTOR_BODY;
    (void)ae_CollisionBodyUnregister(r, h ? h->body : NULL, actor_body);
    ae_wr8(r, BOSS_TIMER, 1);
    ae_wr8(r, BOSS_STATE, 2);
    ae_wr16(r, BOSS_SUBSTATE, 16);
    ae_PadRumble(r, h ? h->camupd : NULL, 1, 200, 3);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 55);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 56);
}

void ae_ovl1a8_BossUpdateWakeSequence(AeRam *r, const AeOvl1a8Hooks *h,
                                      uint32_t camera_slot) {
    uint32_t actor = AE_OVL1A8_ACTORS;
    uint32_t player = BOSS_PLAYER;
    uint32_t state = ae_rd8(r, BOSS_STATE);
    uint32_t substate = ae_rd16(r, BOSS_SUBSTATE);

    if (substate == 16u) {
        int32_t facing_x = ovl1a8_wake_direction_component(r,
                                                            PLAYER_FACING_COMPONENT_X);
        int32_t facing_z = ovl1a8_wake_direction_component(r,
                                                            PLAYER_FACING_COMPONENT_Z);
        ae_wr32(r, actor + ACTOR_POSITION_X,
                ae_rd32(r, player + ACTOR_POSITION_X) + (uint32_t)facing_x);
        ae_wr32(r, actor + ACTOR_POSITION_Y,
                ae_rd32(r, player + ACTOR_POSITION_Y));
        ae_wr32(r, actor + ACTOR_POSITION_Z,
                ae_rd32(r, player + ACTOR_POSITION_Z) + (uint32_t)facing_z);
        ae_wr16(r, WAKE_POSITION, ae_rd16(r, actor + ACTOR_POSITION_X));
        ae_wr16(r, WAKE_POSITION + 2u, ae_rd16(r, actor + ACTOR_POSITION_Y));
        ae_wr16(r, WAKE_POSITION + 4u, ae_rd16(r, actor + ACTOR_POSITION_Z));
        ae_SpawnParticle(r, h ? h->spawn : NULL, 275, WAKE_POSITION);
        ae_EffectSpawnFromDef(r, h ? h->spawn : NULL, WAKE_POSITION,
                              WAKE_EFFECTS, 1, 0, 0);
        ae_EffectSpawnFromDef(r, h ? h->spawn : NULL, WAKE_POSITION,
                              WAKE_EFFECT_DEFINITION, 2, 3, 0);
        ae_CameraShake(r, 0);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 54);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 57);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 59);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);

        uint32_t direction = ae_rd8(r, BOSS_STATE);
        uint32_t direction_record = WAKE_DIRECTION_TABLE + direction * 8u;
        ae_wr16(r, camera_slot + CAMERA_CURRENT_X,
                ae_rd16(r, actor + ACTOR_POSITION_X) + ae_rd16(r, direction_record));
        ae_wr16(r, camera_slot + CAMERA_CURRENT_Y,
                ae_rd16(r, actor + ACTOR_POSITION_Y) + ae_rd16(r, direction_record + WAKE_DIRECTION_Y));
        ae_wr16(r, camera_slot + CAMERA_CURRENT_Z,
                ae_rd16(r, actor + ACTOR_POSITION_Z) + ae_rd16(r, direction_record + WAKE_DIRECTION_Z));
        ae_wr16(r, camera_slot + CAMERA_TARGET_X, ae_rd16(r, actor + ACTOR_POSITION_X));
        ae_wr16(r, camera_slot + CAMERA_TARGET_Y,
                ae_rd16(r, actor + ACTOR_POSITION_Y) - 128u);
        ae_wr16(r, camera_slot + CAMERA_TARGET_Z, ae_rd16(r, actor + ACTOR_POSITION_Z));
    }

    if (state != 0u) {
        int32_t x = ae_rds16(r, camera_slot + CAMERA_CURRENT_X);
        int32_t y = ae_rds16(r, camera_slot + CAMERA_CURRENT_Y);
        int32_t z = ae_rds16(r, camera_slot + CAMERA_CURRENT_Z);
        int32_t next_x = 7 * x + ae_rds16(r, camera_slot + CAMERA_TARGET_X);
        int32_t next_y = 7 * y + ae_rds16(r, camera_slot + CAMERA_TARGET_Y);
        int32_t next_z = 7 * z + ae_rds16(r, camera_slot + CAMERA_TARGET_Z);
        ae_wr16(r, camera_slot + CAMERA_CURRENT_X,
                (uint32_t)ovl1a8_asr(next_x + (next_x < 0 ? 7 : 0), 3));
        ae_wr16(r, camera_slot + CAMERA_CURRENT_Y,
                (uint32_t)ovl1a8_asr(next_y + (next_y < 0 ? 7 : 0), 3));
        ae_wr16(r, camera_slot + CAMERA_CURRENT_Z,
                (uint32_t)ovl1a8_asr(next_z + (next_z < 0 ? 7 : 0), 3));
    }

    uint32_t frame_count = ae_rd16(r, AE_OVL1A8_BOSS + BOSS_FRAME_COUNT_OFFSET);
    if (frame_count < 50u && ae_rd16(r, BOSS_SUBSTATE) % 5u == 4u)
        ae_PadRumble(r, h ? h->camupd : NULL, 1, 200, 3);

    substate = (ae_rd16(r, BOSS_SUBSTATE) - 1u) & 0xFFFFu;
    ae_wr16(r, BOSS_SUBSTATE, substate);
    if (substate == 0u) {
        ae_wr16(r, BOSS_SUBSTATE, 16u);
        state = (state - 1u) & 0xFFu;
        ae_wr8(r, BOSS_STATE, state);
        if (state == 0u) {
            ae_EventCamStartRandom(r, h ? h->player1 : NULL);
            int32_t facing_x = ovl1a8_wake_direction_component(
                r, PLAYER_FACING_COMPONENT_X);
            int32_t facing_z = ovl1a8_wake_direction_component(
                r, PLAYER_FACING_COMPONENT_Z);
            ae_wr32(r, WAKE_FLASH_POSITION + POSITION32_X,
                    ae_rd32(r, player + ACTOR_POSITION_X) + (uint32_t)facing_x);
            ae_wr32(r, WAKE_FLASH_POSITION + POSITION32_Y,
                    ae_rd32(r, player + ACTOR_POSITION_Y));
            ae_wr32(r, WAKE_FLASH_POSITION + POSITION32_Z,
                    ae_rd32(r, player + ACTOR_POSITION_Z) + (uint32_t)facing_z);
            ae_GadgetFlashSet(r, WAKE_FLASH_POSITION);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);
            ae_CameraShake(r, 1);
            ae_wr16(r, BOSS_SUBSTATE, 0xFFFFu);
            ae_wr16(r, AE_OVL1A8_BOSS + BOSS_FRAME_COUNT_OFFSET, 0);
        }
    }

    if (ae_rd8(r, BOSS_STATE) == 0u) {
        frame_count = (ae_rd16(r, AE_OVL1A8_BOSS + BOSS_FRAME_COUNT_OFFSET) + 1u) & 0xFFFFu;
        ae_wr16(r, AE_OVL1A8_BOSS + BOSS_FRAME_COUNT_OFFSET, frame_count);
        substate = ae_rd16(r, BOSS_SUBSTATE);
        if (substate == 0xFFD3u) {
            (void)ae_StageRequest(r, h ? h->stage : NULL, 29,
                                  ae_rd16(r, STAGE_IDENTIFIER_A),
                                  ae_rd16(r, STAGE_IDENTIFIER_B));
        } else if (substate == 0xFFD5u) {
            ae_wr16(r, GLOBAL_EVENT_FLAG, 1);
        } else if (substate == 0xFFDFu) {
            ae_wr8(r, BOSS_MODE, 0);
        }
    }
}

void ae_ovl1a8_BossStart(AeRam *r, const AeOvl1a8Hooks *h) {
    uint32_t actor = AE_OVL1A8_ACTORS;
    ae_ovl1a8_BossModelInit(r, h, 1, actor, actor + ACTOR_ANIMATION_TABLE);
    ae_wr32(r, actor + ACTOR_POSITION_Y, (uint32_t)-2560);
    ae_wr8(r, actor + ACTOR_SEQUENCE_HIT_COUNT, 5);
    ae_wr8(r, actor + ACTOR_SEQUENCE_MODE, 1);
    ae_wr16(r, actor + ACTOR_SEQUENCE_COUNTER, 15);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, 120);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TARGET_X, (uint32_t)-256);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TARGET_Z, (uint32_t)-256);
    ae_wr16(r, actor + ACTOR_COLLISION_HEIGHT, 128);
    ae_wr16(r, actor + ACTOR_SEQUENCE_MODEL_SCALE, 0);
    ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 0);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_Z, 0);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_X, 0);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TARGET_COOLDOWN, 0);
    ae_wr16(r, actor + ACTOR_COLLISION_RADIUS, 256);
    ae_ovl1a8_BossClearTable8(r);
    ae_wr8(r, BOSS_FLAGS, 0);
}

void ae_ovl1a8_BossSlamImpact(AeRam *r, const AeOvl1a8Hooks *h,
                              uint32_t actor, uint32_t position) {
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, 360);
    ae_wr16(r, actor + ACTOR_SEQUENCE_COUNTER, 0);
    ae_wr16(r, actor + ACTOR_COLLISION_HEIGHT, 128);
    ae_ovl1a8_BossSpawnGroundRipples(r, h, position, 32);
    ae_SpawnParticle(r, h ? h->spawn : NULL, 274, position);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 146);
    ae_CameraShake(r, 1);
}

void ae_ovl1a8_BossDeathBurst(AeRam *r, const AeOvl1a8Hooks *h,
                              uint32_t actor, uint32_t position) {
    for (uint32_t i = 0; i < 3u; ++i)
        ae_SpawnRandomParticle(r, position, 0, 0x101u, 2048);
    ae_ovl1a8_BossSpawnEffectGroupsN(r, h, position);
    ae_wr16(r, actor + ACTOR_SEQUENCE_COUNTER, 15);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, 30);
    ae_ovl1a8_BossAddSparks(r, actor, 0);
    ae_CameraShake(r, 1);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 39);
    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 146);
}

static int32_t ovl1a8_half_toward_zero(int32_t value) {
    return ovl1a8_asr(value + (value < 0), 1);
}

static int32_t ovl1a8_quarter_toward_zero(int32_t value) {
    return ovl1a8_asr(value + (value < 0 ? 3 : 0), 2);
}

void ae_ovl1a8_BossSpawnGroundRipples(AeRam *r, const AeOvl1a8Hooks *h,
                                      uint32_t position,
                                      uint32_t max_ripples) {
    int32_t x = ae_rds16(r, position);
    int32_t y = ae_rds16(r, position + POSITION16_Y);
    int32_t z = ae_rds16(r, position + POSITION16_Z);
    int32_t ground_y = ae_GetGroundHeight(r, h ? h->world : NULL, x, y, z);
    if (ae_ram_failed(r)) return;

    uint32_t seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
    uint32_t spawned = 0;
    for (uint32_t i = 0; i < AE_OVL1A8_RIPPLE_COUNT && spawned < max_ripples; ++i) {
        uint32_t ripple = ovl1a8_ripple(i);
        if (ae_rd8(r, ripple + RIPPLE_LIFE) != 0) continue;

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, ripple + RIPPLE_LIFE, 40u + ((seed >> 8) & 31u));
        ae_wr16(r, ripple + RIPPLE_GROUND_Y, (uint32_t)ground_y);

        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr16(r, ripple + RIPPLE_POS_X,
                (uint32_t)(x + ovl1a8_half_toward_zero((int32_t)(seed >> 8) - 128)));
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr16(r, ripple + RIPPLE_POS_Y,
                (uint32_t)(y - ovl1a8_half_toward_zero((int32_t)(seed >> 8) - 128)));
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr16(r, ripple + RIPPLE_POS_Z,
                (uint32_t)(z + ovl1a8_half_toward_zero((int32_t)(seed >> 8) - 128)));
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, ripple + RIPPLE_VELOCITY_X,
                (uint32_t)ovl1a8_quarter_toward_zero((int32_t)(seed >> 8) - 128));
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, ripple + RIPPLE_VELOCITY_Y, (uint32_t)(-((seed >> 8) & 31u) - 24u));
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
        ae_wr8(r, ripple + RIPPLE_VELOCITY_Z,
                (uint32_t)ovl1a8_quarter_toward_zero((int32_t)(seed >> 8) - 128));
        ++spawned;
    }
}

void ae_ovl1a8_BossUpdateRipples(AeRam *r, const AeOvl1a8Hooks *h) {
    uint32_t old_seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
    uint32_t next_seed = (old_seed * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_SPAWN_RAND_SEED, next_seed);
    uint32_t init_args[2] = {old_seed, next_seed};
    (void)ovl1a8_render(r, h, "Area57_Boss_RippleMeshSetup",
                        BOSS_RIPPLE_SETUP_HOOK, 2, init_args);
    if (ae_ram_failed(r)) return;

    uint32_t cursor = ae_rd32(r, ORDERING_TABLE_CURSOR);
    for (uint32_t index = 0; index < AE_OVL1A8_RIPPLE_COUNT; ++index) {
        uint32_t ripple = ovl1a8_ripple(index);
        uint32_t life = ae_rd8(r, ripple + RIPPLE_LIFE);
        if (life == 0) continue;

        life = (life - 1u) & 0xFFu;
        ae_wr8(r, ripple + RIPPLE_LIFE, life);

        int32_t velocity_x = ae_rds8(r, ripple + RIPPLE_VELOCITY_X);
        int32_t velocity_y = ae_rds8(r, ripple + RIPPLE_VELOCITY_Y);
        int32_t velocity_z = ae_rds8(r, ripple + RIPPLE_VELOCITY_Z);
        uint32_t x = (uint32_t)ae_rd16(r, ripple + RIPPLE_POS_X) +
                     (uint32_t)velocity_x;
        uint32_t y = (uint32_t)ae_rd16(r, ripple + RIPPLE_POS_Y) +
                     (uint32_t)velocity_y;
        uint32_t z = (uint32_t)ae_rd16(r, ripple + RIPPLE_POS_Z) +
                     (uint32_t)velocity_z;
        ae_wr16(r, ripple + RIPPLE_POS_X, x);
        ae_wr16(r, ripple + RIPPLE_POS_Y, y);
        ae_wr16(r, ripple + RIPPLE_POS_Z, z);

        int32_t ground_y = ae_rds16(r, ripple + RIPPLE_GROUND_Y);
        int32_t current_y = (int16_t)y;
        int32_t damped_y = (int8_t)(uint8_t)(velocity_y + 4);
        ae_wr8(r, ripple + RIPPLE_VELOCITY_Y, (uint32_t)damped_y);
        if (ground_y < current_y) {
            ae_wr16(r, ripple + RIPPLE_POS_Y, (uint32_t)ground_y);
            damped_y = -damped_y / 3;
            ae_wr8(r, ripple + RIPPLE_VELOCITY_Y, (uint32_t)damped_y);
            if (damped_y >= -11) {
                ae_wr8(r, ripple + RIPPLE_LIFE, 0);
                continue;
            }
        }

        uint32_t draw_args[10] = {
            ripple,
            (uint32_t)(int32_t)(int16_t)ae_rd16(r, ripple + RIPPLE_POS_X),
            (uint32_t)(int32_t)(int16_t)ae_rd16(r, ripple + RIPPLE_POS_Y),
            (uint32_t)(int32_t)(int16_t)ae_rd16(r, ripple + RIPPLE_POS_Z),
            (uint32_t)ae_rds16(r, ripple + RIPPLE_GROUND_Y),
            (uint32_t)ae_rds8(r, ripple + RIPPLE_VELOCITY_X),
            (uint32_t)ae_rds8(r, ripple + RIPPLE_VELOCITY_Y),
            (uint32_t)ae_rds8(r, ripple + RIPPLE_VELOCITY_Z),
            ae_rd8(r, ripple + RIPPLE_LIFE),
            cursor
        };
        (void)ovl1a8_render(r, h, "Area57_Boss_RippleProjectAndEmit",
                            BOSS_RIPPLE_PROJECT_HOOK, 10, draw_args);
        if (ae_ram_failed(r)) return;
    }

    ae_wr32(r, ORDERING_TABLE_CURSOR, cursor);
}

static uint32_t ovl1a8_random_word(AeRam *r) {
    uint32_t seed = (ae_rd16(r, AE_SPAWN_RAND_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
    return seed;
}

static int ovl1a8_set_position_padding(AeRam *r, const AeOvl1a8Hooks *h,
                                       uint32_t position,
                                       uint32_t original_function) {
    if (!h || !h->spawn || !h->spawn->position_padding) {
        ae_ram_fail(r, "Area57 overlay position-padding hook missing");
        return 0;
    }
    ae_wr16(r, position + POSITION16_PADDING,
            h->spawn->position_padding(h->spawn->user, r, original_function));
    return !ae_ram_failed(r);
}

static void ovl1a8_random_effect_direction(AeRam *r, uint32_t position) {
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t seed = ovl1a8_random_word(r);
        ae_wr16(r, position + axis * 2u, (seed >> 4) & 0xFF0u);
    }
}

static void ovl1a8_boss_sequence_tail(AeRam *r, const AeOvl1a8Hooks *h,
                                      uint32_t actor) {
    ae_ovl1a8_BossStepAnim(r, actor);
    ae_ovl1a8_BossDrawScaledModel(r, h, actor);
    ae_ovl1a8_BossUpdateEntries(r, h);
    ae_ovl1a8_BossUpdateDebris(r, h);
    ae_ovl1a8_BossUpdateRipples(r, h);
    ae_ovl1a8_BossUpdateSparks(r, h, actor);
}

static void ovl1a8_draw_shadow(AeRam *r, const AeOvl1a8Hooks *h,
                               uint32_t position, int32_t ground_y,
                               uint32_t size) {
    uint32_t args[5] = {
        (uint32_t)ae_rds32(r, position + POSITION32_X),
        (uint32_t)ae_rds32(r, position + POSITION32_Y),
        (uint32_t)ae_rds32(r, position + POSITION32_Z),
        (uint32_t)ground_y,
        size
    };
    (void)ovl1a8_render(r, h, "DrawShadow", RENDER_SHADOW, 5, args);
}

void ae_ovl1a8_BossUpdateSequence(AeRam *r, const AeOvl1a8Hooks *h) {
    uint32_t actor = AE_OVL1A8_ACTORS;
    uint32_t player = BOSS_PLAYER;
    uint32_t phase = ae_rd8(r, BOSS_PHASE);

    if (ae_rd8(r, BOSS_FLAGS) != 0u) return;
    if (phase >= 20u) {
        ae_ovl1a8_BossUpdateLateSequence(r, h);
        return;
    }

    ae_wr16(r, BOSS_SEQUENCE_POSITION,
            ae_rd16(r, actor + ACTOR_POSITION_X));
    ae_wr16(r, BOSS_SEQUENCE_POSITION + 2u,
            ae_rd16(r, actor + ACTOR_POSITION_Y) - 590u);
    ae_wr16(r, BOSS_SEQUENCE_POSITION + 4u,
            ae_rd16(r, actor + ACTOR_POSITION_Z));

    if (phase != 19u) {
        ae_wr16(r, OVL1A8_SEQUENCE_VECTOR,
                ae_rd16(r, player + ACTOR_POSITION_X) -
                ae_rd16(r, actor + ACTOR_POSITION_X));
        ae_wr16(r, OVL1A8_SEQUENCE_VECTOR + 2u, 0);
        ae_wr16(r, OVL1A8_SEQUENCE_VECTOR + 4u,
                ae_rd16(r, player + ACTOR_POSITION_Z) -
                ae_rd16(r, actor + ACTOR_POSITION_Z));
        (void)ae_sdk_VectorNormalSS(r, OVL1A8_SEQUENCE_VECTOR,
                                    actor + ACTOR_SEQUENCE_HEADING);
    }

    switch (phase) {
    case 1: {
        ae_ovl1a8_BossSetAnim(r, actor, 0);
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ANIM_BUSY) == 0u) {
            uint32_t seed = ovl1a8_random_word(r);
            if (((seed >> 8) & 0x3Fu) == 0u) {
                ae_ovl1a8_BossClearFlag(r, 10);
                ae_ovl1a8_BossSetAnim(r, actor, 2);
                ae_ovl1a8_BossLoopStart(r, h, 174);
            }
        } else {
            int32_t timer = ae_rds16(r, actor + ACTOR_SEQUENCE_ANIM_TIMER);
            if (timer != 0) {
                uint32_t next_timer =
                    (ae_rd16(r, actor + ACTOR_SEQUENCE_ANIM_TIMER) - 1u) & 0xFFFFu;
                ae_wr16(r, actor + ACTOR_SEQUENCE_ANIM_TIMER, next_timer);
                if (next_timer == 0u) {
                    ae_ovl1a8_BossClearFlag(r, 2);
                    ae_wr8(r, actor + ACTOR_SEQUENCE_ANIM_BUSY,
                           ae_rd8(r, actor + ACTOR_SEQUENCE_ANIM_BUSY) - 1u);
                    ae_ovl1a8_BossSetAnim(r, actor, 7);
                    ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, 1);
                    ae_wr32(r, BOSS_SEQUENCE_COUNTER, 0);
                    ae_TrailSpawn(r, BOSS_SEQUENCE_POSITION, 1, -1100, 8);
                    ae_TrailSpawn(r, BOSS_SEQUENCE_POSITION, 1, -450, 0);
                    for (int32_t delay = 3; delay >= 1; --delay) {
                        ovl1a8_random_effect_direction(r,
                                                       OVL1A8_SEQUENCE_EFFECT_POS);
                        ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                              BOSS_SEQUENCE_POSITION,
                                              BOSS_SEQUENCE_EFFECTS_A, 1,
                                              delay * 2,
                                              OVL1A8_SEQUENCE_EFFECT_POS);
                    }
                    (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 173);
                    ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 0);
                }
            }
        }
        break;
    }
    case 2: {
        uint32_t ticks = ae_rd32(r, BOSS_SEQUENCE_COUNTER) + 1u;
        ae_wr32(r, BOSS_SEQUENCE_COUNTER, ticks);
        if (ticks >= 12u) {
            ae_ovl1a8_BossSpawnEffectGroupsN(r, h, BOSS_SEQUENCE_POSITION);
            ae_ovl1a8_BossClearFlag(r, 3);
            ae_SpawnParticle(r, h ? h->spawn : NULL, 274,
                             BOSS_SEQUENCE_POSITION);
            ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                  BOSS_SEQUENCE_POSITION,
                                  BOSS_SEQUENCE_EFFECTS_B, 2, 6, 0);
            ae_wr32(r, BOSS_SEQUENCE_COUNTER, 0);
        }
        break;
    }
    case 3: {
        uint32_t ticks = ae_rd32(r, BOSS_SEQUENCE_COUNTER) + 1u;
        ae_wr32(r, BOSS_SEQUENCE_COUNTER, ticks);
        if (ticks >= 30u) {
            uint32_t variant;
            do {
                variant = (ovl1a8_random_word(r) >> 8) & 7u;
            } while (variant == ae_rd8(r, actor + ACTOR_SEQUENCE_VARIANT));
            ae_wr8(r, actor + ACTOR_SEQUENCE_VARIANT, variant);
            uint32_t direction = BOSS_DIRECTION_TABLE + variant * 8u;
            ae_wr32(r, actor + ACTOR_POSITION_X,
                    (uint32_t)(int32_t)ae_rds16(r, direction));
            ae_wr32(r, actor + ACTOR_POSITION_Y,
                    (uint32_t)(int32_t)ae_rds16(r, direction + 2u));
            ae_wr32(r, actor + ACTOR_POSITION_Z,
                    (uint32_t)(int32_t)ae_rds16(r, direction + 4u));
            ae_wr32(r, BOSS_SEQUENCE_COUNTER, 0);
            ae_ovl1a8_BossClearFlag(r, 4);
            ae_ovl1a8_BossSetAnim(r, actor, 8);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 172);
            ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, 1);
        }
        break;
    }
    case 4: {
        uint32_t ticks = ae_rd32(r, BOSS_SEQUENCE_COUNTER) + 1u;
        ae_wr32(r, BOSS_SEQUENCE_COUNTER, ticks);
        if (ticks == 1u) {
            ae_TrailSpawn(r, BOSS_SEQUENCE_POSITION, 1, 1024, 8);
            ae_TrailSpawn(r, BOSS_SEQUENCE_POSITION, 1, 600, 0);
            ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                  BOSS_SEQUENCE_POSITION,
                                  BOSS_SEQUENCE_EFFECTS_C, 2, 6, 0);
        }
        if (ticks >= 16u) {
            ae_ovl1a8_BossSpawnEffectGroupsN(r, h, BOSS_SEQUENCE_POSITION);
            ae_ovl1a8_BossClearFlag(r, 1);
            ae_wr8(r, actor + ACTOR_SEQUENCE_ANIM_BUSY, 0);
        }
        break;
    }
    case 10:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 11);
            ae_ovl1a8_BossSetAnim(r, actor, 3);
            ae_wr32(r, BOSS_SEQUENCE_COUNTER, 40);
            ae_ovl1a8_BossAddSparks(r, actor, 1);
        }
        break;
    case 11: {
        uint32_t ticks = ae_rd32(r, BOSS_SEQUENCE_COUNTER) - 1u;
        ae_wr32(r, BOSS_SEQUENCE_COUNTER, ticks);
        if (ticks == 0u) {
            ae_ovl1a8_BossClearFlag(r, 12);
            ae_ovl1a8_BossSetAnim(r, actor, 4);
            ae_wr32(r, BOSS_SEQUENCE_COUNTER, 0);
            ae_ovl1a8_BossLoopStop(r, h, 174);
            ae_ovl1a8_BossLoopStart(r, h, 175);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 178);
        }
        break;
    }
    case 12:
        ae_wr32(r, BOSS_SEQUENCE_COUNTER,
                ae_rd32(r, BOSS_SEQUENCE_COUNTER) + 1u);
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 13);
            ae_ovl1a8_BossSetAnim(r, actor, 5);
            ae_TrailSpawn(r, BOSS_SEQUENCE_POSITION, 1, 310, 0);
            ae_wr32(r, BOSS_SEQUENCE_COUNTER, 120);
            ae_ovl1a8_BossAddEntry(r, BOSS_SEQUENCE_POSITION);
            ae_SpawnParticle(r, h ? h->spawn : NULL, 274,
                             BOSS_SEQUENCE_POSITION);
            ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                  BOSS_SEQUENCE_POSITION,
                                  BOSS_SEQUENCE_EFFECTS_B, 1, 0,
                                  actor + ACTOR_SEQUENCE_HEADING);
            ae_ovl1a8_BossLoopStop(r, h, 175);
            ae_ovl1a8_BossLoopStart(r, h, 176);
        }
        break;
    case 13: {
        uint32_t ticks = ae_rd32(r, BOSS_SEQUENCE_COUNTER) - 1u;
        ae_wr32(r, BOSS_SEQUENCE_COUNTER, ticks);
        if (ticks == 0u) {
            ae_ovl1a8_BossClearFlag(r, 14);
            ae_ovl1a8_BossSetAnim(r, actor, 6);
            ae_ovl1a8_BossLoopStop(r, h, 176);
        }
        break;
    }
    case 14:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 1);
            ae_ovl1a8_BossSetAnim(r, actor, 0);
            ae_wr8(r, actor + ACTOR_SEQUENCE_ANIM_BUSY, 1);
            ae_wr16(r, actor + ACTOR_SEQUENCE_ANIM_TIMER,
                    60u + ((ovl1a8_random_word(r) >> 8) & 0x1Fu));
        }
        break;
    case 18:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 1);
            ae_ovl1a8_BossSetAnim(r, actor, 0);
        }
        break;
    case 19: {
        ae_wr16(r, actor + ACTOR_COLLISION_RADIUS, 1);
        ae_wr16(r, actor + ACTOR_COLLISION_HEIGHT, 1);
        uint32_t seed = ovl1a8_random_word(r);
        ae_wr16(r, OVL1A8_SEQUENCE_EFFECT_POS,
                ae_rd16(r, BOSS_SEQUENCE_POSITION) + (seed >> 8) - 128u);
        seed = ovl1a8_random_word(r);
        ae_wr16(r, OVL1A8_SEQUENCE_EFFECT_POS + 2u,
                ae_rd16(r, BOSS_SEQUENCE_POSITION + 2u) +
                2u * ((int32_t)(seed >> 8) - 128));
        seed = ovl1a8_random_word(r);
        ae_wr16(r, OVL1A8_SEQUENCE_EFFECT_POS + 4u,
                ae_rd16(r, BOSS_SEQUENCE_POSITION + 4u) +
                (int32_t)(seed >> 8) - 128);
        ae_SpriteEffectQueue0(r, OVL1A8_SEQUENCE_EFFECT_POS);
        ae_SpawnParticle(r, h ? h->spawn : NULL, 7,
                         OVL1A8_SEQUENCE_EFFECT_POS);
        if (((ovl1a8_random_word(r) >> 8) & 7u) == 0u) {
            ae_SpawnParticle(r, h ? h->spawn : NULL, 258,
                             OVL1A8_SEQUENCE_EFFECT_POS);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);
        }

        ae_wr16(r, OVL1A8_SEQUENCE_VECTOR,
                (uint32_t)-256 - ae_rd16(r, actor + ACTOR_POSITION_X));
        ae_wr16(r, OVL1A8_SEQUENCE_VECTOR + 2u, 0);
        ae_wr16(r, OVL1A8_SEQUENCE_VECTOR + 4u,
                (uint32_t)-256 - ae_rd16(r, actor + ACTOR_POSITION_Z));
        (void)ae_sdk_VectorNormalSS(r, OVL1A8_SEQUENCE_VECTOR,
                                    OVL1A8_SEQUENCE_VECTOR);
        int32_t direction_x = ae_rds16(r, OVL1A8_SEQUENCE_VECTOR);
        int32_t direction_z = ae_rds16(r, OVL1A8_SEQUENCE_VECTOR + 4u);
        ae_wr32(r, actor + ACTOR_POSITION_Y,
                ae_rd32(r, actor + ACTOR_POSITION_Y) +
                ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y));
        ae_wr32(r, actor + ACTOR_POSITION_X,
                ae_rd32(r, actor + ACTOR_POSITION_X) + direction_x / 96);
        ae_wr32(r, actor + ACTOR_POSITION_Z,
                ae_rd32(r, actor + ACTOR_POSITION_Z) + direction_z / 96);
        int32_t vertical_velocity =
            ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y);
        if (vertical_velocity < 32)
            ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y,
                    (uint32_t)(vertical_velocity + 2));
        if (ae_rds32(r, actor + ACTOR_POSITION_Y) < -2432) break;

        ae_wr16(r, OVL1A8_SEQUENCE_IMPACT_POS,
                ae_rd16(r, actor + ACTOR_POSITION_X));
        ae_wr16(r, OVL1A8_SEQUENCE_IMPACT_POS + 2u, (uint32_t)-2624);
        ae_wr16(r, OVL1A8_SEQUENCE_IMPACT_POS + 4u,
                ae_rd16(r, actor + ACTOR_POSITION_Z));
        ae_EffectBigBurst(r, h ? h->spawn : NULL,
                          ae_rds16(r, OVL1A8_SEQUENCE_IMPACT_POS), -2624,
                          ae_rds16(r, OVL1A8_SEQUENCE_IMPACT_POS + 4u));
        ae_ovl1a8_BossSpawnDebris(r, OVL1A8_SEQUENCE_IMPACT_POS);
        ae_CameraShake(r, 2);
        (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);
        ae_ovl1a8_BossRumbleLong(r, h);
        ae_ovl1a8_BossStart(r, h);
        ae_ovl1a8_BossClearFlag(r, 20);
        int32_t x = ae_rds16(r, OVL1A8_SEQUENCE_VECTOR);
        int32_t z = ae_rds16(r, OVL1A8_SEQUENCE_VECTOR + 4u);
        if (x < 0) x += 127;
        if (z < 0) z += 127;
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_X,
                (uint32_t)ovl1a8_asr(x, 7));
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y, (uint32_t)-48);
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z,
                (uint32_t)ovl1a8_asr(z, 7));
        ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 0);
        break;
    }
    default:
        break;
    }

    phase = ae_rd8(r, BOSS_PHASE);
    if (phase == 11u || phase == 12u) {
        uint32_t ticks = ae_rd32(r, BOSS_SEQUENCE_COUNTER);
        if (phase == 11u) {
            if ((ticks & 1u) != 0u) {
                ovl1a8_random_effect_direction(r, OVL1A8_SEQUENCE_EFFECT_POS);
                ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                      BOSS_SEQUENCE_POSITION,
                                      BOSS_SEQUENCE_EFFECTS_D, 1, 0,
                                      OVL1A8_SEQUENCE_EFFECT_POS);
            }
            if ((ticks & 3u) == 0u) {
                ovl1a8_random_effect_direction(r, OVL1A8_SEQUENCE_EFFECT_POS);
                ae_EffectSpawnFromDef(r, h ? h->spawn : NULL,
                                      BOSS_SEQUENCE_POSITION,
                                      BOSS_SEQUENCE_EFFECTS_C, 1, 0,
                                      OVL1A8_SEQUENCE_EFFECT_POS);
            }
        }
        ae_SpawnRandomParticle(r, BOSS_SEQUENCE_POSITION, 0,
                               0x101u, 3000);
    }

    uint32_t effect_timer = ae_rd16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER);
    if (effect_timer != 0u)
        ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, effect_timer - 1u);

    if ((ae_rd32(r, actor + ACTOR_SPECIAL_TYPE) >> 28) == 11u) {
        phase = ae_rd8(r, BOSS_PHASE);
        effect_timer = ae_rd16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER);
        if (phase == 1u && effect_timer == 0u) {
            ae_ovl1a8_BossSpawnDebris(r, BOSS_SEQUENCE_POSITION);
            ae_CameraShake(r, 1);
            uint32_t hit_count =
                (ae_rd8(r, actor + ACTOR_SEQUENCE_HIT_COUNT) - 1u) & 0xFFu;
            ae_wr8(r, actor + ACTOR_SEQUENCE_HIT_COUNT, hit_count);
            if (hit_count == 0u) {
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 179);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);
                ae_ovl1a8_BossClearFlag(r, 19);
                ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y, (uint32_t)-30);
            } else {
                ae_EffectBigBurst(r, h ? h->spawn : NULL,
                                  ae_rds16(r, BOSS_SEQUENCE_POSITION),
                                  ae_rds16(r, BOSS_SEQUENCE_POSITION + 2u),
                                  ae_rds16(r, BOSS_SEQUENCE_POSITION + 4u));
                ae_ovl1a8_BossClearFlag(r, 18);
                ae_ovl1a8_BossSetAnim(r, actor, 1);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);
                ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, phase);
                (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 179);
            }
            ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 0xFFFFu);
            ae_ovl1a8_BossRumbleLong(r, h);
        } else {
            ae_SpawnParticle(r, h ? h->spawn : NULL, 279,
                             BOSS_SEQUENCE_POSITION);
            ae_CameraShake(r, 0);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 96);
            ae_ovl1a8_BossRumbleLong(r, h);
            if (effect_timer < 16u)
                ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 16u);
        }
    }

    ovl1a8_boss_sequence_tail(r, h, actor);
}

static int32_t ovl1a8_clamp(int32_t value, int32_t low, int32_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void ovl1a8_update_late_motion(AeRam *r, const AeOvl1a8Hooks *h,
                                     uint32_t actor, uint32_t distance) {
    uint32_t seed;
    int32_t speed_x = ae_rds16(r, actor + ACTOR_SEQUENCE_TURN_X);
    int32_t speed_z = ae_rds16(r, actor + ACTOR_SEQUENCE_TURN_Z);

    ae_wr32(r, actor + ACTOR_POSITION_X,
            ae_rd32(r, actor + ACTOR_POSITION_X) + (uint32_t)speed_x);
    ae_wr32(r, actor + ACTOR_POSITION_Z,
            ae_rd32(r, actor + ACTOR_POSITION_Z) + (uint32_t)speed_z);
    speed_x = ovl1a8_wave_div8(speed_x * 7);
    speed_z = ovl1a8_wave_div8(speed_z * 7);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_X, (uint32_t)speed_x);
    ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_Z, (uint32_t)speed_z);

    seed = ovl1a8_random_word(r);
    int regenerate_target = (((seed >> 8) & 0x3Fu) == 0u);
    if (!regenerate_target) {
        int32_t delta_x = ae_rds16(r, actor + ACTOR_SEQUENCE_TARGET_X) -
                          ae_rds32(r, actor + ACTOR_POSITION_X);
        int32_t delta_z = ae_rds16(r, actor + ACTOR_SEQUENCE_TARGET_Z) -
                          ae_rds32(r, actor + ACTOR_POSITION_Z);
        uint32_t distance_sq =
            (uint32_t)((int64_t)delta_x * delta_x) +
            (uint32_t)((int64_t)delta_z * delta_z);
        regenerate_target = (int32_t)distance_sq < 9216;
    }
    if (regenerate_target) {
        seed = ovl1a8_random_word(r);
        int32_t offset = (int32_t)(seed >> 8) - 128;
        ae_wr16(r, actor + ACTOR_SEQUENCE_TARGET_COOLDOWN, 0);
        ae_wr16(r, actor + ACTOR_SEQUENCE_TARGET_X,
                (uint32_t)(ovl1a8_asr(offset * 19, 1) - 256));
        seed = ovl1a8_random_word(r);
        offset = (int32_t)(seed >> 8) - 128;
        ae_wr16(r, actor + ACTOR_SEQUENCE_TARGET_Z,
                (uint32_t)(ovl1a8_asr(offset * 19, 1) - 256));
    }

    if (distance < 800u) {
        int32_t divisor = (int32_t)ae_rd8(r, actor + ACTOR_SEQUENCE_HIT_COUNT) * 20 +
                          (int32_t)ae_rds16(r, actor + ACTOR_SEQUENCE_COUNTER) * 8 + 550;
        if (divisor < 400) divisor = 400;
        int32_t toward_x = ae_rds16(r, LATE_VECTOR_NORMAL_TEMP) / divisor;
        int32_t toward_z = ae_rds16(r, LATE_VECTOR_NORMAL_TEMP + 4u) / divisor;
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_X,
                ae_rd16(r, actor + ACTOR_SEQUENCE_VELOCITY_X) - (uint32_t)toward_x);
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z,
                ae_rd16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z) - (uint32_t)toward_z);
    } else {
        ae_wr16(r, LATE_VECTOR_EFFECT_TEMP,
                ae_rd16(r, actor + ACTOR_SEQUENCE_TARGET_X) -
                ae_rd16(r, actor + ACTOR_POSITION_X));
        ae_wr16(r, LATE_VECTOR_EFFECT_TEMP + 2u, 0);
        ae_wr16(r, LATE_VECTOR_EFFECT_TEMP + 4u,
                ae_rd16(r, actor + ACTOR_SEQUENCE_TARGET_Z) -
                ae_rd16(r, actor + ACTOR_POSITION_Z));
        (void)ae_sdk_VectorNormalSS(r, LATE_VECTOR_EFFECT_TEMP,
                                    LATE_VECTOR_EFFECT_TEMP);
        int32_t desired_x = ae_rds16(r, LATE_VECTOR_EFFECT_TEMP) / 800;
        int32_t desired_z = ae_rds16(r, LATE_VECTOR_EFFECT_TEMP + 4u) / 800;
        speed_x = ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_X) + desired_x;
        speed_z = ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z) + desired_z;
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_X,
                (uint32_t)ovl1a8_clamp(speed_x, -80, 80));
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z,
                (uint32_t)ovl1a8_clamp(speed_z, -80, 80));
    }

    speed_x = ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_X);
    speed_z = ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z);
    ae_wr32(r, actor + ACTOR_POSITION_X,
            ae_rd32(r, actor + ACTOR_POSITION_X) + (uint32_t)speed_x);
    ae_wr32(r, actor + ACTOR_POSITION_Z,
            ae_rd32(r, actor + ACTOR_POSITION_Z) + (uint32_t)speed_z);
    ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_X,
            (uint32_t)ovl1a8_wave_div8(speed_x * 7));
    ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z,
            (uint32_t)ovl1a8_wave_div8(speed_z * 7));

    ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP,
            ae_rd16(r, actor + ACTOR_POSITION_X) + 256u);
    ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP + 2u, 0);
    ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP + 4u,
            ae_rd16(r, actor + ACTOR_POSITION_Z) + 256u);
    int32_t radius_sq = ae_sdk_VectorNormalSS(r, LATE_VECTOR_PHYSICS_TEMP,
                                              LATE_VECTOR_PHYSICS_TEMP);
    if (radius_sq > 0x240000) {
        int32_t x = ae_rds16(r, LATE_VECTOR_PHYSICS_TEMP);
        int32_t z = ae_rds16(r, LATE_VECTOR_PHYSICS_TEMP + 4u);
        ae_wr32(r, actor + ACTOR_POSITION_X,
                (uint32_t)(ovl1a8_asr(x * 3, 3) - 256));
        ae_wr32(r, actor + ACTOR_POSITION_Z,
                (uint32_t)(ovl1a8_asr(z * 3, 3) - 256));
    }

    uint32_t age = (ae_rd8(r, actor + ACTOR_SEQUENCE_EFFECT_AGE) + 1u) & 0xFFu;
    ae_wr8(r, actor + ACTOR_SEQUENCE_EFFECT_AGE, age);
    if (ae_rd16(r, actor + ACTOR_SEQUENCE_COUNTER) == 0u) return;

    uint32_t triangle = age & 0x1Fu;
    if (triangle >= 16u) triangle = 31u - triangle;
    int32_t base_radius = ae_rds16(r, actor + ACTOR_SEQUENCE_COUNTER) * 10 + 50;
    uint32_t position = LATE_POSITION;
    if ((ae_rd16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER) & 1u) == 0u) {
        uint32_t outer_step = triangle * 4u;
        uint32_t inner_step = triangle * 2u;
        uint32_t color = ((outer_step + 24u) << 16) |
                         ((inner_step + 24u) << 8) | 24u;
        uint32_t args[4] = {position, (uint32_t)(base_radius + (int32_t)outer_step),
                            1u, color};
        (void)ovl1a8_render(r, h, "EffectRingDraw", RENDER_EFFECT_RING,
                            4, args);
        color = ((inner_step + 48u) << 16) |
                ((outer_step + 48u) << 8) | 48u;
        args[1] = (uint32_t)(base_radius + (int32_t)inner_step + 24);
        args[2] = 5u;
        args[3] = color;
        (void)ovl1a8_render(r, h, "EffectRingDraw", RENDER_EFFECT_RING,
                            4, args);
    }
    if ((age & 2u) == 0u)
        ae_SpawnRandomParticle(r, position, 0, 0,
                               ovl1a8_asr(base_radius, 1) +
                               (int32_t)(triangle * 4u));
    ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 206, position,
                        LATE_EFFECT_GROUPS, 0);
    ae_wr16(r, actor + ACTOR_COLLISION_HEIGHT, base_radius);
}

void ae_ovl1a8_BossUpdateLateSequence(AeRam *r,
                                      const AeOvl1a8Hooks *h) {
    uint32_t actor = AE_OVL1A8_ACTORS;
    uint32_t player = BOSS_PLAYER;
    uint32_t position = LATE_POSITION;
    ae_wr16(r, position, ae_rd16(r, actor + ACTOR_POSITION_X));
    ae_wr16(r, position + POSITION16_Y, ae_rd16(r, actor + ACTOR_POSITION_Y) - 128u);
    ae_wr16(r, position + POSITION16_Z, ae_rd16(r, actor + ACTOR_POSITION_Z));
    ae_wr16(r, LATE_VECTOR_NORMAL_TEMP,
            ae_rd16(r, player + ACTOR_POSITION_X) -
            ae_rd16(r, actor + ACTOR_POSITION_X));
    ae_wr16(r, LATE_VECTOR_NORMAL_TEMP + 2u, 0);
    ae_wr16(r, LATE_VECTOR_NORMAL_TEMP + 4u,
            ae_rd16(r, player + ACTOR_POSITION_Z) -
            ae_rd16(r, actor + ACTOR_POSITION_Z));
    int32_t distance_squared = ae_sdk_VectorNormalSS(r,
        LATE_VECTOR_NORMAL_TEMP, LATE_VECTOR_NORMAL_TEMP);
    uint32_t distance = (uint32_t)ae_sdk_SquareRoot0(r, distance_squared);
    uint32_t phase = ae_rd8(r, BOSS_PHASE);

    int actor_mode = ae_rd8(r, actor + ACTOR_SEQUENCE_MODE) != 0u;
    int sequence_phase = phase >= 22u && phase <= 24u;
    if (sequence_phase || (!actor_mode && ae_rd8(r, BOSS_TIMER) == 0u)) {
        uint32_t mode = ae_rd32(r, LATE_WORLD_MODE) & 3u;
        if (mode == 0u || mode == 2u) {
            ae_SpawnEffectGroup(r, h ? h->spawn : NULL,
                                mode == 0u ? 208 : 209,
                                position, LATE_EFFECT_GROUPS, 0);
        }
    }
    if (!actor_mode) {
        if (ae_rd8(r, BOSS_MODE) == 0u) return;
        if ((ae_rd32(r, actor + ACTOR_ANIMATION) & 0xFFFFu) == 5u)
            ae_ovl1a8_BossSetAnim(r, actor, 6);
        ae_ovl1a8_BossStepAnim(r, actor);
        ae_ovl1a8_BossDrawScaledModel(r, h, actor);
        ovl1a8_draw_shadow(r, h, actor + ACTOR_POSITION_X,
                           -2560, 64);
        return;
    }

    switch (phase) {
    case 22:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 23);
            ae_ovl1a8_BossSetAnim(r, actor, 2);
            ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, 1);
        }
        break;
    case 23:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 24);
            ae_ovl1a8_BossSetAnim(r, actor, 3);
            ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, 1);
        }
        break;
    case 24:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 25);
            ae_ovl1a8_BossSetAnim(r, actor, 4);
            ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, 1);
        }
        break;
    case 25:
        if (ae_rd8(r, actor + ACTOR_SEQUENCE_ACTIVE) == 0u) {
            ae_ovl1a8_BossClearFlag(r, 21);
            ae_ovl1a8_BossSetAnim(r, actor, 0);
            ae_ovl1a8_BossDeathBurst(r, h, actor, position);
        }
        break;
    case 20: {
        ae_ovl1a8_BossSetAnim(r, actor, 0);
        ae_wr32(r, actor + ACTOR_POSITION_X,
                ae_rd32(r, actor + ACTOR_POSITION_X) +
                (uint32_t)ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_X));
        ae_wr32(r, actor + ACTOR_POSITION_Y,
                ae_rd32(r, actor + ACTOR_POSITION_Y) +
                (uint32_t)ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y));
        ae_wr32(r, actor + ACTOR_POSITION_Z,
                ae_rd32(r, actor + ACTOR_POSITION_Z) +
                (uint32_t)ae_rds16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z));
        ae_wr16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y,
                ae_rd16(r, actor + ACTOR_SEQUENCE_VELOCITY_Y) + 6u);
        if (ae_rds32(r, actor + ACTOR_POSITION_Y) >= -2560) {
            ae_ovl1a8_BossClearFlag(r, 21);
            ae_wr32(r, actor + ACTOR_POSITION_Y, (uint32_t)-2560);
            ae_ovl1a8_BossSetAnim(r, actor, 0);
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, 120);
        }
        break;
    }
    case 21:
        ae_ovl1a8_BossSetAnim(r, actor, 0);
        ovl1a8_update_late_motion(r, h, actor, distance);
        break;
    default:
        break;
    }

    uint32_t timer = ae_rd16(r, actor + ACTOR_SEQUENCE_TURN_TIMER);
    if (timer != 0u && ae_rd8(r, BOSS_PHASE) == 21u) {
        --timer;
        ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, timer);
        if (timer == 0u && ae_rd16(r, actor + ACTOR_SEQUENCE_COUNTER) == 0u)
            ae_ovl1a8_BossDeathBurst(r, h, actor, position);
    }

    timer = ae_rd16(r, actor + ACTOR_SEQUENCE_TURN_TIMER);
    if (distance < 1536u && timer == 0u) {
        ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, 20);
        if (ae_ovl1a8_BossAddWaveEntry(r, h) != 0) {
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER, 300);
            ae_wr8(r, BOSS_WAVE_SFX_FLAG, 1);
        } else {
            for (uint32_t i = 0; i < 16u; ++i) {
                ae_SpawnEffectGroup(r, h ? h->spawn : NULL, 205,
                                    position, LATE_EFFECT_GROUPS, 0);
                ae_SpawnParticle(r, h ? h->spawn : NULL, 46, position);
            }
        }
        uint32_t remaining = (ae_rd16(r, actor + ACTOR_SEQUENCE_COUNTER) - 1u) & 0xFFFFu;
        ae_wr16(r, actor + ACTOR_SEQUENCE_COUNTER, remaining);
        if (remaining == 0u) {
            ae_ovl1a8_BossSlamImpact(r, h, actor, position);
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_TIMER,
                    ae_rd8(r, actor + ACTOR_SEQUENCE_HIT_COUNT) * 16u + 150u);
        }
    }

    ae_wr16(r, LATE_VECTOR_EFFECT_TEMP,
            ae_rd16(r, actor + ACTOR_SEQUENCE_VELOCITY_X));
    ae_wr16(r, LATE_VECTOR_EFFECT_TEMP + 2u, 0);
    ae_wr16(r, LATE_VECTOR_EFFECT_TEMP + 4u,
            ae_rd16(r, actor + ACTOR_SEQUENCE_VELOCITY_Z));
    (void)ae_sdk_VectorNormalSS(r, LATE_VECTOR_EFFECT_TEMP,
                                LATE_VECTOR_EFFECT_TEMP);
    int32_t velocity_x = ae_rds16(r, LATE_VECTOR_EFFECT_TEMP);
    int32_t velocity_z = ae_rds16(r, LATE_VECTOR_EFFECT_TEMP + 4u);
    uint32_t angle_vector = actor + ACTOR_SEQUENCE_HEADING;
    int32_t heading_x = ae_rds16(r, angle_vector);
    int32_t heading_z = ae_rds16(r, angle_vector + 4u);
    int32_t dot = (velocity_x * heading_x + velocity_z * heading_z) >> 12;
    int32_t cross = velocity_x * heading_z - velocity_z * heading_x;
    if (dot < 4034) {
        uint32_t matrix = LATE_ROTATION_MATRIX;
        (void)ae_sdk_MatrixIdentity(r, matrix);
        (void)ae_sdk_RotMatrixY(r, cross >= 0 ? 113 : -113, matrix);
        ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP + VECTOR16_X,
                ae_rd16(r, angle_vector + VECTOR16_X));
        ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP + VECTOR16_Y,
                ae_rd16(r, angle_vector + VECTOR16_Y));
        ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP + VECTOR16_Z,
                ae_rd16(r, angle_vector + VECTOR16_Z));
        ae_wr16(r, LATE_VECTOR_PHYSICS_TEMP + VECTOR16_PADDING,
                ae_rd16(r, angle_vector + VECTOR16_PADDING));
        (void)ae_sdk_ApplyMatrixSV(r, matrix, LATE_VECTOR_PHYSICS_TEMP,
                                   angle_vector);
        (void)ae_sdk_VectorNormalSS(r, angle_vector, angle_vector);
    }

    uint32_t effect_timer = ae_rd16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER);
    if (effect_timer != 0u)
        ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, effect_timer - 1u);

    if (ae_rd16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER) == 0u &&
        (ae_rd32(r, actor + ACTOR_SEQUENCE_BODY_TAG) >> 28) == 11u) {
        uint32_t contact = ae_rd16(r, actor + ACTOR_SEQUENCE_BODY_TAG) & 0x0FFFu;
        (void)ae_GetObjectByIndex(r, contact);
        (void)ae_CollisionBodyIndexOf(r, (int16_t)contact);
        if (ae_rd16(r, actor + ACTOR_SEQUENCE_COUNTER) == 0u) {
            ae_ovl1a8_BossSpawnDebris(r, position);
            ae_CameraShake(r, 0);
            ae_EffectBigBurst(r, h ? h->spawn : NULL,
                              ae_rds16(r, position), ae_rds16(r, position + POSITION16_Y),
                              ae_rds16(r, position + POSITION16_Z));
            ae_ovl1a8_BossClearFlag(r, 22);
            ae_ovl1a8_BossSetAnim(r, actor, 1);
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_X,
                    (uint32_t)(-ae_rds16(r, LATE_VECTOR_NORMAL_TEMP) / 48));
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_Z,
                    (uint32_t)(-ae_rds16(r, LATE_VECTOR_NORMAL_TEMP + 4u) / 48));
            ae_wr8(r, actor + ACTOR_SEQUENCE_SUBSTATE, 1);
            ae_wr32(r, BOSS_SEQUENCE_COUNTER, 30);
            ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 120);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 179);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 108);
            uint32_t hits = (ae_rd8(r, actor + ACTOR_SEQUENCE_HIT_COUNT) - 1u) & 0xFFu;
            ae_wr8(r, actor + ACTOR_SEQUENCE_HIT_COUNT, hits);
            if (hits == 0u) {
                ae_wr8(r, actor + ACTOR_SEQUENCE_MODE, 0);
                (void)ae_CollisionBodyUnregister(r, h ? h->body : NULL,
                                                  actor + ACTOR_BODY);
                ae_ovl1a8_BossSetAnim(r, actor, 5);
                ae_ovl1a8_BossClearFlag(r, 26);
                (void)ae_CollisionBodyRegister(r, h ? h->body : NULL, 3,
                                                actor + ACTOR_BODY, actor);
                ae_wr16(r, actor + ACTOR_SEQUENCE_BODY_FLAGS, 0x8000u);
                ae_wr16(r, actor + ACTOR_COLLISION_HEIGHT, 128);
                ae_wr16(r, actor + ACTOR_SEQUENCE_BODY_MODE, 0);
                ae_wr16(r, actor + ACTOR_COLLISION_RADIUS, 256);
            }
            ae_ovl1a8_BossRumbleLong(r, h);
        } else {
            ae_SpawnParticle(r, h ? h->spawn : NULL, 274, position);
            ae_SpawnParticle(r, h ? h->spawn : NULL, 265, position);
            ae_TrailSpawn(r, position, 1, 512, 0);
            ae_wr16(r, actor + ACTOR_SEQUENCE_EFFECT_TIMER, 24);
            int32_t dx = -ae_rds16(r, LATE_VECTOR_NORMAL_TEMP);
            int32_t dz = -ae_rds16(r, LATE_VECTOR_NORMAL_TEMP + 4u);
            if (dx < 0) dx += 31;
            if (dz < 0) dz += 31;
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_X,
                    (uint32_t)ovl1a8_asr(dx, 5));
            ae_wr16(r, actor + ACTOR_SEQUENCE_TURN_Z,
                    (uint32_t)ovl1a8_asr(dz, 5));
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 96);
            (void)ae_PlaySfx(r, h ? h->sound_hw : NULL, 105);
            ae_ovl1a8_BossSpawnGroundRipples(r, h, position, 32);
            uint32_t remaining = ae_rd16(r, actor + ACTOR_SEQUENCE_COUNTER);
            if ((int16_t)remaining != 0) {
                remaining = (remaining - 2u) & 0xFFFFu;
                ae_wr16(r, actor + ACTOR_SEQUENCE_COUNTER, remaining);
                if ((int16_t)remaining <= 0)
                    ae_ovl1a8_BossSlamImpact(r, h, actor, position);
            }
            ae_ovl1a8_BossRumbleBig(r, h);
        }
    }

    if (ae_rd8(r, actor + ACTOR_SEQUENCE_HIT_COUNT) < 4u)
        ae_SpawnParticle(r, h ? h->spawn : NULL, 7, position);
    ae_ovl1a8_BossStepAnim(r, actor);
    ae_ovl1a8_BossDrawScaledModel(r, h, actor);
    ovl1a8_draw_shadow(r, h, actor + ACTOR_POSITION_X, -2560, 64);
    ae_ovl1a8_BossUpdateDebris(r, h);
    ae_ovl1a8_BossUpdateRipples(r, h);
    ae_ovl1a8_BossUpdateSparks(r, h, actor);
    ae_ovl1a8_BossUpdateWaves(r, h);
}

