#include "../ui/ae_missb.h"

#include "../core/ae_sdk.h"
#include "../player/ae_player_act.h"
#include "../player/native_player.h"
#include "ae_gimmick_a.h"
#include "ae_solid.h"
#include "ae_zones.h"
#include "ae_world.h"
#include "../player/ae_player_spawn.h"

#define MBC_STAGE_STATE          0x800F4350u
#define MBC_STAGE_SKIP_SURFACE   0x800F45E8u
#define MBC_AREA_PROPS           0x800B4D64u
#define MBC_AREA_PROP_STRIDE     0x20u
#define MBC_COLLIDER_POSITION_X  0x00u
#define MBC_COLLIDER_POSITION_Y  0x02u
#define MBC_COLLIDER_POSITION_Z  0x04u
#define MBC_COLLIDER_PROP_INDEX  0x0Eu
#define MBC_COLLIDER_PROGRESS    0x0Cu
#define MBC_COLLIDER_KIND        0x12u
#define MBC_COLLIDER_ACTION      0x10u
#define MBC_COLLIDER_LINK_INDEX  0x22u
#define MBC_COLLIDER_FLAGS       0x18u
#define MBC_COLLIDER_OFFSET      0x1Cu
#define MBC_COLLIDER_QUADRANT_MASK 0x20u
#define MBC_COLLIDER_RADIUS      0x1Au
#define MBC_COLLIDER_DELTA_X     0x34u
#define MBC_COLLIDER_DELTA_Z     0x36u
#define MBC_COLLIDER_SPEED       0x32u
#define MBC_COLLIDER_PHASE_X     0x34u
#define MBC_COLLIDER_PHASE_Z     0x36u
#define MBC_COLLIDER_EXTENT_X    0x32u
#define MBC_COLLIDER_EXTENT_Y    0x34u
#define MBC_COLLIDER_EXTENT_Z    0x36u
#define MBC_COLLIDER_ORIGIN_X    0x24u
#define MBC_COLLIDER_ORIGIN_Y    0x26u
#define MBC_COLLIDER_ORIGIN_Z    0x28u
#define MBC_COLLIDER_HALF_X      0x06u
#define MBC_COLLIDER_HALF_Y      0x08u
#define MBC_COLLIDER_HALF_Z      0x0Au
#define MBC_COLLIDER_MODE        0x14u
#define MBC_PLAYER_POSITION_X    0x24u
#define MBC_PLAYER_POSITION_Y    0x28u
#define MBC_PLAYER_POSITION_Z    0x2Cu
#define MBC_PLAYER_VELOCITY_X    0x30u
#define MBC_PLAYER_VELOCITY_Z    0x38u
#define MBC_PLAYER_AREA          0x58u
#define MBC_PLAYER_FACE_X        0x60u
#define MBC_PLAYER_FACE_Z        0x64u
#define MBC_PLAYER_VERTICAL_DELTA 0x34u
#define MBC_PLAYER_BODY_RADIUS   0x44u
#define MBC_GIMMICK_ORIENTATION  0x08u
#define MBC_GIMMICK_PARAM_Y      0x10u
#define MBC_GIMMICK_ANGLE        0x16u
#define MBC_PROP_POSITION_X      0x10u
#define MBC_PROP_POSITION_Y      0x12u
#define MBC_PROP_POSITION_Z      0x14u
#define MBC_PROP_ANGLE_Y         0x16u
#define MBC_PROP_ROTATION_MATRIX 0x1Cu
#define MBC_STAGE_AREA           0x126u
#define MBC_STAGE_COLLIDER_LIST  0x20Cu
#define MBC_STAGE_RECORD_LIST    0x214u
#define MBC_STAGE_PLAYER_MODE    0x150u
#define MBC_RCCAR                0x800F5A20u
#define MBC_RCCAR_AREA           0x58u
#define MBC_SCRIPT_HALF_X        0x06u
#define MBC_SCRIPT_HALF_Y        0x08u
#define MBC_SCRIPT_HALF_Z        0x0Au
#define MBC_SCRIPT_SCALE_X       0x06u
#define MBC_SCRIPT_SCALE_Y       0x08u
#define MBC_SCRIPT_SCALE_Z       0x0Au
#define MBC_SCRIPT_TARGET_X      0x00u
#define MBC_SCRIPT_POSITION_Y   0x02u
#define MBC_SCRIPT_POSITION_Z   0x04u
#define MBC_SCRIPT_DIRECTION     0x0Eu
#define MBC_SCRIPT_DURATION      0x0Cu
#define MBC_COLLIDER_EXTENT_BASE 0x2Au
#define MBC_COLLIDER_AXIS_STRIDE 2u
#define MBC_VECTOR_Y             0x02u
#define MBC_VECTOR_Z             0x04u
#define MBC_PLAYER               0x800EC1E0u
#define MBC_PLAYER_STATE         0x70u
#define MBC_COLLIDER_SCRIPT_TIMER 0x800B3388u
#define MBC_GROUND_QUERY_SOLID_KIND 0x1F8003F0u
#define MBC_GROUND_QUERY_SURFACE_FLAGS 0x1F8003F8u
#define MBC_DEFAULT_STACK        0x801FFF00u
#define MBC_COLLIDER_WORK_BYTES  56u

static int32_t mbc_s16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) != 0u ? (int32_t)low - 0x10000 : (int32_t)low;
}

static int32_t mbc_s32(uint32_t value)
{
    return (value & 0x80000000u) != 0u
        ? (int32_t)((int64_t)value - 0x100000000ll) : (int32_t)value;
}

static int32_t mbc_sra(int32_t value, uint32_t shift)
{
    if (shift == 0u) return value;
    uint32_t bits = (uint32_t)value;
    uint32_t shifted = bits >> shift;
    if ((bits & 0x80000000u) != 0u) shifted |= 0xFFFFFFFFu << (32u - shift);
    return (int32_t)shifted;
}

static int32_t mbc_fixed_product(int32_t left, int32_t right, uint32_t shift)
{
    int32_t product = mbc_s32((uint32_t)left * (uint32_t)right);
    if (product < 0) product = (int32_t)((uint32_t)product + ((1u << shift) - 1u));
    return mbc_sra(product, shift);
}

static int32_t mbc_wrap_add16(int32_t left, int32_t right)
{
    return mbc_s16((uint32_t)left + (uint32_t)right);
}

static uint32_t mbc_frame_stack(const AeMissbHooks *hooks)
{
    return hooks != NULL && hooks->stack_pointer != 0u
        ? hooks->stack_pointer : MBC_DEFAULT_STACK;
}

static void mbc_add_prop_delta(AeRam *r, uint32_t collider,
                               int32_t dx, int32_t dy, int32_t dz)
{
    int32_t index = ae_rds16(r, collider + MBC_COLLIDER_PROP_INDEX);
    if (index < 0) return;
    uint32_t prop = ae_rd32(r, MBC_AREA_PROPS) +
                    (uint32_t)index * MBC_AREA_PROP_STRIDE;
    ae_wr16(r, prop + MBC_PROP_POSITION_X,
            (uint32_t)mbc_wrap_add16((int32_t)ae_rd16(r, prop + MBC_PROP_POSITION_X), dx));
    ae_wr16(r, prop + MBC_PROP_POSITION_Y,
            (uint32_t)mbc_wrap_add16((int32_t)ae_rd16(r, prop + MBC_PROP_POSITION_Y), dy));
    ae_wr16(r, prop + MBC_PROP_POSITION_Z,
            (uint32_t)mbc_wrap_add16((int32_t)ae_rd16(r, prop + MBC_PROP_POSITION_Z), dz));
}

