/* State-6 player spark motion and triangle-renderer dispatch. */
#include "ae_effects.h"

enum {
    AE_FX_PLAYER_STATE6_COUNT = 8,
    AE_FX_PLAYER_STATE6_RECORD_SIZE = 12,
    AE_FX_PLAYER_STATE6_VECTOR_Y = 2,
    AE_FX_PLAYER_STATE6_VECTOR_Z = 4,
    AE_FX_PLAYER_STATE6_VECTOR_PADDING = 6,
    AE_FX_PLAYER_STATE6_POSITION_X = 0,
    AE_FX_PLAYER_STATE6_POSITION_Y = 2,
    AE_FX_PLAYER_STATE6_POSITION_Z = 4,
    AE_FX_PLAYER_STATE6_LIFE = 6,
    AE_FX_PLAYER_STATE6_VELOCITY_X = 8,
    AE_FX_PLAYER_STATE6_VELOCITY_Y = 9,
    AE_FX_PLAYER_STATE6_VELOCITY_Z = 10,
    AE_FX_PLAYER_STATE6_GEOMETRY_VERTEX_1 = 8,
    AE_FX_PLAYER_STATE6_GEOMETRY_VERTEX_2 = 16,
    AE_FX_PLAYER_STATE6_GEOMETRY = 0x1F800010u,
    AE_FX_PLAYER_STATE6_TRIANGLE_COUNT = 2,
    AE_FX_PLAYER_STATE6_OT_PACKET_SIZE = 32,
    AE_FX_PLAYER_STATE6_HOOK_POSITION = 0x1F8002C0u,
    AE_FX_PLAYER_STATE6_HOOK_VELOCITY = 0x1F8002D0u
};

static int fx_player_state6_call(AeRam *r, const AeEffectsHooks *hooks,
                                 AeEffectsCallFn callback, const char *name,
                                 uint32_t a0, uint32_t a1,
                                 uint32_t a2, uint32_t a3) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return 0;
    }
    (void)callback(hooks->user, r, a0, a1, a2, a3);
    return !ae_ram_failed(r);
}

static void fx_player_state6_write_vec(AeRam *r, uint32_t address,
                                       uint16_t x, uint16_t y, uint16_t z) {
    ae_wr16(r, address, x);
    ae_wr16(r, address + AE_FX_PLAYER_STATE6_VECTOR_Y, y);
    ae_wr16(r, address + AE_FX_PLAYER_STATE6_VECTOR_Z, z);
    ae_wr16(r, address + AE_FX_PLAYER_STATE6_VECTOR_PADDING, 0);
}

static int fx_player_state6_spawn(AeRam *r, const AeEffectsHooks *hooks) {
    uint8_t old_position[8];
    uint8_t old_velocity[8];
    for (uint32_t byte = 0; byte < 8u; ++byte) {
        old_position[byte] = (uint8_t)ae_rd8(r, AE_FX_PLAYER_STATE6_HOOK_POSITION + byte);
        old_velocity[byte] = (uint8_t)ae_rd8(r, AE_FX_PLAYER_STATE6_HOOK_VELOCITY + byte);
    }

    fx_player_state6_write_vec(r, AE_FX_PLAYER_STATE6_HOOK_POSITION,
                               (uint16_t)ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_X),
                               (uint16_t)ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_Y),
                               (uint16_t)ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_Z));
    fx_player_state6_write_vec(r, AE_FX_PLAYER_STATE6_HOOK_VELOCITY,
                               (uint16_t)(0u - ae_rd16(r, AE_G_PLAYER + AE_PLAYER_VEL_X)),
                               0,
                               (uint16_t)(0u - ae_rd16(r, AE_G_PLAYER + AE_PLAYER_VEL_Z)));
    if (ae_ram_failed(r)) return 0;

    int success = fx_player_state6_call(r, hooks,
                                        hooks == NULL ? NULL : hooks->SparkAddRandom,
                                        "SparkAddRandom",
                                        AE_FX_PLAYER_STATE6_HOOK_POSITION,
                                        AE_FX_PLAYER_STATE6_HOOK_VELOCITY,
                                        0, 0);
    for (uint32_t byte = 0; byte < 8u; ++byte) {
        ae_wr8(r, AE_FX_PLAYER_STATE6_HOOK_POSITION + byte, old_position[byte]);
        ae_wr8(r, AE_FX_PLAYER_STATE6_HOOK_VELOCITY + byte, old_velocity[byte]);
    }
    return success && !ae_ram_failed(r);
}

static void fx_player_state6_make_geometry(AeRam *r, uint32_t record,
                                           int32_t radius) {
    uint16_t position[3] = {
        (uint16_t)ae_rd16(r, record + AE_FX_PLAYER_STATE6_POSITION_X),
        (uint16_t)ae_rd16(r, record + AE_FX_PLAYER_STATE6_POSITION_Y),
        (uint16_t)ae_rd16(r, record + AE_FX_PLAYER_STATE6_POSITION_Z)
    };
    int32_t velocity[3] = {
        ae_rds8(r, record + AE_FX_PLAYER_STATE6_VELOCITY_X),
        ae_rds8(r, record + AE_FX_PLAYER_STATE6_VELOCITY_Y),
        ae_rds8(r, record + AE_FX_PLAYER_STATE6_VELOCITY_Z)
    };

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr16(r, AE_FX_PLAYER_STATE6_GEOMETRY + axis * 2u,
                (uint16_t)(position[axis] - radius));
        ae_wr16(r, AE_FX_PLAYER_STATE6_GEOMETRY +
                   AE_FX_PLAYER_STATE6_GEOMETRY_VERTEX_1 + axis * 2u,
                (uint16_t)(position[axis] + radius));
        ae_wr16(r, AE_FX_PLAYER_STATE6_GEOMETRY +
                   AE_FX_PLAYER_STATE6_GEOMETRY_VERTEX_2 + axis * 2u,
                (uint16_t)(position[axis] + velocity[axis] * 6));
    }
}

