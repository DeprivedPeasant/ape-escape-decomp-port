/* Effect object transforms, aging, movement and collision-body creation. */
#include "ae_effects.h"

#include "../bodies/ae_bodies.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

enum {
    AE_EFFECT_OBJECT_ANGLE_SCRATCH = 0x1F800170u,
    AE_EFFECT_OBJECT_ANGLE_SCRATCH_Y = 2,
    AE_EFFECT_OBJECT_ANGLE_SCRATCH_Z = 4,
    AE_EFFECT_OBJECT_MATRIX_TRANSLATION_X = 20,
    AE_EFFECT_OBJECT_MATRIX_TRANSLATION_Y = 24,
    AE_EFFECT_OBJECT_MATRIX_TRANSLATION_Z = 28,
    AE_EFFECT_OBJECT_VERTEX_0_Y = 0x1AC,
    AE_EFFECT_OBJECT_VERTEX_0_Z = 0x1AE,
    AE_EFFECT_OBJECT_VERTEX_1_X = 0x1B0,
    AE_EFFECT_OBJECT_VERTEX_1_Y = 0x1B4,
    AE_EFFECT_OBJECT_VERTEX_1_Z = 0x1B6,
    AE_EFFECT_OBJECT_VERTEX_1_PADDING = 0x1B8,
    AE_EFFECT_BODY_POSITION_X = 0x00,
    AE_EFFECT_BODY_POSITION_Y = 0x04,
    AE_EFFECT_BODY_POSITION_Z = 0x08,
    AE_EFFECT_BODY_RADIUS = 0x20,
    AE_EFFECT_BODY_HEIGHT = 0x22,
    AE_EFFECT_BODY_PARAMETER = 0x24,
    AE_EFFECT_BODY_FLAGS = 0x26,
    AE_EFFECT_BODY_DEFAULT_SOLID = 512
};

static int effect_object_call(AeRam *r, const AeEffectsHooks *hooks,
                              AeEffectsCallFn callback, const char *name,
                              uint32_t a0, uint32_t a1, uint32_t a2) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return 0;
    }
    (void)callback(hooks->user, r, a0, a1, a2, 0);
    return !ae_ram_failed(r);
}

static void effect_object_write_rotation(AeRam *r, uint32_t matrix,
                                         int16_t angle) {
    uint8_t saved[6];
    for (uint32_t i = 0; i < sizeof(saved); ++i) {
        saved[i] = (uint8_t)ae_rd8(r, AE_EFFECT_OBJECT_ANGLE_SCRATCH + i);
    }

    ae_wr16(r, AE_EFFECT_OBJECT_ANGLE_SCRATCH, 0);
    ae_wr16(r, AE_EFFECT_OBJECT_ANGLE_SCRATCH + AE_EFFECT_OBJECT_ANGLE_SCRATCH_Y, 0);
    ae_wr16(r, AE_EFFECT_OBJECT_ANGLE_SCRATCH + AE_EFFECT_OBJECT_ANGLE_SCRATCH_Z,
            (uint16_t)angle);
    ae_sdk_RotMatrix(r, AE_EFFECT_OBJECT_ANGLE_SCRATCH, matrix);

    for (uint32_t i = 0; i < sizeof(saved); ++i) {
        ae_wr8(r, AE_EFFECT_OBJECT_ANGLE_SCRATCH + i, saved[i]);
    }
}

static int effect_object_compose_matrix(AeRam *r, uint32_t object,
                                        uint32_t matrix) {
    ae_sdk_MatrixIdentity(r, matrix);
    if (ae_ram_failed(r)) return 0;

    effect_object_write_rotation(r, matrix,
                                 ae_rds16(r, object + AE_EFFECT_OBJECT_ROTATION));
    if (ae_ram_failed(r)) return 0;

    ae_sdk_CompMatrix(r, object + AE_EFFECT_OBJECT_ROTATION_MATRIX, matrix, matrix);
    if (ae_ram_failed(r)) return 0;

    ae_wr32(r, matrix + AE_EFFECT_OBJECT_MATRIX_TRANSLATION_X,
            (uint32_t)ae_rds16(r, object + AE_EFFECT_OBJECT_POSITION_X));
    ae_wr32(r, matrix + AE_EFFECT_OBJECT_MATRIX_TRANSLATION_Y,
            (uint32_t)ae_rds16(r, object + AE_EFFECT_OBJECT_POSITION_Y));
    ae_wr32(r, matrix + AE_EFFECT_OBJECT_MATRIX_TRANSLATION_Z,
            (uint32_t)ae_rds16(r, object + AE_EFFECT_OBJECT_POSITION_Z));
    if (ae_ram_failed(r)) return 0;

    ae_sdk_CompMatrix(r, AE_G_EFFECTS_CAMERA_MATRIX, matrix, matrix);
    return !ae_ram_failed(r);
}