static const AeGimmickAHooks *mbc_gimmick_hooks(AeRam *r,
                                               const AeMissbHooks *h)
{
    if (h == NULL || h->gimmick_a == NULL) {
        ae_ram_fail(r, "AeMissbHooks.gimmick_a not provided");
        return NULL;
    }
    return h->gimmick_a;
}

/* 80088D78: move a rising collider toward the ground-height sample returned
 * by the original collision probe. */
int32_t ae_ColliderScriptRiseToGround(AeRam *r, const AeMissbHooks *h,
                                      uint32_t collider, int32_t unused)
{
    (void)unused;
    if (ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y) > 3072) {
        int32_t prop_index = ae_rds16(r, collider + MBC_COLLIDER_PROP_INDEX);
        if (prop_index >= 0) {
            uint32_t prop = ae_rd32(r, MBC_AREA_PROPS) +
                            (uint32_t)prop_index * MBC_AREA_PROP_STRIDE;
            ae_wr16(r, prop + MBC_PROP_ANGLE_Y, 0u);
        }
        return 0;
    }

    uint32_t vector = mbc_frame_stack(h) - 40u + 16u;
    ae_wr16(r, vector, 0u);
    ae_wr16(r, vector + MBC_VECTOR_Y, 0u);
    ae_wr16(r, vector + MBC_VECTOR_Z, 0u);
    (void)ae_missb_call(r, h, "ColliderGroundProbe", 0x8008BE4Cu,
                        2u, collider, vector, 0u, 0u);

    ae_wr16(r, MBC_STAGE_SKIP_SURFACE, 1u);
    if (h == NULL || h->world == NULL) {
        ae_ram_fail(r, "AeMissbHooks.world not provided for GetGroundHeight");
        return 0;
    }
    int32_t ground_y = ae_GetGroundHeight(
        r, h->world, ae_rds16(r, vector), ae_rds16(r, vector + MBC_VECTOR_Y),
        ae_rds16(r, vector + MBC_VECTOR_Z));
    if (ae_rds16(r, MBC_GROUND_QUERY_SURFACE_FLAGS) == 1 &&
        ae_rds16(r, MBC_GROUND_QUERY_SOLID_KIND) < 0) {
        ground_y = 32767;
    }

    int32_t delta_y = 0;
    int32_t speed = ae_rds16(r, collider + MBC_COLLIDER_SPEED);
    int32_t y = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y);
    if (speed > 0) {
        if (y >= ground_y) {
            ae_wr16(r, collider + MBC_COLLIDER_OFFSET, 0u);
            ae_wr16(r, collider + MBC_COLLIDER_PHASE_X, 0u);
        } else {
            speed = mbc_wrap_add16(speed, -1);
            ae_wr16(r, collider + MBC_COLLIDER_SPEED, (uint32_t)speed);
            int32_t rounded_half = mbc_sra(speed + (speed < 0 ? 1 : 0), 1u);
            int32_t parity = speed - rounded_half * 2;
            delta_y = (1 - parity * 2) * 32;
        }
    } else {
        int32_t offset = mbc_wrap_add16(
            ae_rds16(r, collider + MBC_COLLIDER_OFFSET), 8);
        if (y + offset > ground_y) {
            delta_y = ground_y - y;
            ae_wr16(r, collider + MBC_COLLIDER_OFFSET, 0u);
            ae_wr16(r, collider + MBC_COLLIDER_PHASE_X, 0u);
        } else {
            ae_wr16(r, collider + MBC_COLLIDER_OFFSET, (uint32_t)offset);
        }
    }

    int32_t prop_index = ae_rds16(r, collider + MBC_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = ae_rd32(r, MBC_AREA_PROPS) +
                        (uint32_t)prop_index * MBC_AREA_PROP_STRIDE;
            ae_wr16(r, prop + MBC_PROP_POSITION_Y,
                    (uint32_t)mbc_wrap_add16(ae_rds16(r, prop + MBC_PROP_POSITION_Y), delta_y));
    }
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
            (uint32_t)mbc_wrap_add16(y, delta_y));
    return 0;
}

/* 80088F2C: probe and apply one lift step, shortening player climb movement
 * until its wall probe accepts the horizontal delta. */
