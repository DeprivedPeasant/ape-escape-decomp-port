/* Readable RAM-image ports for the CameraUpdate call tree. */
#include "ae_camupd.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../world/ae_stage.h"

/* Existing exact port in src/player/ae_player_spawn.c. */
void ae_CameraActivateSlot(AeRam *r, uint32_t index);
void ae_EventCamSetScript(AeRam *r, uint32_t slot_index, int32_t script_index,
                          uint32_t group, uint32_t script_set);

static int32_t camupd_s32(uint32_t bits)
{
    if (bits & 0x80000000u)
        return (int32_t)(bits & 0x7FFFFFFFu) - 0x7FFFFFFF - 1;
    return (int32_t)bits;
}

static int32_t camupd_s16(uint32_t bits)
{
    return camupd_s32((bits & 0xFFFFu) | ((bits & 0x8000u) ? 0xFFFF0000u : 0u));
}

static int32_t camupd_s8(uint32_t bits)
{
    return camupd_s32((bits & 0xFFu) | ((bits & 0x80u) ? 0xFFFFFF00u : 0u));
}

static void camupd_copy8(AeRam *r, uint32_t destination, uint32_t source)
{
    uint8_t bytes[8];
    uint32_t i;

    for (i = 0; i < sizeof(bytes); ++i)
        bytes[i] = (uint8_t)ae_rd8(r, source + i);
    for (i = 0; i < sizeof(bytes); ++i)
        ae_wr8(r, destination + i, bytes[i]);
}

static void camupd_copy16(AeRam *r, uint32_t destination, uint32_t source)
{
    uint8_t bytes[16];
    uint32_t i;

    for (i = 0; i < sizeof(bytes); ++i)
        bytes[i] = (uint8_t)ae_rd8(r, source + i);
    for (i = 0; i < sizeof(bytes); ++i)
        ae_wr8(r, destination + i, bytes[i]);
}

static int32_t camupd_arithmetic_shift(int32_t value, uint32_t shift)
{
    uint32_t bits = (uint32_t)value >> shift;
    if (value < 0)
        bits |= UINT32_MAX << (32u - shift);
    return camupd_s32(bits);
}

static int32_t camupd_trunc_shift(int32_t value, uint32_t shift)
{
    uint32_t bias = (1u << shift) - 1u;
    uint32_t bits = (uint32_t)value;

    if (value < 0)
        bits += bias;
    return camupd_arithmetic_shift(camupd_s32(bits), shift);
}

static int32_t camupd_call(AeRam *r, const AeCamupdHooks *h,
                           AeCamupdHookFn function, const char *name,
                           uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (h == NULL || function == NULL) {
        ae_ram_fail(r, "CameraUpdate hook %s was not provided", name);
        return 0;
    }
    return function(h->user, r, a0, a1, a2, a3);
}

static int32_t camupd_camera_atan(void *user, AeRam *r,
                                  uint32_t x, uint32_t z,
                                  uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a2;
    (void)a3;
    return ae_Atan2Safe(r, camupd_s32(x), camupd_s32(z));
}

static int32_t camupd_camera_normalize(void *user, AeRam *r,
                                       uint32_t input, uint32_t output,
                                       uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a2;
    (void)a3;
    return ae_sdk_VectorNormalSS(r, input, output);
}

static int32_t camupd_camera_get_player_dir(void *user, AeRam *r,
                                            uint32_t slot, uint32_t mode,
                                            uint32_t output, uint32_t a3)
{
    (void)user;
    (void)a3;
    ae_CameraGetPlayerDir(r, slot, mode, output);
    return 0;
}

static int32_t camupd_camera_sin(void *user, AeRam *r, uint32_t angle,
                                 uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a1;
    (void)a2;
    (void)a3;
    return ae_sdk_rsin(r, camupd_s32(angle));
}

static int32_t camupd_camera_cos(void *user, AeRam *r, uint32_t angle,
                                 uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a1;
    (void)a2;
    (void)a3;
    return ae_sdk_rcos(r, camupd_s32(angle));
}

static int32_t camupd_camera_memset(void *user, AeRam *r, uint32_t destination,
                                    uint32_t value, uint32_t count,
                                    uint32_t a3)
{
    const AeCamupdHooks *h = (const AeCamupdHooks *)user;
    uint32_t hook_destination = destination == AE_CAMERA_TEMP_AIM
        ? AE_CAMUPD_CAMERA_AIM_TEMP_ABI
        : destination;
    uint32_t offset;
    int32_t result;

    (void)a3;
    result = camupd_call(r, h, h ? h->memset : NULL, "memset",
                         hook_destination, value, count, 0);
    if (hook_destination != destination) {
        for (offset = 0; offset < count; ++offset)
            ae_wr8(r, destination + offset, value);
    }
    return result;
}

static int32_t camupd_camera_area_script(void *user, AeRam *r,
                                         uint32_t mode, uint32_t slot,
                                         uint32_t a2, uint32_t a3)
{
    const AeCamupdHooks *h = (const AeCamupdHooks *)user;
    return camupd_call(r, h, h ? h->CameraAreaScriptDispatch : NULL,
                       "CameraAreaScriptDispatch", mode, slot, a2, a3);
}

static AeCameraHooks camupd_camera_math_hooks(const AeCamupdHooks *h)
{
    AeCameraHooks camera = {0};
    if (h != NULL) {
        camera = h->camera;
        camera.user = (void *)h;
    }
    camera.Atan2Safe = camupd_camera_atan;
    camera.VectorNormalSS = camupd_camera_normalize;
    camera.CameraGetPlayerDir = camupd_camera_get_player_dir;
    camera.rsin = camupd_camera_sin;
    camera.rcos = camupd_camera_cos;
    camera.memset = camupd_camera_memset;
    camera.CameraAreaScriptDispatch = camupd_camera_area_script;
    return camera;
}

/* 80014168 */
void ae_PadRumble(AeRam *r, const AeCamupdHooks *h,
                  int32_t port, uint32_t small_motor, uint32_t large_motor)
{
    if (ae_rd8(r, AE_CAMUPD_G_RUMBLE_ALLOWED) == 0u)
        return;
    if (camupd_call(r, h, h ? h->PadGetState : NULL, "PadGetState",
                    0, 0, 0, 0) != 6)
        return;

    if (port == 0) {
        ae_wr8(r, AE_CAMUPD_G_RUMBLE_STATE, small_motor);
        ae_wr8(r, AE_CAMUPD_G_RUMBLE_STATE + 2u, large_motor);
    } else if (port == 1) {
        ae_wr8(r, AE_CAMUPD_G_RUMBLE_STATE + 1u, small_motor);
        ae_wr8(r, AE_CAMUPD_G_RUMBLE_STATE + 3u, large_motor);
    }
}

/* 800154DC */
void ae_CopySvecAt1C(AeRam *r, uint32_t actor, uint32_t destination)
{
    camupd_copy8(r, destination, actor + AE_CAMUPD_COPY_SVEC_OFFSET);
}

/* 80046340 */
int32_t ae_Atan2Safe(AeRam *r, int32_t x, int32_t z)
{
    if (z == 0)
        return x < 0 ? 0x800 : 0;
    return ae_sdk_ratan2(r, z, x);
}

/* 800470CC */
void ae_CameraGetEye(AeRam *r, uint32_t destination)
{
    camupd_copy8(r, destination, AE_G_CAMERA_EYE);
}

/* 8004A940 */
int32_t ae_IsFlightArea(AeRam *r)
{
    uint32_t area;

    if (ae_rd16(r, AE_G_PLAYER_MODE) != AE_CAMUPD_FLIGHT_PLAYER_MODE)
        return 0;
    area = ae_rd16(r, AE_G_CURRENT_AREA);
    return area - AE_CAMUPD_FLIGHT_AREA_FIRST < AE_CAMUPD_FLIGHT_AREA_COUNT;
}

/* 8004B880 */
void ae_CameraGetPlayerDir(AeRam *r, uint32_t slot, uint32_t mode,
                           uint32_t destination)
{
    (void)slot;
    if (mode == 0u) {
        camupd_copy8(r, destination, AE_CAMUPD_PLAYER_DIR);
    } else if (mode == 1u || mode == 2u) {
        ae_wr16(r, destination, 0);
        ae_wr16(r, destination + AE_CAMUPD_VEC_Y, 0);
        ae_wr16(r, destination + AE_CAMUPD_VEC_Z, 0);
    }
}

