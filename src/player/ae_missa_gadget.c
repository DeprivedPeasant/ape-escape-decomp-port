#include "../core/ae_missa.h"
#include "../core/ae_sdk.h"
#include "native_player.h"

#define AE_MISSA_TARGET_LIST              0x800F51F0u
#define AE_MISSA_TARGET_LIST_COUNT        32u
#define AE_MISSA_TARGET_RECORD_POSITION_X 0x00u
#define AE_MISSA_TARGET_RECORD_POSITION_Y 0x02u
#define AE_MISSA_TARGET_RECORD_POSITION_Z 0x04u
#define AE_MISSA_TARGET_VECTOR_SIZE       8u
#define AE_MISSA_TARGET_VECTOR_Y          0x02u
#define AE_MISSA_TARGET_VECTOR_Z          0x04u
#define AE_MISSA_TARGET_DISTANCE_INITIAL  0x40000000u
#define AE_MISSA_TARGET_TEMP_VECTOR       0x801FEF90u

static int32_t missa_gadget_asr1(int32_t value)
{
    if (value >= 0) return value / 2;
    return -((-(int64_t)value + 1) / 2);
}

uint32_t ae_TargetListFindBest(AeRam *r, const AeMissaHooks *hooks,
                               uint32_t position, uint32_t direction,
                               int32_t max_angle)
{
    uint32_t vector = hooks != NULL && hooks->stack_pointer != 0u ?
                      hooks->stack_pointer - 64u + 16u :
                      AE_MISSA_TARGET_TEMP_VECTOR;
    uint32_t best_distance = AE_MISSA_TARGET_DISTANCE_INITIAL;
    int32_t best_index = -1;

    for (uint32_t index = 0; index < AE_MISSA_TARGET_LIST_COUNT; ++index) {
        uint32_t target = ae_rd32(r, AE_MISSA_TARGET_LIST + index * 4u);
        if (target == 0u) continue;

        int32_t delta_x = (int32_t)ae_rd16(r, target + AE_MISSA_TARGET_RECORD_POSITION_X) -
                          (int32_t)ae_rd16(r, position);
        int32_t delta_y = (int32_t)ae_rd16(r, target + AE_MISSA_TARGET_RECORD_POSITION_Y) -
                          (int32_t)ae_rd16(r, position + 4u);
        int32_t delta_z = (int32_t)ae_rd16(r, target + AE_MISSA_TARGET_RECORD_POSITION_Z) -
                          (int32_t)ae_rd16(r, position + 8u);
        ae_wr16(r, vector, (uint32_t)delta_x);
        ae_wr16(r, vector + AE_MISSA_TARGET_VECTOR_Y, (uint32_t)delta_y);
        ae_wr16(r, vector + AE_MISSA_TARGET_VECTOR_Z, (uint32_t)delta_z);
        int32_t distance_squared = ae_sdk_VectorNormalSS(r, vector, vector);

        int32_t angle_x = (int32_t)ae_rds16(r, vector) - ae_rds16(r, direction);
        int32_t angle_y = missa_gadget_asr1(
            (int32_t)ae_rds16(r, vector + AE_MISSA_TARGET_VECTOR_Y) -
            ae_rds16(r, direction + AE_MISSA_TARGET_VECTOR_Y));
        int32_t angle_z = (int32_t)ae_rds16(r, vector + AE_MISSA_TARGET_VECTOR_Z) -
                          ae_rds16(r, direction + AE_MISSA_TARGET_VECTOR_Z);
        int32_t angle = ae_player_vec_length3(angle_x, angle_y, angle_z);
        if (angle < max_angle && (uint32_t)distance_squared < best_distance) {
            best_distance = (uint32_t)distance_squared;
            best_index = (int32_t)index;
        }
    }

    if (best_index < 0) return 0u;
    return ae_rd32(r, AE_MISSA_TARGET_LIST + (uint32_t)best_index * 4u);
}