int32_t ae_ColliderScriptLiftUpdate(AeRam *r, const AeMissbHooks *h,
                                    uint32_t collider, int32_t index)
{
    uint32_t frame = mbc_frame_stack(h) - 48u;
    uint32_t delta = frame + 16u;
    uint32_t probe = frame + 24u;
    ae_wr16(r, delta, 0u);
    ae_wr16(r, delta + MBC_VECTOR_Y, 0u);
    ae_wr16(r, delta + MBC_VECTOR_Z, 0u);

    if (ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y) >= 3073) {
        int32_t prop_index = ae_rds16(r, collider + MBC_COLLIDER_PROP_INDEX);
        if (prop_index >= 0) {
            uint32_t prop = ae_rd32(r, MBC_AREA_PROPS) +
                            (uint32_t)prop_index * MBC_AREA_PROP_STRIDE;
            ae_wr16(r, prop + MBC_PROP_ANGLE_Y, 0u);
        }
        ae_wr16(r, collider + MBC_COLLIDER_FLAGS,
                (ae_rd16(r, collider + MBC_COLLIDER_FLAGS) & 0x8000u) | 1u);
        return 0;
    }

    uint32_t timer = ae_rd32(r, MBC_COLLIDER_SCRIPT_TIMER);
    if ((int32_t)timer > 0) ae_wr32(r, MBC_COLLIDER_SCRIPT_TIMER, timer - 1u);
    const AeGimmickAHooks *gimmick_hooks = mbc_gimmick_hooks(r, h);
    if (gimmick_hooks == NULL) return 0;

    int32_t movement_state = ae_rds16(r, collider + MBC_COLLIDER_SPEED);
    uint32_t player_state = ae_rd32(r, MBC_PLAYER + MBC_PLAYER_STATE);
    int32_t player_climbing = (player_state & 0xF0u) == 0x70u;
    int32_t probe_min_y;

    if (movement_state >= 0 && player_climbing) {
        int32_t dx = ae_rds16(r, MBC_PLAYER + MBC_PLAYER_VELOCITY_X);
        int32_t dz = ae_rds16(r, MBC_PLAYER + MBC_PLAYER_VELOCITY_Z);
        ae_wr16(r, delta, (uint32_t)dx);
        ae_wr16(r, delta + MBC_VECTOR_Z, (uint32_t)dz);

        while (ae_GimmickSwingProbeWall(r, gimmick_hooks, collider,
                                        dx, dz, probe) != 0) {
            int32_t half_dx = dx;
            int32_t half_dz = dz;
            if (half_dx < 0) ++half_dx;
            if (half_dz < 0) ++half_dz;
            half_dx = mbc_sra(half_dx, 1u);
            half_dz = mbc_sra(half_dz, 1u);
            if (half_dx == 0 && half_dz == 0) {
                dx = 0;
                dz = 0;
                break;
            }
            dx = mbc_s16((uint32_t)dx - (uint32_t)half_dx);
            dz = mbc_s16((uint32_t)dz - (uint32_t)half_dz);
            ae_wr16(r, delta, (uint32_t)dx);
            ae_wr16(r, delta + MBC_VECTOR_Z, (uint32_t)dz);
        }
        ae_wr16(r, delta, (uint32_t)dx);
        ae_wr16(r, delta + MBC_VECTOR_Z, (uint32_t)dz);
        ae_wr16(r, delta + MBC_VECTOR_Y, 32u);
        ae_wr16(r, collider + MBC_COLLIDER_OFFSET, 32u);
        probe_min_y = ae_rds16(r, probe);
        int32_t current_y = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y);
        if (current_y + 32 > probe_min_y) {
            int32_t dy = mbc_wrap_add16(probe_min_y, -current_y);
            ae_wr16(r, delta + MBC_VECTOR_Y, (uint32_t)dy);
            ae_wr16(r, collider + MBC_COLLIDER_OFFSET, 0u);
        }

        if (ae_rd32(r, MBC_COLLIDER_SCRIPT_TIMER) == 0u) {
            ae_wr32(r, MBC_COLLIDER_SCRIPT_TIMER, 24u);
            int32_t area_kind = ae_rds16(r, MBC_STAGE_STATE + MBC_STAGE_AREA);
            if (area_kind == 11 || area_kind == 15 || area_kind == 101) {
                (void)ae_PlaySfx(r, h != NULL ? h->sound_hw : NULL,
                                 area_kind == 11 ? 174 : 177);
            }
        }
    } else {
        (void)ae_GimmickSwingProbeWall(r, gimmick_hooks, collider, 0, 0, probe);
        int32_t phase_x = ae_rds16(r, collider + MBC_COLLIDER_PHASE_X);
        int32_t phase_z = ae_rds16(r, collider + MBC_COLLIDER_PHASE_Z);
        if (phase_x != 0 || phase_z != 0) {
            (void)ae_GimmickSwingProbeWall(r, gimmick_hooks, collider,
                                            phase_x, phase_z, probe + MBC_VECTOR_Y);
            if (ae_rds16(r, probe) < ae_rds16(r, probe + MBC_VECTOR_Y)) {
                ae_wr16(r, delta, (uint32_t)phase_x);
                ae_wr16(r, delta + MBC_VECTOR_Z, (uint32_t)phase_z);
                ae_wr16(r, probe, ae_rd16(r, probe + MBC_VECTOR_Y));
            } else {
                ae_wr16(r, delta, 0u);
                ae_wr16(r, delta + MBC_VECTOR_Z, 0u);
            }
        } else {
            ae_wr16(r, delta, 0u);
            ae_wr16(r, delta + MBC_VECTOR_Z, 0u);
        }

        int32_t offset = mbc_wrap_add16(
            ae_rds16(r, collider + MBC_COLLIDER_OFFSET), 16);
        ae_wr16(r, collider + MBC_COLLIDER_OFFSET, (uint32_t)offset);
      ae_wr16(r, delta + MBC_VECTOR_Y, (uint32_t)offset);
        int32_t current_y = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y);
        probe_min_y = ae_rds16(r, probe);
        if (current_y + offset > probe_min_y) {
            ae_wr16(r, delta + MBC_VECTOR_Y,
                    (uint32_t)mbc_wrap_add16(probe_min_y, -current_y));
            ae_wr16(r, collider + MBC_COLLIDER_OFFSET, 0u);
        }
    }

    int32_t dx = ae_rds16(r, delta);
    int32_t dy = ae_rds16(r, delta + MBC_VECTOR_Y);
    int32_t dz = ae_rds16(r, delta + MBC_VECTOR_Z);
    mbc_add_prop_delta(r, collider, dx, dy, dz);
    (void)ae_GimmickCarryBodiesByDelta(r, gimmick_hooks, collider, delta,
                                       mbc_s16((uint32_t)index));
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_X,
            (uint32_t)mbc_wrap_add16(ae_rds16(r, collider + MBC_COLLIDER_POSITION_X), dx));
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
            (uint32_t)mbc_wrap_add16(ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y), dy));
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_Z,
            (uint32_t)mbc_wrap_add16(ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z), dz));
    ae_wr16(r, collider + MBC_COLLIDER_PHASE_X, (uint32_t)dx);
    ae_wr16(r, collider + MBC_COLLIDER_PHASE_Z, (uint32_t)dz);
    ae_wr16(r, collider + MBC_COLLIDER_QUADRANT_MASK,
            (uint32_t)ae_ColliderComputeQuadrantMask(
                r, h != NULL ? h->zones : NULL, collider));
    return 0;
}

/* 80089B10: advance the orbit phase and apply the change in the player's
 * radial X/Z offset from the current angle to the script's next angle. */
int32_t ae_ColliderScriptOrbitPlayer(AeRam *r, const AeMissbHooks *h,
                                     uint32_t collider, int32_t index)
{
    (void)h;
    uint32_t player = MBC_PLAYER;
    int32_t player_x = ae_rds32(r, player + MBC_PLAYER_POSITION_X);
    int32_t player_z = ae_rds32(r, player + MBC_PLAYER_POSITION_Z);
    int32_t collider_x = ae_rds16(r, collider + MBC_COLLIDER_POSITION_X);
    int32_t collider_z = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z);
    int32_t angle = ae_sdk_ratan2(r, player_z - collider_z,
                                  player_x - collider_x);
    if (angle < 0) angle += 4096;

    uint16_t geometry = (uint16_t)ae_rd16(r, collider + MBC_COLLIDER_KIND);
    uint16_t movement_mode = geometry & 0xF000u;
    int32_t speed = ae_rds16(r, collider + MBC_COLLIDER_OFFSET);
    int32_t angular_delta = mbc_fixed_product(angle, speed, 12u);
    int32_t progress = ae_rds16(r, collider + MBC_COLLIDER_PROGRESS);
    int32_t travel = movement_mode == 0u
        ? mbc_s32((uint32_t)angular_delta + (uint32_t)progress)
        : mbc_s32((uint32_t)angular_delta - (uint32_t)progress);
    int32_t travel_numerator = mbc_s32((uint32_t)travel << 12);
    if (speed == 0) {
        ae_ram_fail(r, "ColliderScriptOrbitPlayer: zero angular speed");
        return 0;
    }
    int32_t radius_phase = travel_numerator / speed;
    int32_t horizontal_distance = ae_player_vec_length3(
        mbc_s32((uint32_t)player_x - (uint32_t)collider_x), 0,
        mbc_s32((uint32_t)player_z - (uint32_t)collider_z));
    int32_t bearing_cosine = ae_sdk_rcos(r, angle);
    int32_t phase_cosine = ae_sdk_rcos(r, radius_phase);
    int32_t cosine_at_bearing = mbc_fixed_product(horizontal_distance,
                                                   bearing_cosine, 12u);
    int32_t cosine_at_phase = mbc_fixed_product(horizontal_distance,
                                                 phase_cosine, 12u);
    int32_t cosine_delta = mbc_s32((uint32_t)cosine_at_bearing -
                                   (uint32_t)cosine_at_phase);
    int32_t bearing_sine = ae_sdk_rsin(r, angle);
    int32_t phase_sine = ae_sdk_rsin(r, radius_phase);
    int32_t sine_at_bearing = mbc_fixed_product(horizontal_distance,
                                                 bearing_sine, 12u);
    int32_t sine_at_phase = mbc_fixed_product(horizontal_distance,
                                               phase_sine, 12u);
    int32_t sine_delta = mbc_s32((uint32_t)sine_at_bearing -
                                 (uint32_t)sine_at_phase);

    uint16_t next_progress;
    if ((uint16_t)cosine_delta == 0u && (uint16_t)sine_delta == 0u) {
        next_progress = (uint16_t)(ae_rd16(r, collider + MBC_COLLIDER_PROGRESS) + 1u);
    } else {
        next_progress = 1u;
    }
    ae_wr16(r, collider + MBC_COLLIDER_PROGRESS, next_progress);

    if (ae_rds16(r, player + MBC_PLAYER_AREA) == mbc_s16((uint32_t)index)) {
        int32_t delta_x = mbc_s16((uint32_t)cosine_delta);
        int32_t delta_z = mbc_s16((uint32_t)sine_delta);
        ae_wr32(r, player + MBC_PLAYER_POSITION_X,
                ae_rd32(r, player + MBC_PLAYER_POSITION_X) + (uint32_t)delta_x);
        ae_wr32(r, player + MBC_PLAYER_POSITION_Y,
                ae_rd32(r, player + MBC_PLAYER_POSITION_Y));
        ae_wr32(r, player + MBC_PLAYER_POSITION_Z,
                ae_rd32(r, player + MBC_PLAYER_POSITION_Z) + (uint32_t)delta_z);
    }
    return 0;
}

