#include "../core/ae_missa.h"

#include "ae_camera.h"
#include "../core/ae_sdk.h"

#define AE_MISSA_RC_CAR_REFERENCE       0x800F5A20u
#define AE_MISSA_RC_CAR_REFERENCE_X     0x24u
#define AE_MISSA_RC_CAR_REFERENCE_Y     0x28u
#define AE_MISSA_RC_CAR_REFERENCE_Z     0x2Cu
#define AE_MISSA_RC_CAR_STEP_X          0x30u
#define AE_MISSA_RC_CAR_STEP_Z          0x38u
#define AE_MISSA_CAMERA_MATRIX          0x800E78C0u
#define AE_MISSA_PLAYER                 0x800EC1E0u
#define AE_MISSA_PLAYER_POSITION_X      0x24u
#define AE_MISSA_PLAYER_POSITION_Z      0x2Cu
#define AE_MISSA_RIGHT_STICK_MAGNITUDE   0x800E54A2u
#define AE_MISSA_CAMERA_STATE            0x800E68D0u
#define AE_MISSA_CAMERA_KIND             0x06u
#define AE_MISSA_CAMERA_SCRATCH          0x1F800000u
#define AE_MISSA_CAMERA_DIRECTION        0x40u
#define AE_MISSA_RC_CAR_LENGTH_LIMIT     385
#define AE_MISSA_RC_CAR_MOVE_DIVIDEND    0x2900
#define AE_MISSA_RC_CAR_MOVE_SCALE       40
#define AE_MISSA_RC_CAR_MOVE_SHIFT       5u
#define AE_MISSA_RC_CAR_MOVE_MIN         16
#define AE_MISSA_RC_CAR_MOVE_MAX         40
#define AE_MISSA_RC_CAR_STICK_LIMIT      21u
#define AE_MISSA_RC_CAR_MOVE_FLAG        0x80u
#define AE_MISSA_RC_CAR_DIRECTION_FLAG   0x81u
#define AE_MISSA_RC_CAR_STACK_FRAME      64u
#define AE_MISSA_RC_CAR_VECTOR_STACK_OFF 0x10u
#define AE_MISSA_RC_CAR_SCREEN_STACK_OFF 0x18u
#define AE_MISSA_RC_CAR_DEPTH_STACK_OFF  0x1Cu