/* 80047DA4 */
void ae_CameraSnapshotPlayer(AeRam *r)
{
    uint32_t player = AE_G_PLAYER;

    ae_CopySvecAt1C(r, player, AE_CAMUPD_SCR_PLAYER_POS);
    camupd_copy8(r, AE_CAMUPD_SCR_PLAYER_DIR,
                 player + AE_PLAYER_FACING);

    ae_wr16(r, AE_SCRATCH_BASE + AE_CAM_SCR_FOCUS,
            ae_rd16(r, player + AE_PLAYER_POSITION));
    ae_wr16(r, AE_SCRATCH_BASE + AE_CAM_SCR_FOCUS + AE_CAMUPD_VEC_Y,
            ae_rd16(r, player + AE_PLAYER_POSITION_Y));
    ae_wr16(r, AE_SCRATCH_BASE + AE_CAM_SCR_FOCUS + AE_CAMUPD_VEC_Z,
            ae_rd16(r, player + AE_PLAYER_POSITION_Z));
    ae_wr32(r, AE_CAMUPD_SCR_CAMERA_VELOCITY,
            ae_rd32(r, player + AE_CAMUPD_PLAYER_LINEAR_VELOCITY));
    ae_wr32(r, AE_CAMUPD_SCR_CAMERA_VELOCITY + AE_CAMUPD_VELOCITY_Y,
            ae_rd32(r, player + AE_CAMUPD_PLAYER_LINEAR_VELOCITY + AE_CAMUPD_VELOCITY_Y));
    ae_wr32(r, AE_CAMUPD_SCR_CAMERA_VELOCITY + AE_CAMUPD_VELOCITY_Z,
            ae_rd32(r, player + AE_CAMUPD_PLAYER_LINEAR_VELOCITY + AE_CAMUPD_VELOCITY_Z));
    ae_wr32(r, AE_CAMUPD_SCR_CAMERA_FOCUS,
            ae_rd32(r, player + AE_CAMUPD_PLAYER_CAM_FOCUS));

    camupd_copy8(r, AE_CAMUPD_CAMERA_SNAPSHOT_POS,
                 AE_CAMUPD_SCR_PLAYER_POS);
    camupd_copy8(r, AE_CAMUPD_CAMERA_SNAPSHOT_DIR,
                 AE_CAMUPD_SCR_PLAYER_DIR);
}

/* 80046DE8 */
void ae_CameraSelectMode(AeRam *r, const AeCamupdHooks *h)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t slots = ae_rd32(r, AE_G_CAMERA_SLOTS);
    uint32_t selected = ae_rd8(r, state + AE_CAM_G_FOLLOW_INDEX);
    uint32_t kind = 1u;
    uint32_t region_slot = ae_rd8(r, state + AE_CAM_G_REGION_SLOT);
    uint32_t style = 0u;
    uint32_t active = ae_rd8(r, state + AE_CAM_G_ACTIVE_INDEX);

    if (region_slot != 0xFFu) {
        uint32_t candidate = slots + region_slot * AE_CAMERA_SLOT_SIZE;
        selected = region_slot;
        kind = ae_rd8(r, candidate + AE_CAM_REGION_FLAG) == 1u ? 5u : 4u;
    }
    if (ae_rd8(r, AE_CAMUPD_G_FORCE_FOLLOW) != 0u) {
        selected = ae_rd8(r, state + AE_CAM_G_FOLLOW_INDEX);
        kind = 1u;
    }
    if (ae_rd8(r, state + AE_CAM_G_FIXED_ACTIVE) == 1u) {
        selected = ae_rd8(r, AE_CAMUPD_G_FIXED_INDEX);
        kind = 3u;
    }
    if ((ae_rd8(r, state + AE_CAM_G_MODE_TIMER) == 1u ||
         ae_rd8(r, state + AE_CAM_G_AIM_MODE) == 1u) &&
        ae_IsFlightArea(r) == 0) {
        selected = ae_rd8(r, AE_CAMUPD_G_AIM_INDEX);
        kind = 2u;
    }
    if (ae_rd8(r, state + AE_CAM_G_REMOVE_GUARD) != 0u) {
        selected = ae_rd8(r, state + AE_CAM_G_REGION_SLOT);
        kind = 4u;
    }
    if (selected == active)
        return;

    if (active == ae_rd8(r, AE_CAMUPD_G_AIM_INDEX) ||
        selected == ae_rd8(r, AE_CAMUPD_G_AIM_INDEX)) {
        style = 2u;
    } else if (kind == 3u || ae_rd8(r, AE_CAMUPD_G_ACTIVE_KIND) == 3u) {
        style = 3u;
    } else if (kind == 5u) {
        uint32_t candidate = slots + selected * AE_CAMERA_SLOT_SIZE;
        if (ae_rd8(r, candidate + AE_CAM_FIXED_ACTIVE) != 0u) {
            style = 3u;
        } else {
            uint32_t prior = slots + active * AE_CAMERA_SLOT_SIZE;
            uint32_t delta = AE_CAMERA_TEMP_AIM;
            ae_wr16(r, delta,
                    ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POSITION_X) -
                    ae_rd16(r, prior + AE_CAM_EYE_X));
            ae_wr16(r, delta + AE_CAMUPD_VEC_Y, 0u);
            ae_wr16(r, delta + AE_CAMUPD_VEC_Z,
                    ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POSITION_Z) -
                    ae_rd16(r, prior + AE_CAM_EYE_Z));
            (void)ae_sdk_VectorNormalSS(r, delta, delta);
            {
                uint32_t dot =
                    (uint32_t)ae_rds16(r, candidate + AE_CAM_SCRIPT_FOCUS) *
                    (uint32_t)ae_rds16(r, delta) +
                    (uint32_t)ae_rds16(r, candidate + AE_CAM_SCRIPT_FOCUS + AE_CAMUPD_VEC_Z) *
                    (uint32_t)ae_rds16(r, delta + AE_CAMUPD_VEC_Z);
                if (camupd_s32(dot) < 0)
                    return;
            }
        }
    } else if (kind == 1u &&
               (uint8_t)(ae_rd8(r, AE_CAMUPD_G_ACTIVE_KIND) - 4u) < 2u) {
        AeCameraHooks camera = camupd_camera_math_hooks(h);
        ae_CameraSlotSnapshotEye(r, &camera);
        style = 1u;
    }

    ae_wr16(r, AE_CAMUPD_G_BLEND_TIMER, 0x80u);
    ae_wr8(r, AE_CAMUPD_G_BLEND_STYLE, style);
    if (kind == 4u && ae_rd8(r, state + AE_CAM_G_REMOVE_GUARD) != 0u)
        ae_wr16(r, AE_CAMUPD_G_BLEND_TIMER, 0u);
    camupd_copy8(r, AE_CAMUPD_G_BLEND_EYE, AE_G_CAMERA_EYE);
    camupd_copy8(r, AE_CAMUPD_G_BLEND_TARGET, AE_G_CAMERA_TARGET);
    ae_wr8(r, state + AE_CAM_G_ACTIVE_INDEX, selected);
    ae_wr8(r, AE_CAMUPD_G_ACTIVE_KIND, kind);
    ae_CameraActivateSlot(r, selected);
}

/* 80047FF0 */
void ae_CameraApplySlot(AeRam *r)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t timer = ae_rd16(r, AE_CAMUPD_G_BLEND_TIMER);
    uint32_t slot = ae_rd32(r, state + AE_CAM_G_ACTIVE_SLOT);

    if (timer == 0u) {
        camupd_copy16(r, AE_G_CAMERA_EYE, slot + AE_CAM_EYE);
        return;
    }

    switch (ae_rd8(r, AE_CAMUPD_G_BLEND_STYLE)) {
    case 0:
        timer = (uint16_t)(timer -
                           (timer < AE_CAMUPD_BLEND_STYLE1_FAST_END ? 1u : 3u));
        break;
    case 1:
        if (timer < AE_CAMUPD_BLEND_STYLE2_FAST_END)
            timer = (uint32_t)(7u * timer) / 8u;
        else
            timer -= 8u;
        break;
    case 2:
        timer = (uint32_t)(3u * timer) / 4u;
        break;
    case 3:
        timer = 0u;
        break;
    default:
        timer = (uint16_t)(timer -
                           (timer < AE_CAMUPD_BLEND_STYLE1_FAST_END ? 1u : 3u));
        break;
    }
    timer = (uint16_t)timer;
    ae_wr16(r, AE_CAMUPD_G_BLEND_TIMER, timer);

    for (uint32_t axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
        uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
        int32_t old_eye = ae_rds16(r, AE_CAMUPD_G_BLEND_EYE + component);
        int32_t new_eye = ae_rds16(r, slot + AE_CAM_EYE + component);
        int32_t old_target = ae_rds16(r, AE_CAMUPD_G_BLEND_TARGET + component);
        int32_t new_target = ae_rds16(r, slot + AE_CAM_TARGET + component);
        int32_t remaining = (int32_t)AE_CAMUPD_BLEND_TIME_SCALE - (int32_t)timer;
        int32_t eye_mix = camupd_s32((uint32_t)old_eye * timer +
                                     (uint32_t)new_eye * (uint32_t)remaining);
        int32_t target_mix = camupd_s32((uint32_t)old_target * timer +
                                        (uint32_t)new_target * (uint32_t)remaining);

        ae_wr16(r, AE_G_CAMERA_EYE + component,
                (uint32_t)camupd_trunc_shift(eye_mix, 7));
        ae_wr16(r, AE_G_CAMERA_TARGET + component,
                (uint32_t)camupd_trunc_shift(target_mix, 7));
    }
}

