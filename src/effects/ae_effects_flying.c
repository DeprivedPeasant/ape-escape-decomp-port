/* Per-frame motion and native-renderer dispatch for the twelve flying meshes. */
#include "ae_effects.h"

#include "../core/ae_sdk_matrix.h"
#include "../math/native_sdk_matrix.h"

#include <string.h>

#ifdef AE_EFFECTS_TEST
void effects_test_record_spawn_particle(int32_t type, uint32_t position);
#endif

static int flying_mesh_compose_local_matrix(AeRam *r, uint32_t record,
                                             uint32_t matrix) {
    uint8_t left[32];
    uint8_t right[32];
    uint8_t result[32];

    for (uint32_t byte = 0; byte < sizeof(left); ++byte) {
        left[byte] = (uint8_t)ae_rd8(r, matrix + byte);
        right[byte] = (uint8_t)ae_rd8(r, record + byte);
    }
    if (ae_ram_failed(r)) return 0;
    memcpy(result, left, sizeof(result));
    if (!ae_sdk_mul_matrix0(left, right, result)) {
        ae_ram_fail(r, "MulMatrix0 failed for flying mesh matrix");
        return 0;
    }
    for (uint32_t byte = 0; byte < sizeof(result); ++byte) {
        ae_wr8(r, matrix + byte, result[byte]);
    }
    return !ae_ram_failed(r);
}

static void flying_mesh_write_translation(AeRam *r, uint32_t record,
                                          uint32_t matrix) {
    ae_wr32(r, matrix + 20u, (uint32_t)ae_rds16(r, record + AE_FM_POSITION_X));
    ae_wr32(r, matrix + 24u, (uint32_t)ae_rds16(r, record + AE_FM_POSITION_Y));
    ae_wr32(r, matrix + 28u, (uint32_t)ae_rds16(r, record + AE_FM_POSITION_Z));
}

static uint32_t flying_mesh_ordering_table(AeRam *r) {
    uint32_t slot = ae_rd32(r, AE_G_FLYING_MESH_OT_STATE + 12u);
    uint32_t offset = slot << 7;
    offset += slot;
    offset <<= 4;
    offset -= slot;
    offset <<= 3;
    return AE_G_FLYING_MESH_OT_BASE + offset;
}

static int flying_mesh_build_transform(AeRam *r, uint32_t record,
                                       uint32_t matrix) {
    ae_sdk_RotMatrixZYX(r, record + AE_FM_ANGLE_X, matrix);
    if (ae_ram_failed(r)) return 0;
    flying_mesh_write_translation(r, record, matrix);
    if (ae_ram_failed(r) || !flying_mesh_compose_local_matrix(r, record, matrix)) return 0;
    ae_sdk_CompMatrix(r, AE_G_FLYING_MESH_CAMERA_MATRIX, matrix, matrix);
    return !ae_ram_failed(r);
}

static void flying_mesh_update_angles(AeRam *r, uint32_t record) {
    static const uint8_t angle_offsets[3] = {
        AE_FM_ANGLE_X, AE_FM_ANGLE_Y, AE_FM_ANGLE_Z
    };
    static const uint8_t velocity_offsets[3] = {
        AE_FM_ANGULAR_VELOCITY_X, AE_FM_ANGULAR_VELOCITY_Y,
        AE_FM_ANGULAR_VELOCITY_Z
    };
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t angle = ae_rd16(r, record + angle_offsets[axis]);
        uint32_t velocity = ae_rd16(r, record + velocity_offsets[axis]);
        ae_wr16(r, record + angle_offsets[axis], angle + velocity);
    }
}

static void flying_mesh_update_position(AeRam *r, uint32_t record) {
    static const uint8_t position_offsets[3] = {
        AE_FM_POSITION_X, AE_FM_POSITION_Y, AE_FM_POSITION_Z
    };
    static const uint8_t velocity_offsets[3] = {
        AE_FM_VELOCITY_X, AE_FM_VELOCITY_Y, AE_FM_VELOCITY_Z
    };
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t position = ae_rd16(r, record + position_offsets[axis]);
        uint32_t velocity = ae_rd16(r, record + velocity_offsets[axis]);
        ae_wr16(r, record + position_offsets[axis], position + velocity);
    }
}

/* 80071370: animate live mesh records, project/draw them, and apply ground
 * response. RTPT and lit packet emission stay behind the named renderer hooks. */
void ae_FlyingMeshesUpdateDraw(AeRam *r, const AeSpawnHooks *spawn,
                               const AeEffectsHooks *hooks) {
    for (uint32_t index = 0; index < AE_FLYING_MESH_COUNT; ++index) {
        uint32_t record = AE_G_FLYING_MESHES + index * AE_FLYING_MESH_SIZE;
        int32_t state = ae_rds8(r, record + AE_FM_STATE);
        if (state == 0) continue;

        uint32_t age = (ae_rd8(r, record + AE_FM_AGE) + 1u) & 0xFFu;
        ae_wr8(r, record + AE_FM_AGE, age);
        if (state < 2 && (age & 1u) != 0u) continue;

        uint32_t matrix = AE_G_FLYING_MESH_TEMP_MATRIX;
        if (!flying_mesh_build_transform(r, record, matrix)) return;

        int32_t model_id = ae_rds16(r, record + AE_FM_MODEL_ID);
        uint32_t models = ae_rd32(r, AE_G_FLYING_MESH_MODEL_TABLE);
        uint32_t model = models + ((uint32_t)model_id << 5);
        ae_MeshTransform(r, hooks, model, matrix,
                         AE_G_FLYING_MESH_OUTPUT_VERTICES);
        if (ae_ram_failed(r)) return;

        ae_MeshDraw(r, hooks, flying_mesh_ordering_table(r),
                    AE_G_FLYING_MESH_OUTPUT_VERTICES, model);
        if (ae_ram_failed(r)) return;

        flying_mesh_update_angles(r, record);
        flying_mesh_update_position(r, record);
        uint32_t vertical_velocity =
            (ae_rd16(r, record + AE_FM_VELOCITY_Y) + AE_FM_GRAVITY) & 0xFFFFu;
        ae_wr16(r, record + AE_FM_VELOCITY_Y, vertical_velocity);

        int32_t position_y = ae_rds16(r, record + AE_FM_POSITION_Y);
        int32_t ground_y = ae_rds16(r, record + AE_FM_GROUND_Y);
        if (ae_rds16(r, record + AE_FM_VELOCITY_Y) >= 0 && position_y > ground_y) {
            if (spawn == NULL) {
                ae_ram_fail(r, "AeSpawnHooks not provided for flying mesh impact");
                return;
            }
            ae_wr16(r, record + AE_FM_POSITION_Y, (uint32_t)ground_y);
#ifdef AE_EFFECTS_TEST
            effects_test_record_spawn_particle(5, record + AE_FM_POSITION_X);
#endif
            ae_SpawnParticle(r, spawn, 5, record + AE_FM_POSITION_X);
            uint32_t remaining = (ae_rd8(r, record + AE_FM_STATE) - 1u) & 0xFFu;
            ae_wr8(r, record + AE_FM_STATE, remaining);
            if (remaining != 0u) {
                int32_t velocity = ae_rds16(r, record + AE_FM_VELOCITY_Y);
                ae_wr16(r, record + AE_FM_VELOCITY_Y, (uint32_t)(-velocity / 3));
            }
        }
        if (ae_ram_failed(r)) return;
    }
}
