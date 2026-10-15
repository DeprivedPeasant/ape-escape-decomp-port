#include "../core/ae_missa.h"
#include "ae_stage.h"
#include "../player/native_player.h"

#define AE_MISSA_STAGE_STATE                 0x800F4350u
#define AE_MISSA_STAGE_ROUTE_KIND            0x0120u
#define AE_MISSA_STAGE_CURRENT_AREA          0x0126u
#define AE_MISSA_STAGE_CURRENT_ENTRY         0x012Au
#define AE_MISSA_STAGE_TRANSITION_TYPE       0x012Cu
#define AE_MISSA_STAGE_EXIT_LIST             0x0238u
#define AE_MISSA_STAGE_EXIT_COUNT            0x028Cu
#define AE_MISSA_PLAYER_POSITION             0x800EC1E0u
#define AE_MISSA_PLAYER_POSITION_X           0x0024u
#define AE_MISSA_EXIT_RECORD_STRIDE           0x0028u
#define AE_MISSA_EXIT_TYPE                    0x0000u
#define AE_MISSA_EXIT_CENTER_X                0x0004u
#define AE_MISSA_EXIT_CENTER_Y                0x0008u
#define AE_MISSA_EXIT_CENTER_Z                0x000Cu
#define AE_MISSA_EXIT_RADIUS                  0x0010u
#define AE_MISSA_EXIT_EXTENT_X                0x0014u
#define AE_MISSA_EXIT_EXTENT_Y                0x0018u
#define AE_MISSA_EXIT_EXTENT_Z                0x001Cu
#define AE_MISSA_EXIT_DESTINATION             0x0020u
#define AE_MISSA_EXIT_ENTRY                   0x0024u
#define AE_MISSA_ROUTE_KIND_LOCKED            36u
#define AE_MISSA_ROUTE_KIND_STAGE_17          17u
#define AE_MISSA_EXIT_DISTANCE_SPHERE         0u
#define AE_MISSA_EXIT_DISTANCE_SPECIAL        2u
#define AE_MISSA_EXIT_DISABLED                3u
#define AE_MISSA_EXIT_STAGE_REQUEST           4u
#define AE_MISSA_STAGE_REQUEST_WARP           22
#define AE_MISSA_STAGE_REQUEST_EXIT           27

static int32_t missa_exit_signed_difference(uint32_t left, uint32_t right)
{
    return (int32_t)(left - right);
}

static int missa_exit_axis_inside(uint32_t player_value, uint32_t center,
                                  int32_t extent)
{
    uint32_t difference = player_value - center;
    if ((int32_t)difference < 0) difference = 0u - difference;
    return (int32_t)difference < extent;
}

static int32_t missa_exit_result(AeRam *r, uint32_t record)
{
    uint32_t destination = ae_rd32(r, record + AE_MISSA_EXIT_DESTINATION);
    uint32_t entry = ae_rd32(r, record + AE_MISSA_EXIT_ENTRY);
    return (int32_t)((destination << 8) | entry);
}