/* 80048BBC */
void ae_CameraShakeApply(AeRam *r)
{
    uint32_t shake = AE_CAMUPD_G_SHAKE;
    uint32_t remaining = ae_rd8(r, shake + AE_CAMUPD_SHAKE_COUNTER);
    int32_t offset[AE_CAMUPD_VEC_COMPONENT_COUNT];
    uint32_t component;

    if (remaining == 0u)
        return;

    remaining = (remaining - 1u) & 0xFFu;
    ae_wr8(r, shake + AE_CAMUPD_SHAKE_COUNTER, remaining);
    offset[0] = (int16_t)(0u - ae_rd16(r, shake + AE_CAMUPD_SHAKE_X));
    offset[1] = (int16_t)(0u - ae_rd16(r, shake + AE_CAMUPD_SHAKE_Y));
    offset[2] = (int16_t)(0u - ae_rd16(r, shake + AE_CAMUPD_SHAKE_Z));
    for (component = 0; component < AE_CAMUPD_VEC_COMPONENT_COUNT; ++component)
        ae_wr16(r, shake + component * AE_CAMUPD_VEC_COMPONENT_SIZE,
                (uint32_t)offset[component]);

    if (remaining < 4u) {
        for (component = 0; component < AE_CAMUPD_VEC_COMPONENT_COUNT; ++component) {
            offset[component] = camupd_trunc_shift(offset[component], 1u);
            ae_wr16(r, shake + component * AE_CAMUPD_VEC_COMPONENT_SIZE,
                    (uint32_t)offset[component]);
        }
    }

    for (component = 0; component < AE_CAMUPD_VEC_COMPONENT_COUNT; ++component) {
        uint32_t delta = component * AE_CAMUPD_VEC_COMPONENT_SIZE;
        int32_t target_delta = camupd_trunc_shift(offset[component] * 3, 1u);

        ae_wr16(r, AE_G_CAMERA_EYE + delta,
                ae_rd16(r, AE_G_CAMERA_EYE + delta) + (uint32_t)offset[component]);
        ae_wr16(r, AE_G_CAMERA_TARGET + delta,
                ae_rd16(r, AE_G_CAMERA_TARGET + delta) + (uint32_t)target_delta);
    }
}

/* 8004BD98 */
int32_t ae_CameraRegionTest(AeRam *r, uint32_t position)
{
    uint32_t count = ae_rd16(r, AE_CAMUPD_G_REGION_COUNT);
    uint32_t regions = ae_rd32(r, AE_CAMUPD_G_REGION_TABLE);
    uint32_t index;

    for (index = 0; index < count; ++index) {
        uint32_t region = regions + index * AE_CAMUPD_REGION_SIZE;
        uint32_t flags = ae_rd16(r, region + AE_CAMUPD_REGION_FLAGS);
        int32_t center_x;
        int32_t center_y;
        int32_t center_z;
        int32_t half_x;
        int32_t half_y;
        int32_t half_z;
        int32_t min_x;
        int32_t max_x;
        int32_t min_y;
        int32_t max_y;
        int32_t min_z;
        int32_t max_z;
        int32_t x;
        int32_t y;
        int32_t z;

        if ((flags & 0x6000u) == 0x6000u || (flags & 0x8300u) == 0x8100u)
            continue;

        center_x = camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_CENTER_X));
        center_y = camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_CENTER_Y));
        center_z = camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_CENTER_Z));
        half_x = camupd_trunc_shift(
            camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_EXTENT_X)), 1u);
        half_y = camupd_trunc_shift(
            camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_EXTENT_Y)), 1u);
        half_z = camupd_trunc_shift(
            camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_EXTENT_Z)), 1u);
        min_x = camupd_s16((uint32_t)(center_x - half_x));
        max_x = camupd_s16((uint32_t)(center_x + half_x));
        min_y = camupd_s16((uint32_t)(center_y - half_y));
        max_y = camupd_s16((uint32_t)(center_y + half_y));
        min_z = camupd_s16((uint32_t)(center_z - half_z));
        max_z = camupd_s16((uint32_t)(center_z + half_z));

        x = camupd_s16(ae_rd16(r, position));
        y = camupd_s16(ae_rd16(r, position + AE_CAMUPD_VEC_Y));   /* compared with the Y bounds (8004BEE0) */
        z = camupd_s16(ae_rd16(r, position + AE_CAMUPD_VEC_Z));   /* compared with the Z bounds (8004BEBC) */
        if (min_x < x && x < max_x &&
            min_y < y && y < max_y &&
            min_z < z && z < max_z)
            return (int32_t)index;
    }
    return -1;
}

/* 8004D134 */
void ae_EventCamLoadRecord(AeRam *r, uint32_t record)
{
    int32_t duration = camupd_s16(
        ae_rd16(r, record + AE_CAMUPD_EVENT_RECORD_TIME));
    uint32_t event = AE_CAMUPD_G_EVENT_CAMERA;
    uint32_t axis;

    if (duration == 0)
        duration = 1;
    ae_wr16(r, event + AE_CAMUPD_EVENT_FRAME_COUNT, (uint32_t)(duration + 1));
    camupd_copy8(r, event + AE_CAMUPD_EVENT_START_EYE,
                 record + AE_CAMUPD_EVENT_RECORD_EYE);

    for (axis = 0; axis < AE_CAMUPD_ROTATION_FIELDS; ++axis) {
        uint32_t component = axis * AE_CAMUPD_ROTATION_FIELD_SIZE;
        int32_t start = camupd_s16(ae_rd16(r, event + component));
        int32_t end = camupd_s16(ae_rd16(r,
            record + AE_CAMUPD_EVENT_RECORD_END + component));
        ae_wr16(r, event + AE_CAMUPD_EVENT_END_EYE + component,
                (uint32_t)((end - start) / duration));
    }

    (void)ae_sdk_RotMatrixZYX(r,
        record + AE_CAMUPD_EVENT_RECORD_START_ROT, AE_CAMUPD_ROTATION_TEMP);
    for (axis = 0; axis < AE_CAMUPD_ROTATION_FIELDS; ++axis) {
        uint32_t component = axis * AE_CAMUPD_ROTATION_FIELD_SIZE;
        uint32_t matrix_offset = AE_CAMUPD_MATRIX_FIRST_ROW +
                                 axis * AE_CAMUPD_MATRIX_ROW_SIZE;
        ae_wr16(r, event + AE_CAMUPD_EVENT_START_ROT + component,
                ae_rd16(r, AE_CAMUPD_ROTATION_TEMP + matrix_offset));
    }

    (void)ae_sdk_RotMatrixZYX(r,
        record + AE_CAMUPD_EVENT_RECORD_END_ROT, AE_CAMUPD_ROTATION_TEMP);
    for (axis = 0; axis < AE_CAMUPD_ROTATION_FIELDS; ++axis) {
        uint32_t component = axis * AE_CAMUPD_ROTATION_FIELD_SIZE;
        uint32_t matrix_offset = AE_CAMUPD_MATRIX_FIRST_ROW +
                                 axis * AE_CAMUPD_MATRIX_ROW_SIZE;
        int32_t start = camupd_s16(ae_rd16(r,
            event + AE_CAMUPD_EVENT_START_ROT + component));
        int32_t end = camupd_s16(ae_rd16(r,
            AE_CAMUPD_ROTATION_TEMP + matrix_offset));
        ae_wr16(r, event + AE_CAMUPD_EVENT_ROT_DELTA + component,
                (uint32_t)((end - start) / duration));
    }

    ae_wr16(r, event + AE_CAMUPD_EVENT_STATE, 4u);
    ae_wr8(r, event + AE_CAMUPD_EVENT_RECORD_KIND,
           ae_rd8(r, record + AE_CAMUPD_EVENT_RECORD_STATE));
}

