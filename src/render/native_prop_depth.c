/* Original ordinary-prop F4 depth-acceptance prefix (see native_prop_depth.md). */
#include "native_prop_depth.h"

#include <stddef.h>

int ae_prop_f4_depth(uint16_t otz, uint16_t near, uint8_t flags, uint16_t *out_depth)
{
    int32_t depth = (int32_t)otz;

    if (out_depth == NULL)
        return 0;
    if (depth < AE_PROP_F4_DEPTH_MIN)
        return 0;
    if ((flags & AE_PROP_F4_FLAG_NO_NEAR_TEST) == 0u) {
        int32_t delta = depth - (int32_t)near;
        if (delta >= AE_PROP_F4_NEAR_WINDOW)
            return 0;
    }
    if (depth >= AE_PROP_F4_DEPTH_LIMIT)
        depth = AE_PROP_F4_DEPTH_CLAMPED;
    *out_depth = (uint16_t)depth;
    return 1;
}