/* 800895F0: interpolate a collider around its script origin, apply the
 * selected X/Y/Z rotation mode, then carry bodies and synchronize its prop. */
int32_t ae_ColliderScriptRotateUpdate(AeRam *r, const AeMissbHooks *h,
                                      uint32_t collider, int32_t index)
{
    uint16_t geometry = (uint16_t)ae_rd16(r, collider + MBC_COLLIDER_KIND);
    uint32_t mode = geometry & 0x0F00u;
    uint32_t movement_mode = geometry & 0xF000u;
    uint32_t kind = geometry & 0x00FFu;
    int32_t linked_index = ae_rds16(r, collider + MBC_COLLIDER_LINK_INDEX);
    int32_t start_x, start_y, start_z;
    if (linked_index < 0) {
        start_x = ae_rds16(r, collider + MBC_COLLIDER_POSITION_X);
        start_y = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y);
        start_z = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z);
    } else {
        uint32_t colliders = ae_rd32(r, MBC_STAGE_STATE + MBC_STAGE_COLLIDER_LIST);
        uint32_t linked = colliders + (uint32_t)linked_index * MBC_COLLIDER_WORK_BYTES;
        start_x = ae_rds16(r, linked + MBC_COLLIDER_POSITION_X);
        start_y = ae_rds16(r, linked + MBC_COLLIDER_POSITION_Y);
        start_z = ae_rds16(r, linked + MBC_COLLIDER_POSITION_Z);
    }

    int32_t duration = ae_rds16(r, collider + MBC_COLLIDER_OFFSET);
    if (duration == 0) {
        ae_ram_fail(r, "ColliderScriptRotateUpdate: zero interpolation duration");
        return 0;
    }
    int32_t elapsed = ae_rds16(r, collider + MBC_COLLIDER_PROGRESS);
    int32_t phase = mbc_s32((uint32_t)elapsed << 12) / duration;
    int32_t secondary_phase = 0;
    if (kind == 5u) {
        int32_t secondary_range = ae_rds16(r, collider + MBC_COLLIDER_PHASE_X);
        if (secondary_range == 0) {
            ae_ram_fail(r, "ColliderScriptRotateUpdate: zero secondary range");
            return 0;
        }
        secondary_phase = mbc_s32((uint32_t)ae_rds16(
            r, collider + MBC_COLLIDER_SPEED) << 12) / secondary_range;
    }

    int32_t radius = ae_rds16(r, collider + MBC_COLLIDER_RADIUS);
    int32_t sine = ae_sdk_rsin(r, phase);
    int32_t cosine = ae_sdk_rcos(r, phase);
    int32_t next_x = start_x;
    int32_t next_y = start_y;
    int32_t next_z = start_z;
    if (mode == 0x0100u) {
        next_x = mbc_wrap_add16(start_x, mbc_fixed_product(radius, sine, 12u));
        next_z = mbc_wrap_add16(start_z, mbc_fixed_product(radius, cosine, 12u));
        if (kind == 5u) {
            int32_t secondary_sine = ae_sdk_rsin(r, secondary_phase);
            if (secondary_sine < 0) secondary_sine += 31;
            next_y = mbc_wrap_add16(start_y,
                                    mbc_sra(secondary_sine, 5u) - 128);
        }
    } else if (mode == 0x0200u) {
        next_y = mbc_wrap_add16(start_y, mbc_fixed_product(radius, cosine, 12u));
        next_z = mbc_wrap_add16(start_z, mbc_fixed_product(radius, sine, 12u));
    } else {
        next_x = mbc_wrap_add16(start_x, mbc_fixed_product(radius, cosine, 12u));
        next_y = mbc_wrap_add16(start_y, mbc_fixed_product(radius, sine, 12u));
    }

    uint32_t stack = mbc_frame_stack(h);
    uint32_t delta = stack - 96u + 24u;
    ae_wr16(r, delta, (uint32_t)(next_x - ae_rds16(r, collider)));
    ae_wr16(r, delta + MBC_VECTOR_Y,
            (uint32_t)(next_y - ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y)));
    ae_wr16(r, delta + MBC_VECTOR_Z,
            (uint32_t)(next_z - ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z)));

    const AeGimmickAHooks *gimmick_hooks = mbc_gimmick_hooks(r, h);
    if (gimmick_hooks == NULL) return 0;
    (void)ae_GimmickCarryBodiesByDelta(r, gimmick_hooks, collider, delta,
                                       mbc_s16((uint32_t)index));
    ae_wr16(r, delta, (uint32_t)next_x);
    ae_wr16(r, delta + MBC_VECTOR_Y, (uint32_t)next_y);
    ae_wr16(r, delta + MBC_VECTOR_Z, (uint32_t)next_z);
    (void)ae_GimmickCarryBodiesSwing(r, gimmick_hooks, collider, delta,
                                     mbc_s16((uint32_t)index));

    int32_t prop_index = ae_rds16(r, collider + MBC_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = ae_rd32(r, MBC_AREA_PROPS) +
                        (uint32_t)prop_index * MBC_AREA_PROP_STRIDE;
        if (mode == 0x0100u) {
            ae_wr16(r, prop + MBC_PROP_POSITION_X, (uint32_t)next_x);
            ae_wr16(r, prop + MBC_PROP_POSITION_Z, (uint32_t)next_z);
        } else if (mode == 0x0200u) {
            ae_wr16(r, prop + MBC_PROP_POSITION_Y, (uint32_t)next_y);
            ae_wr16(r, prop + MBC_PROP_POSITION_Z, (uint32_t)next_z);
        } else {
            ae_wr16(r, prop + MBC_PROP_POSITION_X, (uint32_t)next_x);
            ae_wr16(r, prop + MBC_PROP_POSITION_Y, (uint32_t)next_y);
        }
        if (kind == 4u) {
            ae_wr16(r, prop + MBC_PROP_POSITION_Y,
                    ae_rd16(r, prop + MBC_PROP_POSITION_Y) -
                    ae_rd16(r, collider + MBC_COLLIDER_HALF_Y));
        }
        int32_t first_step = ae_GimmickSwingFirstStep(r, collider);
        if (first_step == 0 && mode == 0x0100u && kind == 3u) {
            uint32_t angles = mbc_frame_stack(h) - 96u + 16u;
            ae_wr16(r, angles, 0u);
            ae_wr16(r, angles + MBC_VECTOR_Y, (uint32_t)phase);
            ae_wr16(r, angles + MBC_VECTOR_Z, 0u);
            (void)ae_sdk_RotMatrixZYX(r, angles,
                                      ae_rd32(r, prop + MBC_PROP_ROTATION_MATRIX));
        }
    }
    if (mode == 0x0100u) {
        ae_wr16(r, collider, (uint32_t)next_x);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Z, (uint32_t)next_z);
    } else if (mode == 0x0200u) {
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y, (uint32_t)next_y);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Z, (uint32_t)next_z);
    } else {
        ae_wr16(r, collider, (uint32_t)next_x);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y, (uint32_t)next_y);
    }
    ae_wr16(r, collider + MBC_COLLIDER_QUADRANT_MASK,
            (uint32_t)ae_ColliderComputeQuadrantMask(
                r, h != NULL ? h->zones : NULL, collider));
    (void)ae_GimmickSwingAdvance(r, collider);

    uint16_t raw_duration = (uint16_t)ae_rd16(r, collider + MBC_COLLIDER_OFFSET);
    uint16_t raw_next_elapsed = (uint16_t)(ae_rd16(r, collider + MBC_COLLIDER_PROGRESS) +
                                           (movement_mode == 0u ? 1u : 0xFFFFu));
    ae_wr16(r, collider + MBC_COLLIDER_PROGRESS, raw_next_elapsed);
    if (movement_mode == 0u) {
        if (mbc_s16(raw_next_elapsed) > duration) {
            ae_wr16(r, collider + MBC_COLLIDER_PROGRESS,
                    (uint32_t)(raw_next_elapsed - raw_duration));
        }
    } else if (mbc_s16(raw_next_elapsed) < 0) {
        ae_wr16(r, collider + MBC_COLLIDER_PROGRESS,
                (uint32_t)(raw_next_elapsed + raw_duration));
    }
    if (kind == 5u) {
        uint16_t raw_limit = (uint16_t)ae_rd16(r, collider + MBC_COLLIDER_PHASE_X);
        uint16_t raw_next_phase = (uint16_t)(ae_rd16(r, collider + MBC_COLLIDER_SPEED) +
                                             (movement_mode == 0u ? 1u : 0xFFFFu));
        ae_wr16(r, collider + MBC_COLLIDER_SPEED, raw_next_phase);
        if (movement_mode == 0u && mbc_s16(raw_next_phase) > ae_rds16(r, collider + MBC_COLLIDER_PHASE_X)) {
            ae_wr16(r, collider + MBC_COLLIDER_SPEED,
                    (uint32_t)(raw_next_phase - raw_limit));
        } else if (movement_mode != 0u && mbc_s16(raw_next_phase) < 0) {
            ae_wr16(r, collider + MBC_COLLIDER_SPEED,
                    (uint32_t)(raw_next_phase + raw_limit));
        }
    }
    return 0;
}