/* 8004D39C */
void ae_EventCamStart(AeRam *r, const AeCamupdHooks *h, uint32_t record_index)
{
    uint32_t count = ae_rd16(r, AE_CAMUPD_G_EVENT_RECORD_COUNT);

    if (record_index >= count) {
        (void)camupd_call(r, h, h ? h->Printf : NULL, "Printf",
                          AE_CAMUPD_EVENT_START_ERROR_FORMAT, 0, 0, 0);
        return;
    }

    ae_wr8(r, AE_CAMUPD_G_EVENT_ACTIVE, 0);
    ae_EventCamLoadRecord(r,
        ae_rd32(r, AE_CAMUPD_G_EVENT_RECORDS) +
        record_index * AE_CAMUPD_EVENT_RECORD_SIZE);
    ae_wr8(r, AE_CAMUPD_G_EVENT_DONE, 0);
    ae_wr8(r, AE_CAMUPD_G_EVENT_SLOT, record_index);
    ae_CutsceneModeOff(r);
}

/* 8004BB68 */
int32_t ae_EventCamCreate(AeRam *r, const AeCamupdHooks *h,
                          int32_t script_index, uint32_t activate,
                          uint32_t script_set)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t eye = AE_CAMUPD_EVENT_CREATE_EYE_TEMP;
    int32_t slot;

    ae_CameraGetEye(r, eye);
    slot = ae_CameraSlotCreate(r, &camera, 4, eye);
    ae_EventCamSetScript(r, (uint32_t)slot, script_index, 0, script_set);
    if (activate == 0u) {
        ae_CameraActivateSlot(r, (uint32_t)slot);
        ae_wr8(r, AE_CAMUPD_EVENT_ACTIVE_SLOT, (uint32_t)slot);
    }
    return slot;
}

/* 8004BA0C */
void ae_EventCamTrigger(AeRam *r, const AeCamupdHooks *h, uint32_t trigger)
{
    switch (trigger) {
    case 2: {
        uint32_t area = ae_rd16(r, AE_CAMUPD_G_STAGE_STATE +
                                  AE_CAMUPD_STAGE_CURRENT_AREA);
        uint32_t entry = ae_rd16(r, AE_CAMUPD_G_STAGE_STATE +
                                   AE_CAMUPD_STAGE_ENTRY_INDEX);
        if (h == NULL || h->stage == NULL) {
            ae_ram_fail(r, "EventCamTrigger requires the verified StageRequest port");
            return;
        }
        (void)ae_StageRequest(r, h->stage, 19, (int32_t)area, (int32_t)entry);
        (void)ae_EventCamCreate(r, h, 1, 0, 0);
        ae_wr8(r, AE_CAMUPD_EVENT_TRIGGER_FLAG, 1);
        ae_wr16(r, AE_CAMUPD_G_BLEND_TIMER, 0);
        break;
    }
    case 3:
        (void)camupd_call(r, h, h ? h->SpawnParticle : NULL, "SpawnParticle",
                          281u, AE_CAMUPD_G_EVENT_TRIGGER_PARTICLE, 0, 0);
        ae_PadRumble(r, h, 1, 180u, 9u);
        break;
    case 4:
        ae_wr8(r, AE_CAMUPD_G_EVENT_TRIGGER_FLAG, 1);
        break;
    case 5:
        ae_wr8(r, AE_CAMUPD_G_EVENT_TRIGGER_FLAG, 0);
        break;
    default:
        break;
    }
}

/* 8004D6F4 */
void ae_StageExitRequest(AeRam *r, const AeCamupdHooks *h)
{
    uint32_t area = ae_rd16(r, AE_CAMUPD_G_STAGE_STATE +
                              AE_CAMUPD_STAGE_CURRENT_AREA);
    uint32_t entry = ae_rd16(r, AE_CAMUPD_G_STAGE_STATE +
                               AE_CAMUPD_STAGE_ENTRY_INDEX);

    ae_wr8(r, AE_CAMUPD_G_EVENT_RUNNING, 0);
    ae_wr16(r, AE_CAMUPD_G_EVENT_CAMERA + AE_CAMUPD_EVENT_FRAME_COUNT, 0);
    if (h == NULL || h->stage == NULL) {
        ae_ram_fail(r, "StageExitRequest requires the verified StageRequest port");
        return;
    }
    (void)ae_StageRequest(r, h->stage, 25, (int32_t)area, (int32_t)entry);
}

/* 8004D7A0 */
void ae_EventCamUpdate(AeRam *r, const AeCamupdHooks *h)
{
    int32_t active = camupd_s8(ae_rd8(r, AE_CAMUPD_G_EVENT_RUNNING));

    if (active != 0) {
        if (camupd_s16(ae_rd16(r,
                AE_CAMUPD_G_EVENT_CAMERA + AE_CAMUPD_EVENT_FRAME_COUNT)) == 0) {
            uint32_t remaining =
                (ae_rd8(r, AE_CAMUPD_G_EVENT_RUNNING) - 1u) & 0xFFu;
            ae_wr8(r, AE_CAMUPD_G_EVENT_RUNNING, remaining);
            if (camupd_s8(remaining) == 0) {
                ae_StageExitRequest(r, h);
            } else {
                int32_t index = camupd_s8(ae_rd8(r, AE_CAMUPD_G_EVENT_RECORD_INDEX));
                uint32_t record_offset = (uint32_t)(index *
                    (int32_t)AE_CAMUPD_EVENT_RECORD_SIZE);
                uint32_t records = ae_rd32(r, AE_CAMUPD_G_EVENT_RECORDS_ALT);

                ae_EventCamLoadRecord(r, records + record_offset);
                if (ae_rd8(r, AE_CAMUPD_G_EVENT_KIND) != 0u) {
                    ae_wr16(r, AE_CAMUPD_G_EVENT_CAMERA + AE_CAMUPD_EVENT_STATE, 0);
                    ae_wr8(r, AE_CAMUPD_G_EVENT_KIND, 0);
                }
                index = (index + 1) & 0xFF;
                ae_wr8(r, AE_CAMUPD_G_EVENT_RECORD_INDEX, (uint32_t)index);
                ae_wr8(r, AE_CAMUPD_G_EVENT_RECORD_SLOT, (uint32_t)index);
            }
        }

        if ((ae_rd32(r, AE_CAMUPD_G_EVENT_FLAGS_A) &
             ae_rd32(r, AE_CAMUPD_G_EVENT_FLAGS_B) & 0x8E0u) != 0u)
            ae_StageExitRequest(r, h);
    }
}