int32_t ae_AreaCheckExits(AeRam *r, const AeMissaHooks *hooks, uint32_t mode)
{
    uint32_t stage = AE_MISSA_STAGE_STATE;
    uint32_t exits = ae_rd32(r, stage + AE_MISSA_STAGE_EXIT_LIST);
    int32_t route_kind = (int16_t)ae_rd16(r, stage + AE_MISSA_STAGE_ROUTE_KIND);
    int32_t exit_count = ae_rds32(r, stage + AE_MISSA_STAGE_EXIT_COUNT);
    uint32_t player = AE_MISSA_PLAYER_POSITION;

    if (exits == 0u) return -1;
    if (route_kind == AE_MISSA_ROUTE_KIND_LOCKED) return -1;
    if (exit_count <= 0) return -1;

    for (int32_t index = 0; index < exit_count; ++index) {
        uint32_t record = exits + (uint32_t)index * AE_MISSA_EXIT_RECORD_STRIDE;
        uint32_t type = ae_rd32(r, record + AE_MISSA_EXIT_TYPE);

        if (mode == 0u) {
            if (type == AE_MISSA_EXIT_DISTANCE_SPECIAL &&
                route_kind == AE_MISSA_ROUTE_KIND_STAGE_17) {
                int inside = missa_exit_axis_inside(
                                 ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X),
                                 ae_rd32(r, record + AE_MISSA_EXIT_CENTER_X),
                                 ae_rds32(r, record + AE_MISSA_EXIT_EXTENT_X)) &&
                             missa_exit_axis_inside(
                                 ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 4u),
                                 ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Y),
                                 ae_rds32(r, record + AE_MISSA_EXIT_EXTENT_Y)) &&
                             missa_exit_axis_inside(
                                 ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 8u),
                                 ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Z),
                                 ae_rds32(r, record + AE_MISSA_EXIT_EXTENT_Z));
                if (!inside) continue;
                (void)ae_StageRequest(r, hooks != NULL ? hooks->stage : NULL,
                                     AE_MISSA_STAGE_REQUEST_WARP,
                                     (int32_t)ae_rd16(r, stage + AE_MISSA_STAGE_CURRENT_AREA),
                                     (int32_t)ae_rd16(r, stage + AE_MISSA_STAGE_CURRENT_ENTRY));
                return -1;
            }

            int32_t dx = missa_exit_signed_difference(
                ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X),
                ae_rd32(r, record + AE_MISSA_EXIT_CENTER_X));
            int32_t dy = missa_exit_signed_difference(
                ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 4u),
                ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Y));
            int32_t dz = missa_exit_signed_difference(
                ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 8u),
                ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Z));
            uint32_t distance = (uint32_t)ae_player_vec_length3(dx, dy, dz);
            if (distance < ae_rd32(r, record + AE_MISSA_EXIT_RADIUS)) {
                return missa_exit_result(r, record);
            }
            continue;
        }

        if (type == AE_MISSA_EXIT_DISTANCE_SPHERE) {
            int32_t dx = missa_exit_signed_difference(
                ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X),
                ae_rd32(r, record + AE_MISSA_EXIT_CENTER_X));
            int32_t dy = missa_exit_signed_difference(
                ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 4u),
                ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Y));
            int32_t dz = missa_exit_signed_difference(
                ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 8u),
                ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Z));
            uint32_t distance = (uint32_t)ae_player_vec_length3(dx, dy, dz);
            if (distance < ae_rd32(r, record + AE_MISSA_EXIT_EXTENT_X)) {
                return missa_exit_result(r, record);
            }
            continue;
        }

        if (type == AE_MISSA_EXIT_DISABLED) return -1;

        int inside = missa_exit_axis_inside(
                         ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X),
                         ae_rd32(r, record + AE_MISSA_EXIT_CENTER_X),
                         ae_rds32(r, record + AE_MISSA_EXIT_EXTENT_X)) &&
                     missa_exit_axis_inside(
                         ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 4u),
                         ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Y),
                         ae_rds32(r, record + AE_MISSA_EXIT_EXTENT_Y)) &&
                     missa_exit_axis_inside(
                         ae_rd32(r, player + AE_MISSA_PLAYER_POSITION_X + 8u),
                         ae_rd32(r, record + AE_MISSA_EXIT_CENTER_Z),
                         ae_rds32(r, record + AE_MISSA_EXIT_EXTENT_Z));
        if (!inside) continue;

        if (type == AE_MISSA_EXIT_STAGE_REQUEST) {
            (void)ae_StageRequest(r, hooks != NULL ? hooks->stage : NULL,
                                 AE_MISSA_STAGE_REQUEST_EXIT,
                                 (int32_t)ae_rd16(r, stage + AE_MISSA_STAGE_CURRENT_AREA),
                                 (int32_t)ae_rd16(r, stage + AE_MISSA_STAGE_CURRENT_ENTRY));
            return -1;
        }
        return missa_exit_result(r, record);
    }
    return -1;
}