/* 80089D64 and 8008AA64 are eight-byte return-zero script actions. */
int32_t ae_ColliderScriptNoOpA(AeRam *r, uint32_t collider, int32_t index)
{
    (void)r;
    (void)collider;
    (void)index;
    return 0;
}

int32_t ae_ColliderScriptNoOpB(AeRam *r, uint32_t collider, int32_t index)
{
    (void)r;
    (void)collider;
    (void)index;
    return 0;
}

static int32_t mbc_mulhi_s32(int32_t left, int32_t right)
{
    uint64_t product = (uint64_t)((int64_t)left * (int64_t)right);
    return mbc_s32((uint32_t)(product >> 32));
}

static int32_t mbc_prop_scale_component(int32_t half_size)
{
    int32_t negated = -half_size;
    int32_t sign = mbc_sra(negated, 31u);
    int32_t quotient = mbc_sra(mbc_mulhi_s32(negated, 0x51EB851F), 5u) - sign;
    int32_t fixed = (int32_t)((uint32_t)quotient << 12);
    int32_t fixed_sign = mbc_sra(fixed, 31u);
    int32_t product_high = mbc_mulhi_s32(fixed, (int32_t)0xB60B60B7u);
    int32_t scaled = mbc_sra(mbc_s32((uint32_t)product_high + (uint32_t)fixed), 8u)
                    - fixed_sign;
    return (int16_t)(uint16_t)scaled;
}

static void mbc_transform_actor(AeRam *r, uint32_t actor,
                                uint32_t collider, uint32_t script_record,
                                int32_t index)
{
    if (ae_rds16(r, actor + MBC_PLAYER_AREA) != index) return;

    uint32_t x = actor + MBC_PLAYER_POSITION_X;
    uint32_t y = actor + MBC_PLAYER_POSITION_Y;
    uint32_t z = actor + MBC_PLAYER_POSITION_Z;
    int32_t collider_y = ae_rds16(r, collider + MBC_COLLIDER_HALF_Y);
    int32_t script_y = ae_rds16(r, script_record + MBC_SCRIPT_HALF_Y);
    ae_wr32(r, y, ae_rd32(r, y) + (uint32_t)(collider_y - script_y));

    int32_t actor_x = ae_rds32(r, x);
    int32_t center_x = ae_rds16(r, collider + MBC_COLLIDER_POSITION_X);
    int32_t half_delta_x = ae_rds16(r, collider + MBC_COLLIDER_HALF_X) -
                           ae_rds16(r, script_record + MBC_SCRIPT_HALF_X);
    uint32_t next_x = actor_x < center_x
        ? (uint32_t)actor_x + (uint32_t)half_delta_x
        : (uint32_t)actor_x - (uint32_t)half_delta_x;
    ae_wr32(r, x, next_x);

    int32_t actor_z = ae_rds32(r, z);
    int32_t center_z = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z);
    int32_t half_delta_z = ae_rds16(r, collider + MBC_COLLIDER_HALF_Z) -
                           ae_rds16(r, script_record + MBC_SCRIPT_HALF_Z);
    uint32_t next_z = actor_z < center_z
        ? (uint32_t)actor_z + (uint32_t)half_delta_z
        : (uint32_t)actor_z - (uint32_t)half_delta_z;
    ae_wr32(r, z, next_z);

}

/* 80089D6C: move a scripted collider toward its linked target and synchronize
 * its prop matrix and the player or RC car when they occupy that area. */