/* 8004D40C */
void ae_StageExitSequenceUpdate(AeRam *r, const AeCamupdHooks *h)
{
    uint32_t event = AE_CAMUPD_G_EVENT_CAMERA;
    uint32_t player = AE_G_PLAYER;
    uint32_t player_position = AE_CAMUPD_EXIT_TEMP_MEMSET;
    uint32_t position = AE_CAMUPD_EXIT_TEMP_POS;
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    int32_t frames;
    uint32_t axis;

    (void)camupd_call(r, h, h ? h->memset : NULL, "memset",
                      player_position, 0, AE_CAMUPD_VEC_SIZE, 0);
    for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
        uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
        ae_wr16(r, player_position + component,
                ae_rd16(r, player + AE_PLAYER_POSITION_X +
                           axis * AE_CAMUPD_PLAYER_POSITION_FIELD_SIZE));
    }
    camupd_copy8(r, position, player_position);

    frames = camupd_s16(ae_rd16(r, event + AE_CAMUPD_EVENT_FRAME_COUNT));
    if (frames == 0)
        return;
    frames--;
    ae_wr16(r, event + AE_CAMUPD_EVENT_FRAME_COUNT, (uint32_t)frames);

    if (frames == 0) {
        if (ae_rd8(r, AE_CAMUPD_G_EVENT_DONE) == 0u) {
            uint32_t event_kind = ae_rd8(r, AE_CAMUPD_G_EVENT_KIND);
            if (event_kind != 0u) {
                ae_EventCamStart(r, h, event_kind - 1u);
                ae_wr16(r, event + AE_CAMUPD_EVENT_STATE, 0);
            } else {
                uint32_t active_slot;
                uint32_t slots = ae_rd32(r, AE_G_CAMERA_SLOTS);
                ae_wr32(r, AE_CAMUPD_G_CAMERA_EXIT_CLEAR, 0);
                active_slot = ae_rd8(r, AE_G_CAMERA_STATE + AE_CAM_G_FOLLOW_INDEX);
                ae_CameraSlotPlaceBehindPlayer(r, &camera,
                    slots + active_slot * AE_CAMERA_SLOT_SIZE,
                    position, AE_CAMUPD_EXIT_SLOT_DISTANCE);
                ae_CutsceneModeOn(r);
                return;
            }
        } else if (camupd_s8(ae_rd8(r, AE_CAMUPD_G_EVENT_RUNNING)) == 0) {
            uint32_t active_slot;
            uint32_t slots = ae_rd32(r, AE_G_CAMERA_SLOTS);
            ae_wr32(r, AE_CAMUPD_G_CAMERA_EXIT_CLEAR, 0);
            active_slot = ae_rd8(r, AE_G_CAMERA_STATE + AE_CAM_G_FOLLOW_INDEX);
            ae_CameraSlotPlaceBehindPlayer(r, &camera,
                slots + active_slot * AE_CAMERA_SLOT_SIZE,
                position, AE_CAMUPD_EXIT_SLOT_DISTANCE);
            return;
        }
    }

    if (ae_rd8(r, AE_CAMUPD_G_EVENT_KIND) == 0u) {
        int32_t fade = camupd_s16(ae_rd16(r, event + AE_CAMUPD_EVENT_STATE));
        if (frames < 5 || fade != 0) {
            int32_t intensity;
            if (fade != 0) {
                intensity = camupd_s32((uint32_t)fade * 31u);
                ae_wr16(r, event + AE_CAMUPD_EVENT_STATE, (uint32_t)(fade - 1));
            } else {
                intensity = camupd_s32((uint32_t)(4 - frames) * 31u);
            }
            ae_ScreenFlashDraw(r, h, (uint32_t)intensity);
        }
    }

    {
        int32_t eye[AE_CAMUPD_ROTATION_FIELDS];
        int32_t target[AE_CAMUPD_ROTATION_FIELDS];

        for (axis = 0; axis < AE_CAMUPD_ROTATION_FIELDS; ++axis) {
            uint32_t component = axis * AE_CAMUPD_ROTATION_FIELD_SIZE;
            eye[axis] = camupd_s16(ae_rd16(r,
                event + AE_CAMUPD_EVENT_START_EYE + component));
            target[axis] = camupd_s16(ae_rd16(r,
                event + AE_CAMUPD_EVENT_START_ROT + component));
            ae_wr16(r, AE_CAMUPD_EXIT_TEMP_TARGET + component,
                    (uint32_t)(eye[axis] + target[axis]));
        }

        camupd_copy8(r, AE_CAMUPD_G_CAMERA_EXIT_BUFFER + AE_CAMUPD_EXIT_CAMERA_EYE,
                     event + AE_CAMUPD_EVENT_START_EYE);
        camupd_copy8(r, AE_CAMUPD_G_CAMERA_EXIT_BUFFER + AE_CAMUPD_EXIT_CAMERA_TARGET,
                     AE_CAMUPD_EXIT_TEMP_TARGET);

        for (axis = 0; axis < AE_CAMUPD_ROTATION_FIELDS; ++axis) {
            uint32_t component = axis * AE_CAMUPD_ROTATION_FIELD_SIZE;
            ae_wr16(r, event + AE_CAMUPD_EVENT_START_EYE + component,
                    ae_rd16(r, event + AE_CAMUPD_EVENT_START_EYE + component) +
                    ae_rd16(r, event + AE_CAMUPD_EVENT_END_EYE + component));
            ae_wr16(r, event + AE_CAMUPD_EVENT_START_ROT + component,
                    ae_rd16(r, event + AE_CAMUPD_EVENT_START_ROT + component) +
                    ae_rd16(r, event + AE_CAMUPD_EVENT_ROT_DELTA + component));
        }
    }
}

/* 8004CEB4: the native renderer consumes the game-selected frame image. */
void ae_CameraTransitionFrameDraw(AeRam *r, const AeCamupdHooks *h)
{
    uint32_t counter = ae_rd8(r, AE_CAMUPD_G_TRANSITION_COUNTER);
    uint32_t cursor;
    uint32_t display_index;
    uint32_t display_record;
    uint32_t image;
    uint32_t mode;

    if (counter == 0u)
        return;

    counter--;
    ae_wr8(r, AE_CAMUPD_G_TRANSITION_COUNTER, counter);
    cursor = ae_rd32(r, AE_CAMUPD_G_ORDER_TABLE_CURSOR);
    display_index = ae_rd32(r, AE_CAMUPD_G_DISPLAY_INDEX);
    display_record = AE_CAMUPD_G_DISPLAY_STATE +
                     display_index * AE_CAMUPD_DISPLAY_RECORD_STRIDE;
    image = ae_rd32(r, display_record + AE_CAMUPD_DISPLAY_RECORD_IMAGE);
    mode = ae_rd16(r, display_record + AE_CAMUPD_DISPLAY_RECORD_MODE);

    (void)camupd_call(r, h, h ? h->RenderCameraTransition : NULL,
                      "RenderCameraTransition", counter, cursor, image, mode);
    ae_wr32(r, AE_CAMUPD_G_ORDER_TABLE_CURSOR,
            cursor + AE_CAMUPD_TRANSITION_OT_ADVANCE);
}

/* 8004BF20 */
void ae_CameraRegionUpdate(AeRam *r, const AeCamupdHooks *h)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t regions = ae_rd32(r, AE_CAMUPD_G_REGION_TABLE);
    uint32_t player_position = AE_CAMUPD_REGION_TEMP_PLAYER;
    uint32_t test_position = AE_CAMUPD_REGION_TEST_POSITION;
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t delay = ae_rd8(r, state + AE_CAM_G_REGION_DELAY);
    int32_t candidate_index;
    uint32_t candidate;
    uint32_t old_index;

    (void)camupd_call(r, h, h ? h->memset : NULL, "memset",
                      player_position, 0, AE_CAMUPD_VEC_SIZE, 0);
    ae_wr16(r, player_position,
            ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POSITION_X));
    ae_wr16(r, player_position + AE_CAMUPD_VEC_Y,
            ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POSITION_Y) - 0x80u);
    ae_wr16(r, player_position + AE_CAMUPD_VEC_Z,
            ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POSITION_Z));
    camupd_copy8(r, test_position, player_position);

    if (delay != 0u) {
        delay--;
        ae_wr8(r, state + AE_CAM_G_REGION_DELAY, delay);
    }
    candidate_index = ae_CameraRegionTest(r, test_position);
    candidate = candidate_index < 0 ? 0xFFu : (uint32_t)candidate_index;
    old_index = ae_rd8(r, state + AE_CAM_G_REGION_OLD);
    if (candidate == old_index)
        return;

    delay = ae_rd8(r, state + AE_CAM_G_REGION_DELAY);
    if (delay == 0u) {
        delay = 1u;
        ae_wr8(r, state + AE_CAM_G_REGION_DELAY, delay);
    }
    if (delay != 1u)
        return;
    ae_wr8(r, state + AE_CAM_G_REGION_DELAY, 0);

    if (candidate == 0xFFu) {
        if (old_index != 0xFFu) {
            uint32_t old_region = regions + old_index * AE_CAMUPD_REGION_SIZE;
            uint32_t old_flags = ae_rd16(r, old_region + AE_CAMUPD_REGION_FLAGS);
            uint32_t old_slot = old_flags & AE_CAMUPD_REGION_SLOT_MASK;
            uint32_t kind = old_flags & AE_CAMUPD_REGION_KIND_MASK;
            if (old_slot != 0u &&
                (kind == 0u || kind == AE_CAMUPD_REGION_REMOVE_MASK))
                ae_CameraSlotRemove(r, &camera, old_slot);
        }
        ae_wr8(r, AE_CAM_G_REGION_SLOT + state, 0xFFu);
        ae_wr8(r, AE_CAMUPD_G_REGION_STATE_A, 0);
        ae_wr8(r, AE_CAMUPD_G_REGION_STATE_B, 0);
        ae_wr8(r, AE_CAMUPD_G_REGION_STATE_C, 0);
        ae_CameraSelectMode(r, h);
        ae_wr8(r, state + AE_CAM_G_REGION_OLD, candidate);
        return;
    }

    if (old_index != 0xFFu) {
        uint32_t old_region = regions + old_index * AE_CAMUPD_REGION_SIZE;
        uint32_t old_flags = ae_rd16(r, old_region + AE_CAMUPD_REGION_FLAGS);
        uint32_t old_slot = old_flags & AE_CAMUPD_REGION_SLOT_MASK;
        if (old_slot != 0u &&
            (old_flags & AE_CAMUPD_REGION_REMOVE_MASK) == 0u)
            ae_CameraSlotRemove(r, &camera, old_slot);
    }

    ae_wr8(r, state + AE_CAM_G_REGION_SLOT, 0xFFu);
    ae_wr8(r, AE_CAMUPD_G_REGION_STATE_A, 0);
    ae_wr8(r, AE_CAMUPD_G_REGION_STATE_B, 0);
    ae_wr8(r, AE_CAMUPD_G_REGION_STATE_C, 0);
    {
        uint32_t region = regions + candidate * AE_CAMUPD_REGION_SIZE;
        uint32_t flags = ae_rd16(r, region + AE_CAMUPD_REGION_FLAGS);
        uint32_t slot_index;
        uint32_t slot;
        uint32_t axis;

        for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
            uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
            static const uint32_t center_fields[3] = {
                AE_CAMUPD_REGION_CENTER_X,
                AE_CAMUPD_REGION_CENTER_Y,
                AE_CAMUPD_REGION_CENTER_Z,
            };
            static const uint32_t extent_fields[3] = {
                AE_CAMUPD_REGION_EXTENT_X,
                AE_CAMUPD_REGION_EXTENT_Y,
                AE_CAMUPD_REGION_EXTENT_Z,
            };
            int32_t extent = camupd_s16(
                ae_rd16(r, region + extent_fields[axis]));
            ae_wr16(r, AE_G_CAMERA_FOCUS + component,
                    ae_rd16(r, region + center_fields[axis]));
            ae_wr16(r, AE_CAMUPD_G_REGION_HALF_EXTENTS + component,
                    (uint32_t)camupd_trunc_shift(extent, 1u));
        }

        if ((flags & AE_CAMUPD_REGION_ACTIVE_MASK) == 0u) {
            flags |= AE_CAMUPD_REGION_ACTIVE_MASK;
            ae_wr16(r, region + AE_CAMUPD_REGION_FLAGS, flags);
        }
        slot_index = flags & AE_CAMUPD_REGION_SLOT_MASK;
        if (slot_index == 0u) {
            uint32_t activate =
                (flags & AE_CAMUPD_REGION_KIND_MASK) == AE_CAMUPD_REGION_KIND_EVENT;
            int32_t created = ae_EventCamCreate(r, h,
                camupd_s16(ae_rd16(r, region + AE_CAMUPD_REGION_SCRIPT_INDEX)),
                activate, 1u);
            uint32_t slots = ae_rd32(r, AE_G_CAMERA_SLOTS);

            slot_index = (uint32_t)created & AE_CAMUPD_REGION_SLOT_MASK;
            flags = (flags & 0xFFF0u) | slot_index;
            ae_wr16(r, region + AE_CAMUPD_REGION_FLAGS, flags);
            slot = slots + slot_index * AE_CAMERA_SLOT_SIZE;
            ae_wr8(r, slot + AE_CAM_REGION_LINK_INDEX, candidate);
        } else {
            uint32_t slots = ae_rd32(r, AE_G_CAMERA_SLOTS);
            slot = slots + slot_index * AE_CAMERA_SLOT_SIZE;
            ae_wr8(r, state + AE_CAM_G_REGION_SLOT, slot_index);
            ae_wr16(r, slot + AE_CAM_SCRIPT_TIMER,
                    ae_rd16(r, slot + AE_CAM_SCRIPT_STEP));
            if ((flags & AE_CAMUPD_REGION_KIND_MASK) == 0u) {
                camupd_copy8(r, slot + AE_CAM_EYE, AE_G_CAMERA_EYE);
                camupd_copy8(r, slot + AE_CAM_TARGET, AE_G_CAMERA_TARGET);
            }
        }
    }
    ae_wr8(r, state + AE_CAM_G_REGION_OLD, candidate);
}