/* EffectObjDraw @8006FA10 builds a rotated world matrix and passes the object
 * parameters and transform to the native renderer for projection and packets. */
void ae_EffectObjDraw(AeRam *r, const AeEffectsHooks *hooks, uint32_t object) {
    uint32_t matrix = AE_G_EFFECT_OBJECT_CAMERA_MATRIX;
    if (!effect_object_compose_matrix(r, object, matrix)) return;

    if (!effect_object_call(r, hooks, hooks == NULL ? NULL : hooks->SetRotMatrix,
                            "SetRotMatrix", matrix, 0, 0)) return;
    if (!effect_object_call(r, hooks, hooks == NULL ? NULL : hooks->SetTransMatrix,
                            "SetTransMatrix", matrix, 0, 0)) return;

    /* These source vectors are consumed by the original projection loop. */
    uint32_t vertices = AE_G_EFFECT_OBJECT_TEMP_MATRIX;
    ae_wr16(r, vertices + AE_EFFECT_OBJECT_VERTEX_1_X,
            ae_rd16(r, object + AE_EFFECT_OBJECT_SIZE_X));
    ae_wr16(r, vertices + AE_EFFECT_OBJECT_VERTEX_1_Y,
            ae_rd16(r, object + AE_EFFECT_OBJECT_RADIUS_Y));
    ae_wr16(r, vertices + AE_EFFECT_OBJECT_VERTEX_1_Z, 0);
    ae_wr16(r, vertices + AE_EFFECT_OBJECT_VERTEX_0_Y,
            (uint16_t)(ae_rd16(r, object + AE_EFFECT_OBJECT_RADIUS_X) +
                       ae_rd16(r, object + AE_EFFECT_OBJECT_SIZE_Y)));
    ae_wr16(r, vertices + AE_EFFECT_OBJECT_VERTEX_0_Z,
            ae_rd16(r, object + AE_EFFECT_OBJECT_SIZE_Z));
    ae_wr16(r, vertices + AE_EFFECT_OBJECT_VERTEX_1_PADDING, 0);
    if (ae_ram_failed(r)) return;

    (void)effect_object_call(r, hooks,
                             hooks == NULL ? NULL : hooks->GpuDrawEffectObj,
                             "GpuDrawEffectObj", object, matrix,
                             AE_G_EFFECT_OBJECT_TEMP_MATRIX + AE_EFFECT_OBJECT_VERTEX_0_Y);
}

static void effect_object_update_motion(AeRam *r, uint32_t object) {
    int32_t angular_velocity = ae_rds16(r, object + AE_EFFECT_OBJECT_ROTATION);
    int32_t x_velocity = ae_rds16(r, object + AE_EFFECT_OBJECT_VELOCITY_X);
    int32_t y_velocity = ae_rds16(r, object + AE_EFFECT_OBJECT_VELOCITY_Y);
    int32_t z_velocity = ae_rds16(r, object + AE_EFFECT_OBJECT_VELOCITY_Z);

    ae_wr32(r, object + AE_EFFECT_OBJECT_RENDER_COLOR,
            ae_rd32(r, object + AE_EFFECT_OBJECT_RENDER_COLOR) -
            ae_rd32(r, object + AE_EFFECT_OBJECT_COLOR_STEP));
    ae_wr16(r, object + AE_EFFECT_OBJECT_SIZE_X,
            ae_rd16(r, object + AE_EFFECT_OBJECT_SIZE_X) +
            ae_rd16(r, object + AE_EFFECT_OBJECT_VELOCITY_X));
    ae_wr16(r, object + AE_EFFECT_OBJECT_RADIUS_X,
            ae_rd16(r, object + AE_EFFECT_OBJECT_RADIUS_X) +
            ae_rd16(r, object + AE_EFFECT_OBJECT_VELOCITY_Y));
    ae_wr16(r, object + AE_EFFECT_OBJECT_RADIUS_Y,
            ae_rd16(r, object + AE_EFFECT_OBJECT_RADIUS_Y) +
            ae_rd16(r, object + AE_EFFECT_OBJECT_VELOCITY_Z));

    /* The signed multiply-and-shift sequence rounds negative products toward
     * zero before applying the arithmetic shift. */
    int32_t damped_angle = angular_velocity * 7;
    if (damped_angle < 0) damped_angle += 7;
    ae_wr16(r, object + AE_EFFECT_OBJECT_ROTATION, damped_angle >> 3);

    int32_t damped_x = x_velocity * 7;
    if (damped_x < 0) damped_x += 7;
    ae_wr16(r, object + AE_EFFECT_OBJECT_VELOCITY_X, damped_x >> 3);

    int32_t damped_y = y_velocity * 15;
    if (damped_y < 0) damped_y += 15;
    ae_wr16(r, object + AE_EFFECT_OBJECT_VELOCITY_Y, damped_y >> 4);

    int32_t damped_z = z_velocity * 15;
    if (damped_z < 0) damped_z += 15;
    ae_wr16(r, object + AE_EFFECT_OBJECT_VELOCITY_Z, damped_z >> 4);
}