int32_t ae_ColliderScriptPropTransformUpdate(AeRam *r,
                                             const AeMissbHooks *h,
                                             uint32_t collider, int32_t index)
{
    int32_t linked_index = ae_rds16(r, collider + MBC_COLLIDER_LINK_INDEX);
    if (linked_index < 0) return 0;

    uint32_t stage = MBC_STAGE_STATE;
    uint32_t records = ae_rd32(r, stage + MBC_STAGE_RECORD_LIST);
    uint32_t script_record = records + (uint32_t)linked_index * 16u;
    int32_t target_x = ae_rds16(r, script_record);
    int32_t target_y = ae_rds16(r, script_record + MBC_SCRIPT_POSITION_Y);
    int32_t target_z = ae_rds16(r, script_record + MBC_SCRIPT_POSITION_Z);
    uint32_t delta = mbc_frame_stack(h) - 88u + 24u;
    ae_wr16(r, delta, (uint32_t)(target_x - ae_rds16(r, collider)));
    ae_wr16(r, delta + MBC_VECTOR_Y,
            (uint32_t)(target_y - ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y)));
    ae_wr16(r, delta + MBC_VECTOR_Z,
            (uint32_t)(target_z - ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z)));

    const AeGimmickAHooks *gimmick_hooks = mbc_gimmick_hooks(r, h);
    if (gimmick_hooks == NULL) return 0;
    (void)ae_GimmickCarryBodiesByDelta(r, gimmick_hooks, collider, delta,
                                       mbc_s16((uint32_t)index));

    uint8_t collider_type = ae_rd8(r, collider + MBC_COLLIDER_KIND);
    if (collider_type == 14u) {
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            uint32_t field = collider + MBC_COLLIDER_ORIGIN_X + axis * 2u;
            ae_wr16(r, field, ae_rd16(r, field) + ae_rd16(r, delta + axis * 2u));
        }
        if (ae_rds16(r, stage + MBC_STAGE_PLAYER_MODE) == 6 &&
            ae_rds16(r, MBC_PLAYER + MBC_PLAYER_AREA) == mbc_s16((uint32_t)index)) {
            ae_PlayerSetRope(r, collider, collider + MBC_COLLIDER_ORIGIN_X);
        }
    }

    uint8_t collider_action = ae_rd8(r, collider + MBC_COLLIDER_ACTION);
    if ((collider_action == 59u || collider_action == 89u) &&
        ae_rds16(r, stage + MBC_STAGE_PLAYER_MODE) == 5 &&
        ae_rds16(r, MBC_PLAYER + MBC_PLAYER_AREA) == mbc_s16((uint32_t)index)) {
        ae_PlayerSetClimbTarget(r, target_x, target_y, target_z,
            ae_rds16(r, collider + MBC_COLLIDER_HALF_X),
            ae_rds16(r, collider + MBC_COLLIDER_HALF_Y),
            collider_action == 59u ? 1 : 0);
    }

    int32_t prop_index = ae_rds16(r, collider + MBC_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = ae_rd32(r, MBC_AREA_PROPS) +
                        (uint32_t)prop_index * MBC_AREA_PROP_STRIDE;
        ae_wr16(r, prop + MBC_PROP_POSITION_X, (uint32_t)target_x);
        ae_wr16(r, prop + MBC_PROP_POSITION_Y, (uint32_t)target_y);
        ae_wr16(r, prop + MBC_PROP_POSITION_Z, (uint32_t)target_z);
        uint32_t model = ae_rd32(r, prop);
        if ((ae_rd32(r, model) & 0xFu) == 3u) {
            uint32_t matrix = ae_rd32(r, prop + MBC_PROP_ROTATION_MATRIX);
            if (collider_type == 8u) {
                uint32_t scale = mbc_frame_stack(h) - 88u + 32u;
                ae_wr16(r, scale, (uint32_t)mbc_prop_scale_component(
                    ae_rds16(r, script_record + MBC_SCRIPT_SCALE_X)));
                ae_wr16(r, scale + MBC_VECTOR_Y, (uint32_t)mbc_prop_scale_component(
                    ae_rds16(r, script_record + MBC_SCRIPT_SCALE_Y)));
                ae_wr16(r, scale + MBC_VECTOR_Z, (uint32_t)mbc_prop_scale_component(
                    ae_rds16(r, script_record + MBC_SCRIPT_SCALE_Z)));
                (void)ae_sdk_RotMatrixZYX(r, scale, matrix);
                int32_t area = mbc_s16((uint32_t)index);
                if (ae_rds16(r, MBC_PLAYER + MBC_PLAYER_AREA) == area) {
                    int32_t angle = ae_rds16(r, scale + MBC_VECTOR_Y);
                    ae_wr16(r, MBC_PLAYER + MBC_PLAYER_FACE_X,
                            (uint32_t)ae_sdk_rcos(r, -angle));
                    ae_wr16(r, MBC_PLAYER + MBC_PLAYER_FACE_Z,
                            (uint32_t)ae_sdk_rsin(r, -angle));
                    ae_wr32(r, MBC_PLAYER + MBC_PLAYER_POSITION_X, (uint32_t)target_x);
                    ae_wr32(r, MBC_PLAYER + MBC_PLAYER_POSITION_Z, (uint32_t)target_z);
                }
                if (ae_rds16(r, MBC_RCCAR + MBC_RCCAR_AREA) == area) {
                    int32_t angle = ae_rds16(r, scale + MBC_VECTOR_Y);
                    ae_wr16(r, MBC_RCCAR + MBC_PLAYER_FACE_X,
                            (uint32_t)ae_sdk_rcos(r, -angle));
                    ae_wr16(r, MBC_RCCAR + MBC_PLAYER_FACE_Z,
                            (uint32_t)ae_sdk_rsin(r, -angle));
                    ae_wr32(r, MBC_RCCAR + MBC_PLAYER_POSITION_X, (uint32_t)target_x);
                    ae_wr32(r, MBC_RCCAR + MBC_PLAYER_POSITION_Z, (uint32_t)target_z);
                }
            } else {
                uint32_t scale = mbc_frame_stack(h) - 88u + 32u;
                for (uint32_t axis = 0; axis < 3u; ++axis) {
                    int32_t half_size = ae_rds16(
                        r, script_record + MBC_SCRIPT_HALF_X + axis * MBC_COLLIDER_AXIS_STRIDE);
                    int32_t dimension = ae_rds16(
                        r, collider + MBC_COLLIDER_EXTENT_BASE + axis * MBC_COLLIDER_AXIS_STRIDE);
                    int32_t numerator = (int32_t)((uint32_t)half_size << 12);
                    if (dimension == 0) {
                        ae_ram_fail(r, "ColliderScriptPropTransformUpdate: zero prop dimension");
                        return 0;
                    }
                    ae_wr16(r, scale + axis * 2u, (uint32_t)(numerator / dimension));
                }
                ae_SvecToDiagMatrix(r, scale, matrix);
                mbc_transform_actor(r, MBC_PLAYER, collider, script_record,
                                    mbc_s16((uint32_t)index));
                mbc_transform_actor(r, MBC_RCCAR, collider, script_record,
                                    mbc_s16((uint32_t)index));
            }
        }
    }

    ae_wr16(r, collider, (uint32_t)target_x);
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y, (uint32_t)target_y);
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_Z, (uint32_t)target_z);
    if (collider_type == 12u) {
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            ae_wr16(r, collider + MBC_COLLIDER_HALF_X + axis * MBC_COLLIDER_AXIS_STRIDE,
                    ae_rd16(r, script_record + MBC_SCRIPT_HALF_X + axis * MBC_COLLIDER_AXIS_STRIDE));
        }
    }
    ae_wr16(r, collider + MBC_COLLIDER_QUADRANT_MASK,
            (uint32_t)ae_ColliderComputeQuadrantMask(
                r, h != NULL ? h->zones : NULL, collider));
    int32_t counter = mbc_wrap_add16(ae_rds16(r, collider + MBC_COLLIDER_SPEED), 1);
    ae_wr16(r, collider + MBC_COLLIDER_SPEED, (uint32_t)counter);
    ae_wr16(r, collider + MBC_COLLIDER_LINK_INDEX,
            ae_rd16(r, script_record + MBC_SCRIPT_DIRECTION));
    if (ae_rds16(r, script_record + MBC_SCRIPT_DURATION) <= 0) {
        ae_wr16(r, collider + MBC_COLLIDER_SPEED, 0u);
    }
    return 0;
}