static int32_t camupd_script_degrees_to_angle(int32_t degrees)
{
    int32_t scaled = camupd_s32((uint32_t)degrees << 12);
    int64_t product = (int64_t)scaled * (int32_t)0xB60B60B7u;
    int32_t high = camupd_s32((uint32_t)((uint64_t)product >> 32));
    int32_t quotient = camupd_arithmetic_shift(
        camupd_s32((uint32_t)high + (uint32_t)scaled), 8);

    return camupd_s32((uint32_t)quotient - (scaled < 0 ? UINT32_MAX : 0u));
}

static int32_t camupd_script_angle_remainder(int32_t angle)
{
    int32_t shifted = camupd_s32((uint32_t)angle + 0x1000u);
    int32_t adjusted = shifted < 0
        ? camupd_s32((uint32_t)shifted + 0x0FFFu)
        : shifted;
    int32_t turns = camupd_arithmetic_shift(adjusted, 12);

    return camupd_s32((uint32_t)shifted - ((uint32_t)turns << 12));
}

static int32_t camupd_script_mul_q12(int32_t value, int32_t scale)
{
    int32_t product = camupd_s32((uint32_t)((uint64_t)(int64_t)value *
                                             (int64_t)scale));
    return camupd_arithmetic_shift(product, 12);
}

static void camupd_script_set_timer(AeRam *r, uint32_t slot, uint32_t timer)
{
    ae_wr16(r, slot + AE_CAM_SCRIPT_TIMER, timer);
    ae_wr16(r, slot + AE_CAM_SCRIPT_STEP, timer);
}

static void camupd_script_move_from_focus(AeRam *r, uint32_t slot,
                                          uint32_t focus, int32_t yaw,
                                          int32_t distance, int32_t y_offset)
{
    int32_t cosine = ae_sdk_rcos(r, yaw);
    int32_t sine = ae_sdk_rsin(r, yaw);

    ae_wr16(r, slot + AE_CAM_EYE_X,
            ae_rd16(r, focus) + (uint32_t)camupd_script_mul_q12(cosine, distance));
    ae_wr16(r, slot + AE_CAM_EYE_Y,
            ae_rd16(r, focus + AE_CAMUPD_VEC_Y) + (uint32_t)y_offset);
    ae_wr16(r, slot + AE_CAM_EYE_Z,
            ae_rd16(r, focus + AE_CAMUPD_VEC_Z) + (uint32_t)camupd_script_mul_q12(sine, distance));
}

static void camupd_script_command_15(AeRam *r, const AeCamupdHooks *h,
                                     uint32_t slot, uint32_t *script)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t focus = AE_CAMUPD_SCRIPT_FOCUS_TEMP;
    uint32_t direction = AE_CAMUPD_SCRIPT_DIRECTION_TEMP;
    uint32_t mode = ae_rd8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION);
    int32_t offset_degrees = ae_rds16(r, *script);
    int32_t distance;
    int32_t y_offset;
    int32_t yaw;

    *script += 2u;
    distance = ae_rds16(r, *script);
    *script += 2u;
    (void)ae_CameraSlotGetFocusPoint(r, &camera, slot, mode, focus);
    ae_CameraGetPlayerDir(r, slot, mode, direction);
    yaw = ae_Atan2Safe(r, camupd_s16(ae_rd16(r, direction)),
                       camupd_s16(ae_rd16(r, direction + AE_CAMUPD_VEC_Z)));
    yaw = camupd_script_angle_remainder(
        camupd_s32((uint32_t)yaw + (uint32_t)camupd_script_degrees_to_angle(offset_degrees)));
    if (mode == 0u) {
        ae_wr32(r, slot + AE_CAM_YAW, (uint32_t)yaw);
        ae_wr32(r, slot + AE_CAM_DISTANCE, (uint32_t)distance);
    }
    y_offset = ae_rds16(r, *script);
    camupd_script_move_from_focus(r, slot, focus, yaw, distance, y_offset);
    camupd_copy8(r, slot + AE_CAM_TARGET, focus);
    *script += 2u;
}

static void camupd_script_command_16(AeRam *r, const AeCamupdHooks *h,
                                     uint32_t slot, uint32_t *script)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t focus = AE_CAMUPD_SCRIPT_FOCUS_TEMP;
    uint32_t mode = ae_rd8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION);
    int32_t offset_degrees = ae_rds16(r, *script);
    int32_t distance;
    int32_t y_offset;
    int32_t yaw;

    *script += 2u;
    (void)ae_CameraSlotGetFocusPoint(r, &camera, slot, mode, focus);
    distance = ae_rds16(r, *script);
    *script += 2u;
    yaw = camupd_script_degrees_to_angle(offset_degrees);
    if (mode == 0u)
        ae_wr32(r, slot + AE_CAM_DISTANCE, (uint32_t)distance);
    ae_wr32(r, slot + AE_CAM_YAW, (uint32_t)yaw);
    y_offset = ae_rds16(r, *script);
    camupd_script_move_from_focus(r, slot, focus, yaw, distance, y_offset);
    *script += 2u;
    (void)ae_CameraSlotGetFocusPoint(r, &camera, slot, mode,
                                     slot + AE_CAM_TARGET);
}

