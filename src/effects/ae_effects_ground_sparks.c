/* Ground spark rotation, movement, impact response and native rendering. */
#include "ae_effects.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

#include <string.h>

enum {
    AE_GROUND_SPARK_COUNT = 32,
    AE_GROUND_SPARK_SIZE = 32,
    AE_GROUND_SPARK_POSITION_X = 0,
    AE_GROUND_SPARK_POSITION_Y = 2,
    AE_GROUND_SPARK_POSITION_Z = 4,
    AE_GROUND_SPARK_LIFE = 6,
    AE_GROUND_SPARK_SCALE = 7,
    AE_GROUND_SPARK_MOVE_X = 8,
    AE_GROUND_SPARK_MOVE_Z = 9,
    AE_GROUND_SPARK_VERTICAL_VELOCITY = 10,
    AE_GROUND_SPARK_ANGLE_X = 20,
    AE_GROUND_SPARK_ANGLE_Y = 22,
    AE_GROUND_SPARK_ANGLE_Z = 24,
    AE_GROUND_SPARK_GROUND_Y = 28,
    AE_GROUND_SPARK_POSITION_STEP = 30,
    AE_GROUND_SPARK_ANGULAR_STEP = 31,
    AE_GROUND_SPARK_VERTICAL_ACCELERATION = 6,
    AE_GROUND_SPARK_BOUNCE_VELOCITY_MIN = -48,
    AE_GROUND_SPARK_RENDER_SCALE = 64,
    AE_GROUND_SPARK_MATRIX_TRANSLATION_X = 20,
    AE_GROUND_SPARK_MATRIX_TRANSLATION_Y = 24,
    AE_GROUND_SPARK_MATRIX_TRANSLATION_Z = 28,
    AE_GROUND_SPARK_RENDER_SCALE_SCRATCH = 0x1F8002C0u,
    AE_GROUND_SPARK_GTE_OUTPUT = 0x1F800040u,
    AE_GROUND_SPARK_GTE_DEPTH = 0x1F800060u
};

#ifdef AE_EFFECTS_TEST
void effects_test_record_spawn_particle(int32_t type, uint32_t position);
#endif

static int ground_spark_call(AeRam *r, const AeEffectsHooks *hooks,
                             AeEffectsCallFn callback, const char *name,
                             uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return 0;
    }
    (void)callback(hooks->user, r, a0, a1, a2, a3);
    return !ae_ram_failed(r);
}

static int ground_spark_render(AeRam *r, const AeEffectsHooks *hooks,
                               uint32_t record, uint32_t matrix,
                               uint32_t scale, int *visible) {
    if (hooks == NULL || hooks->GpuDrawGroundSpark == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.GpuDrawGroundSpark not provided");
        return 0;
    }
    int32_t result = hooks->GpuDrawGroundSpark(hooks->user, r, record, matrix,
                                                AE_G_GROUND_SPARKS_OT_STATE,
                                                scale);
    if (ae_ram_failed(r)) return 0;
    *visible = result != 0;
    return 1;
}

static void ground_spark_update_spin(AeRam *r, uint32_t record) {
    int32_t angular_step = ae_rds8(r, record + AE_GROUND_SPARK_ANGULAR_STEP);
    int32_t position_step = ae_rds8(r, record + AE_GROUND_SPARK_POSITION_STEP);
    ae_wr16(r, record + AE_GROUND_SPARK_ANGLE_X,
            ae_rd16(r, record + AE_GROUND_SPARK_ANGLE_X) + angular_step);
    ae_wr16(r, record + AE_GROUND_SPARK_ANGLE_Y,
            ae_rd16(r, record + AE_GROUND_SPARK_ANGLE_Y) + angular_step);
    ae_wr16(r, record + AE_GROUND_SPARK_ANGLE_Z,
            ae_rd16(r, record + AE_GROUND_SPARK_ANGLE_Z) + position_step);
}

static void ground_spark_build_matrix(AeRam *r, uint32_t record,
                                      uint32_t matrix) {
    uint32_t scale_value = ae_rd8(r, record + AE_GROUND_SPARK_SCALE) *
                           AE_GROUND_SPARK_RENDER_SCALE;
    uint8_t saved_scale[12];
    for (uint32_t byte = 0; byte < sizeof(saved_scale); ++byte) {
        saved_scale[byte] = (uint8_t)ae_rd8(r,
                                            AE_GROUND_SPARK_RENDER_SCALE_SCRATCH + byte);
    }
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr32(r, AE_GROUND_SPARK_RENDER_SCALE_SCRATCH + axis * 4u,
                scale_value);
    }

    ae_sdk_RotMatrix(r, record + AE_GROUND_SPARK_ANGLE_X, matrix);
    if (ae_ram_failed(r)) return;
    ae_sdk_ScaleMatrix(r, matrix, AE_GROUND_SPARK_RENDER_SCALE_SCRATCH);
    if (ae_ram_failed(r)) return;

    ae_wr32(r, matrix + AE_GROUND_SPARK_MATRIX_TRANSLATION_X,
            (uint32_t)ae_rds16(r, record + AE_GROUND_SPARK_POSITION_X));
    ae_wr32(r, matrix + AE_GROUND_SPARK_MATRIX_TRANSLATION_Y,
            (uint32_t)ae_rds16(r, record + AE_GROUND_SPARK_POSITION_Y));
    ae_wr32(r, matrix + AE_GROUND_SPARK_MATRIX_TRANSLATION_Z,
            (uint32_t)ae_rds16(r, record + AE_GROUND_SPARK_POSITION_Z));
    ae_sdk_CompMatrix(r, AE_G_GROUND_SPARK_CAMERA_MATRIX, matrix, matrix);

    for (uint32_t byte = 0; byte < sizeof(saved_scale); ++byte) {
        ae_wr8(r, AE_GROUND_SPARK_RENDER_SCALE_SCRATCH + byte, saved_scale[byte]);
    }
}