static int32_t effect_object_body_height(int32_t width, int32_t depth) {
    int32_t difference = width - depth;
    int32_t height = difference < 0 ? -difference : difference;
    return height < 73 ? height : height - 64;
}

static uint32_t effect_object_create_body(AeRam *r, uint32_t object) {
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (ae_ram_failed(r) || body == 0u) return body;

    int32_t width = ae_rds16(r, object + AE_EFFECT_OBJECT_RADIUS_X);
    int32_t depth = ae_rds16(r, object + AE_EFFECT_OBJECT_RADIUS_Y);
    int32_t radius_sum = (int16_t)(width + depth);
    int32_t radius = (radius_sum + (radius_sum < 0 ? 1 : 0)) >> 1;
    ae_wr32(r, body + AE_EFFECT_BODY_POSITION_X,
            (uint32_t)ae_rds16(r, object + AE_EFFECT_OBJECT_POSITION_X));
    ae_wr32(r, body + AE_EFFECT_BODY_POSITION_Y,
            (uint32_t)ae_rds16(r, object + AE_EFFECT_OBJECT_POSITION_Y));
    ae_wr32(r, body + AE_EFFECT_BODY_POSITION_Z,
            (uint32_t)ae_rds16(r, object + AE_EFFECT_OBJECT_POSITION_Z));
    ae_wr16(r, body + AE_EFFECT_BODY_RADIUS, (uint16_t)radius);
    ae_wr16(r, body + AE_EFFECT_BODY_HEIGHT,
            (uint16_t)effect_object_body_height(width, depth));
    ae_wr16(r, body + AE_EFFECT_BODY_PARAMETER,
            ae_rd16(r, object + AE_EFFECT_OBJECT_BODY_TYPE) | 1u);
    ae_wr16(r, body + AE_EFFECT_BODY_FLAGS, AE_EFFECT_BODY_DEFAULT_SOLID);
    if (ae_ram_failed(r)) return 0;

    return ae_ram_failed(r) ? 0u : body;
}

/* EffectObjsUpdate @8006FEE8 ages and moves the sixteen records, draws them,
 * and creates type-16 collision bodies for objects marked for registration. */
void ae_EffectObjsUpdate(AeRam *r, const AeEffectsHooks *hooks) {
    for (uint32_t index = 0; index < AE_EFFECT_OBJECT_COUNT; ++index) {
        uint32_t object = AE_G_EFFECT_OBJECTS + index * AE_EFFECT_OBJECT_SIZE;
        uint32_t active = ae_rd8(r, object + AE_EFFECT_OBJECT_ACTIVE);
        if (active == 0u) continue;

        uint32_t timer = ae_rd8(r, object + AE_EFFECT_OBJECT_TIMER);
        if (timer != 0u) {
            ae_wr8(r, object + AE_EFFECT_OBJECT_TIMER, timer - 1u);
            continue;
        }

        ae_wr8(r, object + AE_EFFECT_OBJECT_ACTIVE, active - 1u);
        ae_EffectObjDraw(r, hooks, object);
        if (ae_ram_failed(r)) return;

        effect_object_update_motion(r, object);
        if (ae_ram_failed(r)) return;

        uint32_t body = ae_rd32(r, object + AE_EFFECT_OBJECT_BODY);
        if (body != 0u && (ae_rd32(r, body + AE_EFFECT_BODY_CONTACT) >> 28) == 6u) {
            ae_wr8(r, object + AE_EFFECT_OBJECT_SPAWN_BODY, 0);
            continue;
        }
        if (ae_rd8(r, object + AE_EFFECT_OBJECT_SPAWN_BODY) == 0u) continue;

        body = effect_object_create_body(r, object);
        if (ae_ram_failed(r)) return;
        if (body == 0u) continue;
        ae_wr32(r, object + AE_EFFECT_OBJECT_BODY, body);
        if (ae_ram_failed(r)) return;
        ae_CollisionBodyRegister(r, NULL, 16, body, 0);
        if (ae_ram_failed(r)) return;
    }
}