static void camupd_script_command_17(AeRam *r, const AeCamupdHooks *h,
                                     uint32_t slot, uint32_t *script)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t focus = AE_CAMUPD_SCRIPT_FOCUS_TEMP;
    int32_t timer = ae_rd16(r, *script);
    int32_t speed;
    uint32_t axis;

    *script += 2u;
    ae_wr8(r, slot + AE_CAM_SCRIPT_STATE, 7);
    camupd_script_set_timer(r, slot, (uint32_t)timer);
    (void)ae_CameraSlotGetFocusPoint(r, &camera, slot,
        ae_rd8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION), focus);
    for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
        uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
        int32_t delta = camupd_s16(ae_rd16(r, focus + component)) -
                        camupd_s16(ae_rd16(r, slot + AE_CAM_EYE + component));
        ae_wr16(r, focus + component, (uint32_t)delta);
    }
    (void)ae_sdk_VectorNormalSS(r, focus, focus);
    camupd_copy8(r, slot + AE_CAM_SCRIPT_FOCUS, focus);
    speed = ae_rds16(r, *script);
    *script += 2u;
    for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
        uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
        int32_t direction = camupd_s16(ae_rd16(r, focus + component));
        ae_wr16(r, slot + AE_CAM_SCRIPT_VELOCITY + component,
                (uint32_t)camupd_script_mul_q12(direction, speed));
    }
}

static void camupd_script_command_18(AeRam *r, uint32_t slot,
                                     uint32_t *script)
{
    uint32_t duration = ae_rd16(r, *script);
    int32_t rate;

    *script += 2u;
    ae_wr8(r, slot + AE_CAM_SCRIPT_STATE, 5);
    camupd_script_set_timer(r, slot, duration);
    rate = camupd_script_degrees_to_angle(ae_rds16(r, *script));
    *script += 2u;
    ae_wr16(r, slot + AE_CAM_SCRIPT_RATE, (uint32_t)rate);
    ae_wr16(r, slot + AE_CAM_SCRIPT_VELOCITY, ae_rd16(r, *script));
    *script += 2u;
    ae_wr16(r, slot + AE_CAM_SCRIPT_VELOCITY + AE_CAMUPD_VEC_Y,
            ae_rd16(r, *script));
    *script += 2u;
}

static void camupd_script_command_19_20_25(AeRam *r, uint32_t slot,
                                           uint32_t opcode, uint32_t *script)
{
    uint32_t timer = ae_rd16(r, *script);
    uint32_t axis;

    *script += 2u;
    ae_wr8(r, slot + AE_CAM_SCRIPT_STATE, opcode == 20u ? 8u : 9u);
    camupd_script_set_timer(r, slot, timer);
    for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
        uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
        ae_wr16(r, slot + AE_CAM_EYE + component, ae_rd16(r, *script));
        *script += 2u;
    }
    for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
        uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
        ae_wr16(r, slot + AE_CAM_SCRIPT_FOCUS + component, ae_rd16(r, *script));
        *script += 2u;
    }
    camupd_copy8(r, slot + AE_CAM_ANCHOR, slot + AE_CAM_EYE);
}

static int32_t camupd_script_direction_yaw(AeRam *r, const AeCamupdHooks *h,
                                           uint32_t slot, uint32_t output)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t vector = AE_CAMUPD_SCRIPT_VECTOR_TEMP2;
    uint32_t mode = ae_rd8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION);

    ae_CameraSlotGetFocusPoint(r, &camera, slot, (int32_t)mode, output);
    ae_wr16(r, vector, ae_rd16(r, AE_G_CAMERA_EYE) - ae_rd16(r, output));
    ae_wr16(r, vector + AE_CAMUPD_VEC_Y, 0);
    ae_wr16(r, vector + AE_CAMUPD_VEC_Z,
            ae_rd16(r, AE_G_CAMERA_EYE + AE_CAMUPD_VEC_Z) -
            ae_rd16(r, output + AE_CAMUPD_VEC_Z));
    (void)ae_sdk_VectorNormalSS(r, vector, vector);
    return ae_Atan2Safe(r, camupd_s16(ae_rd16(r, vector)),
                        camupd_s16(ae_rd16(r, vector + AE_CAMUPD_VEC_Z)));
}

static void camupd_script_command_26(AeRam *r, const AeCamupdHooks *h,
                                     uint32_t slot,
                                     uint32_t *script)
{
    uint32_t direction = AE_CAMUPD_SCRIPT_DIRECTION_TEMP;
    uint32_t mode = ae_rd8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION);
    int32_t distance = ae_rds16(r, *script);
    int32_t yaw;
    int32_t y_offset;

    *script += 2u;
    yaw = camupd_script_direction_yaw(r, h, slot, direction);
    if (mode == 0u) {
        ae_wr32(r, slot + AE_CAM_YAW, (uint32_t)yaw);
        ae_wr32(r, slot + AE_CAM_DISTANCE, (uint32_t)distance);
    }
    y_offset = ae_rds16(r, *script);
    ae_wr16(r, slot + AE_CAM_EYE_X,
        ae_rd16(r, direction) + (uint32_t)camupd_script_mul_q12(
            camupd_s16(ae_rd16(r, AE_CAMUPD_SCRIPT_VECTOR_TEMP2)), distance));
    ae_wr16(r, slot + AE_CAM_EYE_Y,
        ae_rd16(r, direction + AE_CAMUPD_VEC_Y) + (uint32_t)y_offset);
    ae_wr16(r, slot + AE_CAM_EYE_Z,
        ae_rd16(r, direction + AE_CAMUPD_VEC_Z) + (uint32_t)camupd_script_mul_q12(
            camupd_s16(ae_rd16(r, AE_CAMUPD_SCRIPT_VECTOR_TEMP2 + AE_CAMUPD_VEC_Z)),
            distance));
    camupd_copy8(r, slot + AE_CAM_TARGET, direction);
    *script += 2u;
}

/* 8004C318 */
void ae_CameraScriptRun(AeRam *r, const AeCamupdHooks *h, uint32_t slot)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t script = ae_rd32(r, slot + AE_CAM_SCRIPT_POINTER);
    uint32_t opcode;
    uint32_t axis;
    int32_t signed_opcode;
    int finished = 0;

    if (script == 0u || ae_rds16(r, slot + AE_CAM_SCRIPT_TIMER) != 0)
        return;

    while (!finished) {
        opcode = ae_rd16(r, script);
        signed_opcode = camupd_s16(opcode);
        script += 2u;
        if (signed_opcode < 1 || signed_opcode > (int32_t)AE_CAMUPD_SCRIPT_MAX_OPCODE) {
            (void)camupd_call(r, h, h ? h->Printf : NULL, "Printf",
                AE_CAMUPD_SCRIPT_ERROR_FORMAT, (uint32_t)signed_opcode, 0, 0);
            finished = 1;
            continue;
        }

        switch (opcode) {
        case 1: {
            int32_t relative = ae_rds16(r, script);
            script = camupd_s32(script + (uint32_t)relative);
            finished = 1;
            break;
        }
        case 2:
            ae_CameraSlotRemove(r, &camera, ae_rd8(r, slot + AE_CAM_LINKED_SLOT));
            finished = 1;
            break;
        case 3:
        case 4: {
            uint32_t value = ae_rd16(r, script);
            if (opcode == 3u) {
                ae_wr8(r, AE_CAMUPD_G_SCRIPT_EYE_HEIGHT_ACTIVE, 1);
                ae_wr16(r, AE_CAMUPD_G_SCRIPT_EYE_HEIGHT, value);
            } else {
                ae_wr8(r, AE_CAMUPD_G_SCRIPT_DISTANCE_ACTIVE, 1);
                ae_wr16(r, AE_CAMUPD_G_SCRIPT_FOLLOW_DISTANCE, value);
            }
            script += 2u;
            break;
        }
        case 5:
            (void)ae_CameraSlotGetFocusPoint(r, &camera, slot,
                ae_rd8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION), AE_SCRATCH_BASE);
            break;
        case 6:
        case 7:
        case 8: {
            uint32_t timer = ae_rd16(r, script);
            ae_wr8(r, slot + AE_CAM_SCRIPT_STATE, opcode == 6u ? 2u : 3u);
            camupd_script_set_timer(r, slot, timer);
            script += 2u;
            finished = 1;
            break;
        }
        case 9: {
            uint32_t timer = ae_rd16(r, script);
            ae_wr8(r, slot + AE_CAM_SCRIPT_STATE, 1);
            camupd_script_set_timer(r, slot, timer);
            script += 2u;
            finished = 1;
            break;
        }
        case 10:
        case 11:
        case 12:
            ae_wr8(r, slot + AE_CAMUPD_G_SCRIPT_DIRECTION,
                   opcode == 10u ? 0u : (opcode == 11u ? 1u : 2u));
            ae_wr16(r, slot + AE_CAM_FOCUS_DISTANCE, 0);
            ae_wr16(r, slot + AE_CAM_FOCUS_HEIGHT, 0);
            break;
        case 13:
            ae_wr16(r, slot + AE_CAM_FOCUS_DISTANCE, ae_rd16(r, script));
            ae_wr16(r, slot + AE_CAM_FOCUS_HEIGHT, ae_rd16(r, script + 2u));
            script += 4u;
            break;
        case 14:
        case 15:
            ae_wr8(r, slot + AE_CAMUPD_G_SCRIPT_FOCUS_MODE,
                   opcode == 14u ? 0u : 1u);
            break;
        case 16:
            camupd_script_command_15(r, h, slot, &script);
            break;
        case 17:
            camupd_script_command_16(r, h, slot, &script);
            break;
        case 18:
            camupd_script_command_17(r, h, slot, &script);
            finished = 1;
            break;
        case 19:
            camupd_script_command_18(r, slot, &script);
            finished = 1;
            break;
        case 20:
        case 25:
            camupd_script_command_19_20_25(r, slot, opcode, &script);
            finished = 1;
            break;
        case 21:
            for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
                uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
                ae_wr16(r, AE_SCRATCH_BASE + component,
                        ae_rd16(r, AE_SCRATCH_BASE + component) + ae_rd16(r, script));
                script += 2u;
            }
            break;
        case 22: {
            int32_t trigger = ae_rds16(r, script);
            script += 2u;
            ae_wr32(r, slot + AE_CAM_SCRIPT_POINTER, script);
            ae_EventCamTrigger(r, h, (uint32_t)trigger);
            script = ae_rd32(r, slot + AE_CAM_SCRIPT_POINTER);
            break;
        }
        case 23: {
            uint32_t timer = ae_rd16(r, script);
            script += 2u;
            camupd_script_set_timer(r, slot, timer);
            ae_wr8(r, slot + AE_CAM_SCRIPT_ACTIVE, ae_rd8(r, script));
            script += 2u;
            finished = 1;
            break;
        }
        case 24:
            for (axis = 0; axis < AE_CAMUPD_VEC_COMPONENT_COUNT; ++axis) {
                uint32_t component = axis * AE_CAMUPD_VEC_COMPONENT_SIZE;
                ae_wr16(r, AE_G_CAMERA_EYE + component, ae_rd16(r, script));
                script += 2u;
            }
            ae_CameraSlotSnapshotEye(r, &camera);
            break;
        case 26:
            camupd_script_command_26(r, h, slot, &script);
            break;
        default:
            ae_ram_fail(r, "CameraScriptRun opcode dispatch is incomplete");
            finished = 1;
            break;
        }
    }
    ae_wr32(r, slot + AE_CAM_SCRIPT_POINTER, script);
}