static int32_t missa_rc_s32(uint32_t value)
{
    if ((value & 0x80000000u) == 0u) return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

static int32_t missa_rc_renderer_call(AeRam *r, const AeCameraHooks *camera,
                                      AeCameraHook callback, const char *name,
                                      uint32_t a0, uint32_t a1,
                                      uint32_t a2, uint32_t a3)
{
    if (camera == NULL || callback == NULL) {
        ae_ram_fail(r, "CameraRcCarFollow: %s hook missing", name);
        return 0;
    }
    return callback(camera->user, r, a0, a1, a2, a3);
}

/* 8004AA58. Camera state and threshold logic stay in C; PsyQ matrix setup and
 * projection calls retain their original callback boundary. */
void ae_CameraRcCarFollow(AeRam *r, const AeMissaHooks *hooks,
                          uint32_t camera_slot)
{
    const AeCameraHooks *camera = hooks != NULL ? hooks->camera : NULL;
    const uint32_t reference = AE_MISSA_RC_CAR_REFERENCE;
    const uint32_t player = AE_MISSA_PLAYER;
    const uint32_t stack_pointer = hooks != NULL && hooks->stack_pointer != 0u
        ? hooks->stack_pointer : 0x80200000u;
    const uint32_t local_base = stack_pointer - AE_MISSA_RC_CAR_STACK_FRAME;
    const uint32_t input_vector = local_base + AE_MISSA_RC_CAR_VECTOR_STACK_OFF;
    const uint32_t projected_xy = local_base + AE_MISSA_RC_CAR_SCREEN_STACK_OFF;
    const uint32_t projected_depth = local_base + AE_MISSA_RC_CAR_DEPTH_STACK_OFF;

    uint16_t target_x = ae_rd16(r, reference + AE_MISSA_RC_CAR_REFERENCE_X);
    uint16_t target_y = ae_rd16(r, reference + AE_MISSA_RC_CAR_REFERENCE_Y);
    uint16_t target_z = ae_rd16(r, reference + AE_MISSA_RC_CAR_REFERENCE_Z);
    uint32_t step_x = ae_rd32(r, reference + AE_MISSA_RC_CAR_STEP_X) << 2;
    uint32_t step_z = ae_rd32(r, reference + AE_MISSA_RC_CAR_STEP_Z) << 2;

    ae_wr16(r, input_vector, (uint32_t)target_x + step_x);
    ae_wr16(r, input_vector + 2u, target_y);
    ae_wr16(r, input_vector + 4u, (uint32_t)target_z + step_z);

    (void)missa_rc_renderer_call(r, camera,
        camera != NULL ? camera->SetRotMatrix : NULL, "SetRotMatrix",
        AE_MISSA_CAMERA_MATRIX, 0, 0, 0);
    (void)missa_rc_renderer_call(r, camera,
        camera != NULL ? camera->SetTransMatrix : NULL, "SetTransMatrix",
        AE_MISSA_CAMERA_MATRIX, 0, 0, 0);
    int32_t projected_distance = missa_rc_renderer_call(r, camera,
        camera != NULL ? camera->RotTransPers : NULL, "RotTransPers",
        input_vector, projected_xy, projected_depth, projected_depth);
    if (projected_distance <= 0) projected_distance = 1;

    uint32_t dx = (uint32_t)target_x - ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X);
    uint32_t dz = (uint32_t)target_z - ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_Z);
    uint32_t distance_squared = dx * dx + dz * dz;
    int32_t distance = ae_sdk_SquareRoot0(r, missa_rc_s32(distance_squared));

    ae_wr8(r, camera_slot + AE_MISSA_RC_CAR_MOVE_FLAG, 0u);
    if (distance >= AE_MISSA_RC_CAR_LENGTH_LIMIT &&
        ae_rd16(r, AE_MISSA_RIGHT_STICK_MAGNITUDE) != 0u) {
        int32_t move_base = AE_MISSA_RC_CAR_MOVE_DIVIDEND /
                            projected_distance;
        int32_t move_frames = move_base + 16;
        if (move_frames <= 0) move_frames = 1;
        else if (move_frames > AE_MISSA_RC_CAR_MOVE_MAX)
            move_frames = AE_MISSA_RC_CAR_MOVE_MAX;
        int32_t scaled_frames = (move_frames * AE_MISSA_RC_CAR_MOVE_SCALE) /
                                (1 << AE_MISSA_RC_CAR_MOVE_SHIFT);
        if (scaled_frames < AE_MISSA_RC_CAR_MOVE_MIN)
            scaled_frames = AE_MISSA_RC_CAR_MOVE_MIN;
        else if (scaled_frames > AE_MISSA_RC_CAR_MOVE_MAX)
            scaled_frames = AE_MISSA_RC_CAR_MOVE_MAX;
        ae_wr8(r, camera_slot + AE_MISSA_RC_CAR_MOVE_FLAG,
               (uint32_t)scaled_frames);
    }

    if (ae_rd16(r, AE_MISSA_RIGHT_STICK_MAGNITUDE) >= AE_MISSA_RC_CAR_STICK_LIMIT)
        ae_wr8(r, camera_slot + AE_MISSA_RC_CAR_DIRECTION_FLAG, 1u);

    if (ae_rd16(r, AE_MISSA_CAMERA_STATE + AE_MISSA_CAMERA_KIND) == 1u &&
        ae_rd8(r, camera_slot + AE_MISSA_RC_CAR_DIRECTION_FLAG) != 0u) {
        uint32_t direction = AE_MISSA_CAMERA_SCRATCH + AE_MISSA_CAMERA_DIRECTION;
        ae_wr16(r, direction,
                (uint32_t)target_x - ae_rd16(r, player + AE_MISSA_PLAYER_POSITION_X));
        ae_wr16(r, direction + 2u,
                (uint32_t)target_y - ae_rd16(r, player + AE_MISSA_PLAYER_POSITION_X + 4u));
        ae_wr16(r, direction + 4u,
                (uint32_t)target_z - ae_rd16(r, player + AE_MISSA_PLAYER_POSITION_Z));
        (void)ae_sdk_VectorNormalSS(r, direction, direction);
    }

    ae_CameraFollowUpdate(r, camera, camera_slot, 0);
}
