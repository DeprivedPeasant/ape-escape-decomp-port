#include "ae_playerupdate.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

#define TURN_MATRIX 0x1F800028u
#define VECTOR_X 0u
#define VECTOR_Z 4u
#define SVECTOR_SIZE 8u

/* TurnTowardsY @80066C90. The cross-product division truncates toward zero,
 * whereas the dot-product shift rounds down. Both intermediate sums wrap. */
void ae_TurnTowardsY(AeRam *r, const AePlayerUpdateHooks *hooks,
                     uint32_t target, uint32_t direction, int32_t max_angle)
{
    int32_t target_x = ae_rds16(r, target + VECTOR_X);
    int32_t target_z = ae_rds16(r, target + VECTOR_Z);
    int32_t direction_x = ae_rds16(r, direction + VECTOR_X);
    int32_t direction_z = ae_rds16(r, direction + VECTOR_Z);
    int32_t dot = (int32_t)((uint32_t)(target_x * direction_x) +
                            (uint32_t)(target_z * direction_z));
    if ((dot >> 12) < ae_sdk_rcos(r, max_angle)) {
        ae_sdk_MatrixIdentity(r, TURN_MATRIX);
        /* Re-read after the matrix write: callers may alias scratchpad. */
        target_x = ae_rds16(r, target + VECTOR_X);
        target_z = ae_rds16(r, target + VECTOR_Z);
        direction_x = ae_rds16(r, direction + VECTOR_X);
        direction_z = ae_rds16(r, direction + VECTOR_Z);
        int32_t cross = (int32_t)((uint32_t)(target_x * direction_z) -
                                 (uint32_t)(target_z * direction_x));
        int32_t angle = cross / 4096 > 0 ? max_angle :
                         (int32_t)(0u - (uint32_t)max_angle);
        if (!hooks || !hooks->RotMatrixY) {
            ae_ram_fail(r, "TurnTowardsY: missing RotMatrixY hook");
            return;
        }
        hooks->RotMatrixY(hooks->user, r, (uint32_t)angle, TURN_MATRIX);
        ae_sdk_ApplyMatrixSV(r, TURN_MATRIX, direction, direction);
    } else {
        /* The two unaligned word loads precede all stores, including padding. */
        uint8_t copy[SVECTOR_SIZE];
        for (uint32_t i = 0; i < SVECTOR_SIZE; ++i)
            copy[i] = (uint8_t)ae_rd8(r, target + i);
        for (uint32_t i = 0; i < SVECTOR_SIZE; ++i)
            ae_wr8(r, direction + i, copy[i]);
    }
}