static void fx_player_state6_swap_quad_edges(AeRam *r, uint32_t record,
                                              int32_t radius) {
    uint16_t position[3] = {
        (uint16_t)ae_rd16(r, record + AE_FX_PLAYER_STATE6_POSITION_X),
        (uint16_t)ae_rd16(r, record + AE_FX_PLAYER_STATE6_POSITION_Y),
        (uint16_t)ae_rd16(r, record + AE_FX_PLAYER_STATE6_POSITION_Z)
    };
    /* The original updates only the X and Y lanes between triangles; both Z
     * coordinates remain from the first source pass. */
    for (uint32_t axis = 0; axis < 2u; ++axis) {
        ae_wr16(r, AE_FX_PLAYER_STATE6_GEOMETRY + axis * 2u,
                (uint16_t)(position[axis] + radius));
        ae_wr16(r, AE_FX_PLAYER_STATE6_GEOMETRY +
                   AE_FX_PLAYER_STATE6_GEOMETRY_VERTEX_1 + axis * 2u,
                (uint16_t)(position[axis] - radius));
    }
}

static void fx_player_state6_update_record(AeRam *r, uint32_t record) {
    static const uint8_t positions[3] = {
        AE_FX_PLAYER_STATE6_POSITION_X,
        AE_FX_PLAYER_STATE6_POSITION_Y,
        AE_FX_PLAYER_STATE6_POSITION_Z
    };
    static const uint8_t velocities[3] = {
        AE_FX_PLAYER_STATE6_VELOCITY_X,
        AE_FX_PLAYER_STATE6_VELOCITY_Y,
        AE_FX_PLAYER_STATE6_VELOCITY_Z
    };

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        int32_t position = (int32_t)ae_rd16(r, record + positions[axis]);
        int32_t velocity = ae_rds8(r, record + velocities[axis]);
        ae_wr16(r, record + positions[axis], (uint16_t)(position + velocity));
    }
}

/* FxPlayerState6Update @8006E1C0 updates eight spark records. The native
 * renderer receives each triangle's source vertices; game-owned age, motion,
 * geometry and ordering-table cursor updates stay in RAM. */
void ae_FxPlayerState6Update(AeRam *r, const AeEffectsHooks *hooks) {
    if (ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE) == 6u) {
        if (!fx_player_state6_spawn(r, hooks)) return;
    }

    if (!fx_player_state6_call(r, hooks,
                               hooks == NULL ? NULL : hooks->SetRotMatrix,
                               "SetRotMatrix", AE_G_EFFECTS_CAMERA_MATRIX,
                               0, 0, 0)) return;
    if (!fx_player_state6_call(r, hooks,
                               hooks == NULL ? NULL : hooks->SetTransMatrix,
                               "SetTransMatrix", AE_G_EFFECTS_CAMERA_MATRIX,
                               0, 0, 0)) return;

    uint32_t ordering_table_pointer = ae_rd32(r, AE_G_FX_PLAYER_STATE6_OT_POINTER);
    for (int32_t index = AE_FX_PLAYER_STATE6_COUNT - 1; index >= 0; --index) {
        uint32_t record = AE_G_FX_PLAYER_STATE6 +
                          (uint32_t)index * AE_FX_PLAYER_STATE6_RECORD_SIZE;
        int32_t old_life = ae_rds8(r, record + AE_FX_PLAYER_STATE6_LIFE);
        if (old_life == 0) continue;

        fx_player_state6_update_record(r, record);
        ae_wr8(r, record + AE_FX_PLAYER_STATE6_LIFE,
               ae_rd8(r, record + AE_FX_PLAYER_STATE6_LIFE) - 1u);
        if (ae_ram_failed(r)) return;

        int32_t radius = old_life + 1;
        fx_player_state6_make_geometry(r, record, radius);
        for (uint32_t triangle = 0;
             triangle < AE_FX_PLAYER_STATE6_TRIANGLE_COUNT; ++triangle) {
            if (!fx_player_state6_call(r, hooks,
                                       hooks == NULL ? NULL : hooks->GpuDrawFxPlayerState6,
                                       "GpuDrawFxPlayerState6", record,
                                       AE_FX_PLAYER_STATE6_GEOMETRY,
                                       AE_FX_PLAYER_STATE6_TRIANGLE_COUNT - triangle,
                                       (uint32_t)radius)) return;
            ordering_table_pointer += AE_FX_PLAYER_STATE6_OT_PACKET_SIZE;
            if (triangle + 1u < AE_FX_PLAYER_STATE6_TRIANGLE_COUNT) {
                fx_player_state6_swap_quad_edges(r, record, radius);
            }
        }
    }
    ae_wr32(r, AE_G_FX_PLAYER_STATE6_OT_POINTER, ordering_table_pointer);
}