static int32_t mbc_abs_delta_less_than(int32_t point, int32_t center,
                                      int32_t extent)
{
    int32_t delta = mbc_s32((uint32_t)point - (uint32_t)center);
    if (delta >= 0) return delta < extent;
    delta = mbc_s32((uint32_t)center - (uint32_t)point);
    return delta < extent;
}

static int32_t mbc_player_vertical_overlap(AeRam *r, uint32_t collider,
                                          int32_t transformed_height)
{
    int32_t player_y = ae_rds32(r, MBC_PLAYER + MBC_PLAYER_POSITION_Y);
    int32_t collider_y = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y);
    int32_t top_test = mbc_s32((uint32_t)collider_y - 16u);
    int32_t lower_test = mbc_s32((uint32_t)collider_y -
                                  (uint32_t)transformed_height + 32u);
    return top_test >= player_y && player_y >= lower_test;
}

static int32_t mbc_round_fixed_product(int32_t value, int32_t scale,
                                       uint32_t shift)
{
    return mbc_fixed_product(value, scale, shift);
}

/* 80092324: update one face of an oriented collision box around its saved
 * origin, move the player out of the active wall, and report the response
 * geometry. */
int32_t ae_GimmickColliderContactUpdate(AeRam *r,
                                       const AeMissbHooks *h,
                                       uint32_t gimmick, uint32_t collider)
{
    int32_t orientation = ae_rds16(r, gimmick + MBC_GIMMICK_ORIENTATION);
    int32_t angle = ae_rds16(r, gimmick + MBC_GIMMICK_ANGLE);
    int32_t transformed_height;
    int32_t transformed_side;
    int32_t geometry = 0;
    int32_t touching = 0;
    int32_t player_radius = ae_rds16(r, MBC_PLAYER + MBC_PLAYER_BODY_RADIUS);
    int32_t extent_y = ae_rds16(r, collider + MBC_COLLIDER_EXTENT_Y);
    int32_t extent_test = mbc_s32((uint32_t)extent_y - 32u);

    if (orientation >= 4 && orientation <= 7) {
        int32_t extent_z = ae_rds16(r, collider + MBC_COLLIDER_EXTENT_Z);
        int32_t height_scale = (orientation <= 5)
            ? ae_sdk_rsin(r, angle) : ae_sdk_rcos(r, angle);
        int32_t side_scale = (orientation <= 5)
            ? ae_sdk_rcos(r, angle) : ae_sdk_rsin(r, angle);
        transformed_height = mbc_round_fixed_product(
            extent_z, mbc_s32((uint32_t)height_scale << 1), 12u);
        transformed_side = mbc_round_fixed_product(
            extent_z, mbc_s32((uint32_t)side_scale << 1), 13u);

        if (extent_test < transformed_height || transformed_side < 64) {
            transformed_side = ae_rds16(r, gimmick + MBC_GIMMICK_PARAM_Y);
            if (mbc_player_vertical_overlap(r, collider, transformed_height)) {
                int32_t player_x = ae_rds32(r, MBC_PLAYER + MBC_PLAYER_POSITION_X);
                int32_t player_z = ae_rds32(r, MBC_PLAYER + MBC_PLAYER_POSITION_Z);
                int32_t collider_x = ae_rds16(r, collider + MBC_COLLIDER_POSITION_X);
                int32_t collider_z = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z);
                int32_t half_x = ae_rds16(r, collider + MBC_COLLIDER_HALF_X);
                int32_t half_z = ae_rds16(r, collider + MBC_COLLIDER_HALF_Z);
                touching = mbc_abs_delta_less_than(player_x, collider_x, half_x) &&
                    mbc_abs_delta_less_than(player_z, collider_z,
                                            half_z + player_radius);
            }
            transformed_height = extent_y;
        } else if (transformed_height < 64) {
            transformed_height = mbc_s32(
                (uint32_t)ae_rds16(r, gimmick + MBC_GIMMICK_PARAM_Y) << 1);
            transformed_side = extent_z;
        } else {
            geometry = 0x2000;
        }

        ae_wr16(r, collider + MBC_COLLIDER_HALF_Z,
                (uint32_t)transformed_side);
        ae_wr16(r, collider + MBC_COLLIDER_HALF_Y,
                (uint32_t)transformed_height);
    } else if (orientation >= 0 && orientation <= 3) {
        int32_t extent_x = ae_rds16(r, collider + MBC_COLLIDER_EXTENT_X);
        int32_t height_scale = (orientation <= 1)
            ? ae_sdk_rsin(r, angle) : ae_sdk_rcos(r, angle);
        int32_t side_scale = (orientation <= 1)
            ? ae_sdk_rcos(r, angle) : ae_sdk_rsin(r, angle);
        transformed_height = mbc_round_fixed_product(
            extent_x, mbc_s32((uint32_t)height_scale << 1), 12u);
        transformed_side = mbc_round_fixed_product(
            extent_x, mbc_s32((uint32_t)side_scale << 1), 13u);

        if (extent_test < transformed_height || transformed_side < 64) {
            transformed_side = ae_rds16(r, gimmick + MBC_GIMMICK_PARAM_Y);
            if (mbc_player_vertical_overlap(r, collider, transformed_height)) {
                int32_t player_x = ae_rds32(r, MBC_PLAYER + MBC_PLAYER_POSITION_X);
                int32_t player_z = ae_rds32(r, MBC_PLAYER + MBC_PLAYER_POSITION_Z);
                int32_t collider_x = ae_rds16(r, collider + MBC_COLLIDER_POSITION_X);
                int32_t collider_z = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z);
                int32_t half_x = ae_rds16(r, collider + MBC_COLLIDER_HALF_X);
                int32_t half_z = ae_rds16(r, collider + MBC_COLLIDER_HALF_Z);
                touching = mbc_abs_delta_less_than(player_z, collider_z, half_z) &&
                    mbc_abs_delta_less_than(player_x, collider_x,
                                            half_x + player_radius);
            }
            transformed_height = extent_y;
        } else if (transformed_height < 64) {
            transformed_height = mbc_s32(
                (uint32_t)ae_rds16(r, gimmick + MBC_GIMMICK_PARAM_Y) << 1);
            transformed_side = extent_x;
        } else {
            geometry = 0x2000;
        }

        ae_wr16(r, collider + MBC_COLLIDER_HALF_X,
                (uint32_t)transformed_side);
        ae_wr16(r, collider + MBC_COLLIDER_HALF_Y,
                (uint32_t)transformed_height);
    } else {
        if (h == NULL) {
            ae_ram_fail(r, "AeMissbHooks required for stale caller registers in GimmickColliderContactUpdate");
            return 0;
        }
        transformed_height = h->caller_s1;
        transformed_side = mbc_s32(h->caller_a3);
        geometry = h->caller_s2;
    }

    int32_t mode = 6;
    if (angle < 512 && touching == 0 && geometry == 0x2000)
        mode = 14;
    ae_wr16(r, collider + MBC_COLLIDER_MODE, (uint32_t)mode);
    ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
            ae_rd16(r, collider + MBC_COLLIDER_ORIGIN_Y));

    uint32_t player = MBC_PLAYER;
    int32_t position_y = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Y);
    int32_t origin_x = ae_rd16(r, collider + MBC_COLLIDER_ORIGIN_X);
    int32_t origin_z = ae_rd16(r, collider + MBC_COLLIDER_ORIGIN_Z);
    int32_t extent_x = ae_rd16(r, collider + MBC_COLLIDER_EXTENT_X);
    int32_t extent_z = ae_rd16(r, collider + MBC_COLLIDER_EXTENT_Z);

    switch (orientation) {
    case 0:
    case 1: {
        int32_t delta_x = mbc_s32((uint32_t)extent_x -
                                  (uint32_t)transformed_side);
        int32_t x = orientation == 0
            ? mbc_s32((uint32_t)origin_x + (uint32_t)delta_x)
            : mbc_s32((uint32_t)origin_x - (uint32_t)delta_x);
        int32_t delta_y = mbc_s32((uint32_t)extent_y -
                                  (uint32_t)transformed_height);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_X, (uint32_t)x);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
                (uint32_t)mbc_s32((uint32_t)position_y -
                                  (uint32_t)delta_y));
        if (geometry == 0x2000)
            geometry = orientation == 0 ? 0x2058 : 0x2056;
        break;
    }
    case 2:
    case 3: {
        int32_t delta_x = mbc_s32((uint32_t)extent_x -
                                  (uint32_t)transformed_side);
        int32_t x = orientation == 2
            ? mbc_s32((uint32_t)origin_x + (uint32_t)delta_x)
            : mbc_s32((uint32_t)origin_x - (uint32_t)delta_x);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_X, (uint32_t)x);
        if (geometry == 0x2000) {
            geometry = orientation == 2 ? 0x2056 : 0x2058;
            int32_t rise = mbc_s32(
                (uint32_t)ae_rds16(r, gimmick + MBC_GIMMICK_PARAM_Y) << 1);
            ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
                    (uint32_t)mbc_s32((uint32_t)position_y -
                                      (uint32_t)rise));
        } else if (touching) {
            int32_t reach = ae_rds16(r, collider + MBC_COLLIDER_HALF_X) +
                            player_radius;
            int32_t player_x = ae_rds32(r, player + MBC_PLAYER_POSITION_X);
            int32_t x16 = mbc_s16((uint32_t)x);
            int32_t offset = orientation == 2
                ? mbc_s32((uint32_t)x16 - (uint32_t)player_x)
                : mbc_s32((uint32_t)player_x - (uint32_t)x16);
            int32_t edge = mbc_s32((uint32_t)reach - (uint32_t)offset);
            int32_t pushed_x = orientation == 2
                ? mbc_s32((uint32_t)player_x + (uint32_t)edge)
                : mbc_s32((uint32_t)player_x - (uint32_t)edge);
            ae_wr32(r, player + MBC_PLAYER_POSITION_X, (uint32_t)pushed_x);
        }
        break;
    }
    case 4:
    case 5: {
        int32_t delta_z = mbc_s32((uint32_t)extent_z -
                                  (uint32_t)transformed_side);
        int32_t z = orientation == 4
            ? mbc_s32((uint32_t)origin_z + (uint32_t)delta_z)
            : mbc_s32((uint32_t)origin_z - (uint32_t)delta_z);
        int32_t delta_y = mbc_s32((uint32_t)extent_y -
                                  (uint32_t)transformed_height);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Z, (uint32_t)z);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
                (uint32_t)mbc_s32((uint32_t)position_y -
                                  (uint32_t)delta_y));
        if (geometry == 0x2000)
            geometry = orientation == 4 ? 0x2055 : 0x2057;
        break;
    }
    case 6:
    case 7: {
        int32_t delta_z = mbc_s32((uint32_t)extent_z -
                                  (uint32_t)transformed_side);
        int32_t z = orientation == 6
            ? mbc_s32((uint32_t)origin_z - (uint32_t)delta_z)
            : mbc_s32((uint32_t)origin_z + (uint32_t)delta_z);
        ae_wr16(r, collider + MBC_COLLIDER_POSITION_Z, (uint32_t)z);
        if (geometry == 0x2000) {
            geometry = orientation == 6 ? 0x2055 : 0x2057;
            int32_t rise = mbc_s32(
                (uint32_t)ae_rds16(r, gimmick + MBC_GIMMICK_PARAM_Y) << 1);
            ae_wr16(r, collider + MBC_COLLIDER_POSITION_Y,
                    (uint32_t)mbc_s32((uint32_t)position_y -
                                      (uint32_t)rise));
        } else if (touching) {
            int32_t reach = ae_rds16(r, collider + MBC_COLLIDER_HALF_Z) +
                            player_radius;
            int32_t pushed_z = orientation == 6
                ? mbc_s32((uint32_t)mbc_s16((uint32_t)z) - (uint32_t)reach)
                : mbc_s32((uint32_t)mbc_s16((uint32_t)z) + (uint32_t)reach);
            ae_wr32(r, player + MBC_PLAYER_POSITION_Z, (uint32_t)pushed_z);
        }
        break;
    }
    default:
        break;
    }

    int32_t player_x = ae_rds32(r, player + MBC_PLAYER_POSITION_X);
    int32_t player_z = ae_rds32(r, player + MBC_PLAYER_POSITION_Z);
    int32_t center_x = ae_rds16(r, collider + MBC_COLLIDER_POSITION_X);
    int32_t center_z = ae_rds16(r, collider + MBC_COLLIDER_POSITION_Z);
    int32_t half_x = ae_rds16(r, collider + MBC_COLLIDER_HALF_X);
    int32_t half_z = ae_rds16(r, collider + MBC_COLLIDER_HALF_Z);
    if (mbc_abs_delta_less_than(player_x, center_x, half_x) &&
        mbc_abs_delta_less_than(player_z, center_z, half_z)) {
        if (h == NULL || h->world == NULL) {
            ae_ram_fail(r, "AeMissbHooks.world not provided for GetGroundHeight");
            return geometry;
        }
        int32_t player_y = ae_rds32(r, player + MBC_PLAYER_POSITION_Y);
        int32_t vertical_delta = ae_rds32(r, player + MBC_PLAYER_VERTICAL_DELTA);
        int32_t next_y = mbc_s32((uint32_t)player_y + (uint32_t)vertical_delta);
        int32_t ground = ae_GetGroundHeight(r, h->world, player_x, next_y, player_z);
        if (ground < next_y)
            ae_wr32(r, player + MBC_PLAYER_POSITION_Y, (uint32_t)ground);
    }
    return geometry;
}