static int32_t ground_spark_half_toward_zero(int32_t value) {
    return value >= 0 ? value / 2 : -((-value) / 2);
}

static void ground_spark_bounce(AeRam *r, uint32_t record,
                                const AeSpawnHooks *spawn) {
    int32_t move_x = ae_rds8(r, record + AE_GROUND_SPARK_MOVE_X);
    int32_t move_z = ae_rds8(r, record + AE_GROUND_SPARK_MOVE_Z);
    int32_t position_x = (int32_t)ae_rd16(r, record + AE_GROUND_SPARK_POSITION_X) + move_x;
    int32_t position_y = (int32_t)ae_rd16(r, record + AE_GROUND_SPARK_POSITION_Y) +
                         ae_rds16(r, record + AE_GROUND_SPARK_VERTICAL_VELOCITY);
    int32_t position_z = (int32_t)ae_rd16(r, record + AE_GROUND_SPARK_POSITION_Z) + move_z;
    ae_wr16(r, record + AE_GROUND_SPARK_POSITION_X, (uint16_t)position_x);
    ae_wr16(r, record + AE_GROUND_SPARK_POSITION_Y, (uint16_t)position_y);
    ae_wr16(r, record + AE_GROUND_SPARK_POSITION_Z, (uint16_t)position_z);
    uint16_t accelerated_velocity = (uint16_t)(ae_rd16(
        r, record + AE_GROUND_SPARK_VERTICAL_VELOCITY) +
        AE_GROUND_SPARK_VERTICAL_ACCELERATION);
    ae_wr16(r, record + AE_GROUND_SPARK_VERTICAL_VELOCITY, accelerated_velocity);
    if (ae_ram_failed(r)) return;

    int32_t ground_y = ae_rds16(r, record + AE_GROUND_SPARK_GROUND_Y);
    if (ground_y >= ae_rds16(r, record + AE_GROUND_SPARK_POSITION_Y)) return;

    ae_wr16(r, record + AE_GROUND_SPARK_POSITION_Y,
            ae_rd16(r, record + AE_GROUND_SPARK_GROUND_Y));
    int32_t velocity_x = ground_spark_half_toward_zero(move_x);
    int32_t velocity_z = ground_spark_half_toward_zero(move_z);
    int32_t accelerated_vertical_velocity = ae_rds16(
        r, record + AE_GROUND_SPARK_VERTICAL_VELOCITY);
    int32_t vertical_velocity = ground_spark_half_toward_zero(
        -accelerated_vertical_velocity);
    if (vertical_velocity < AE_GROUND_SPARK_BOUNCE_VELOCITY_MIN) {
        vertical_velocity = AE_GROUND_SPARK_BOUNCE_VELOCITY_MIN;
    }
    ae_wr8(r, record + AE_GROUND_SPARK_MOVE_X, (uint8_t)velocity_x);
    ae_wr8(r, record + AE_GROUND_SPARK_MOVE_Z, (uint8_t)velocity_z);
    ae_wr16(r, record + AE_GROUND_SPARK_VERTICAL_VELOCITY,
            (uint16_t)vertical_velocity);

    uint32_t life = (ae_rd8(r, record + AE_GROUND_SPARK_LIFE) - 1u) & 0xFFu;
    ae_wr8(r, record + AE_GROUND_SPARK_LIFE, life);
    if (life == 0u || ae_ram_failed(r)) return;

#ifdef AE_EFFECTS_TEST
    effects_test_record_spawn_particle(4, record);
#endif
    ae_SpawnParticle(r, spawn, 4, record);
}

/* GroundSparksUpdateDraw @80070560 rotates each active spark, asks the native
 * renderer to project it, then applies movement and ground-bounce behavior. */
void ae_GroundSparksUpdateDraw(AeRam *r, const AeSpawnHooks *spawn,
                               const AeEffectsHooks *hooks) {
    for (uint32_t byte = 0; byte < 32u; ++byte) {
        ae_wr8(r, AE_G_GROUND_SPARK_CAMERA_MATRIX + byte,
               ae_rd8(r, AE_G_EFFECTS_CAMERA_MATRIX + byte));
    }
    if (ae_ram_failed(r)) return;

    for (uint32_t index = 0; index < AE_GROUND_SPARK_COUNT; ++index) {
        uint32_t record = AE_G_GROUND_SPARKS + index * AE_GROUND_SPARK_SIZE;
        if (ae_rd8(r, record + AE_GROUND_SPARK_LIFE) == 0u) continue;

        ground_spark_update_spin(r, record);
        uint32_t matrix = AE_G_GROUND_SPARK_TEMP_MATRIX;
        ground_spark_build_matrix(r, record, matrix);
        if (ae_ram_failed(r)) return;

        if (!ground_spark_call(r, hooks,
                               hooks == NULL ? NULL : hooks->SetRotMatrix,
                               "SetRotMatrix", matrix, 0, 0, 0)) return;
        if (!ground_spark_call(r, hooks,
                               hooks == NULL ? NULL : hooks->SetTransMatrix,
                               "SetTransMatrix", matrix, 0, 0, 0)) return;

        int visible = 0;
        if (!ground_spark_render(r, hooks, record, matrix,
                                 ae_rd8(r, record + AE_GROUND_SPARK_SCALE),
                                 &visible)) return;
        if (!visible) continue;

        ground_spark_bounce(r, record, spawn);
        if (ae_ram_failed(r)) return;
    }
}