/* 80047BD0 */
void ae_CameraUpdate(AeRam *r, const AeCamupdHooks *h)
{
    AeCameraHooks camera = camupd_camera_math_hooks(h);
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t slots = ae_rd32(r, AE_G_CAMERA_SLOTS);
    uint32_t slot;
    uint32_t index;

    ae_CameraSnapshotPlayer(r);
    if (ae_rd8(r, state + AE_CAM_G_UNK_3E) == 0u)
        ae_CameraRegionUpdate(r, h);
    ae_CameraSelectMode(r, h);

    for (index = 0; index < AE_CAMERA_SLOT_COUNT; ++index) {
        slot = slots + index * AE_CAMERA_SLOT_SIZE;
        ae_wr8(r, state + AE_CAM_G_FOLLOW_GUARD, 0);
        if (ae_rd8(r, slot + AE_CAM_KIND) == 4u ||
            ae_rd8(r, slot + AE_CAM_KIND) == 5u) {
            ae_CameraScriptRun(r, h, slot);
            ae_CameraSlotUpdate(r, &camera, slot);
        }
    }

    for (index = 0; index < AE_CAMERA_SLOT_COUNT; ++index) {
        uint32_t kind;

        slot = slots + index * AE_CAMERA_SLOT_SIZE;
        kind = ae_rd8(r, slot + AE_CAM_KIND);
        if (kind == 0u)
            continue;

        switch (kind) {
        case 1:
            ae_CameraFollowDispatch(r, &camera, slot);
            break;
        case 2:
            ae_CameraKindAimUpdate(r, &camera, slot);
            break;
        case 3:
            ae_CameraKindFixedUpdate(r, &camera, slot);
            break;
        case 4:
        case 5:
            break;
        default:
            ae_CameraFollowDispatch(r, &camera, slot);
            break;
        }
    }

    ae_CameraApplySlot(r);
    ae_CameraShakeApply(r);
    ae_EventCamUpdate(r, h);
    ae_StageExitSequenceUpdate(r, h);
    ae_CameraTransitionFrameDraw(r, h);

    {
        uint32_t blend_x = ae_rd16(r, AE_CAMUPD_G_EYE_BLEND_X);
        uint32_t blend_y = ae_rd16(r, AE_CAMUPD_G_EYE_BLEND_Y);
        uint32_t blend_z = ae_rd16(r, AE_CAMUPD_G_EYE_BLEND_Z);
        uint32_t roll_slot = ae_rd32(r, state + AE_CAM_G_ACTIVE_SLOT);
        int32_t roll = camupd_s16(ae_rd16(r, roll_slot + AE_CAM_UNK_2C));

        ae_wr16(r, AE_CAMUPD_G_EYE_BLEND_X, 0);
        ae_wr16(r, AE_CAMUPD_G_EYE_BLEND_Y, 0);
        ae_wr16(r, AE_CAMUPD_G_EYE_BLEND_Z, 0);
        ae_wr16(r, AE_G_CAMERA_EYE,
                ae_rd16(r, AE_G_CAMERA_EYE) + blend_x);
        ae_wr16(r, AE_G_CAMERA_EYE + AE_CAMUPD_VEC_Y,
                ae_rd16(r, AE_G_CAMERA_EYE + AE_CAMUPD_VEC_Y) + blend_y);
        ae_wr16(r, AE_G_CAMERA_EYE + AE_CAMUPD_VEC_Z,
                ae_rd16(r, AE_G_CAMERA_EYE + AE_CAMUPD_VEC_Z) + blend_z);
        ae_wr16(r, AE_G_CAMERA_TARGET,
                ae_rd16(r, AE_G_CAMERA_TARGET) + blend_x);
        ae_wr16(r, AE_G_CAMERA_TARGET + AE_CAMUPD_VEC_Y,
                ae_rd16(r, AE_G_CAMERA_TARGET + AE_CAMUPD_VEC_Y) + blend_y);
        ae_wr16(r, AE_G_CAMERA_TARGET + AE_CAMUPD_VEC_Z,
                ae_rd16(r, AE_G_CAMERA_TARGET + AE_CAMUPD_VEC_Z) + blend_z);

        if (h == NULL || h->CameraLookAt == NULL) {
            ae_ram_fail(r, "CameraUpdate hook CameraLookAt was not provided");
            return;
        }
        (void)h->CameraLookAt(h->user, r, AE_G_CAMERA_EYE, AE_G_CAMERA_TARGET,
            AE_G_CAMERA_MATRIX, AE_CAMUPD_G_CAMERA_ROTATION, (uint32_t)roll);
    }
    ae_ScreenFadeUpdate(r, h);
}

/* 80046990: the PS1 ordering-table packet is owned by the native renderer. */
void ae_ScreenFlashDraw(AeRam *r, const AeCamupdHooks *h, uint32_t intensity)
{
    (void)camupd_call(r, h, h ? h->RenderScreenFlash : NULL,
                      "RenderScreenFlash", intensity, 0, 0, 0);
}

/* 80046A78 */
void ae_ScreenFadeUpdate(AeRam *r, const AeCamupdHooks *h)
{
    uint32_t counter = ae_rd8(r, AE_CAMUPD_G_FADE_COUNTER);
    uint32_t value;

    if (counter == 0u)
        return;

    ae_wr8(r, AE_CAMUPD_G_FADE_COUNTER, counter - 1u);
    value = ae_rd8(r, AE_CAMUPD_G_FADE_VALUE);
    ae_ScreenFlashDraw(r, h, value);
    ae_wr8(r, AE_CAMUPD_G_FADE_VALUE,
           value + ae_rd8(r, AE_CAMUPD_G_FADE_STEP));
}

/* 8005DD18 */
void ae_CutsceneModeOn(AeRam *r)
{
    ae_wr16(r, AE_CAMUPD_G_CUTSCENE_MODE, 1);
}

/* 8005DD28 */
void ae_CutsceneModeOff(AeRam *r)
{
    ae_wr16(r, AE_CAMUPD_G_CUTSCENE_MODE, 0);
}
