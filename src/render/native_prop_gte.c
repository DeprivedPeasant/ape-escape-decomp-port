/* Restricted GTE AVSZ3/AVSZ4 and DPCS helpers (see native_prop_gte.md). */
#include "native_prop_gte.h"

#include <string.h>

static uint16_t otz_saturate(uint32_t v)
{
    return v > 65535u ? (uint16_t)65535u : (uint16_t)v;
}

int ae_prop_otz3(const uint16_t sz[3], uint16_t *out)
{
    uint32_t sum;
    if (!sz || !out)
        return 0;
    sum = (uint32_t)sz[0] + sz[1] + sz[2];
    *out = otz_saturate((AE_PROP_GTE_ZSF3 * sum) >> 12);
    return 1;
}

int ae_prop_otz4(const uint16_t sz[4], uint16_t *out)
{
    uint32_t sum;
    if (!sz || !out)
        return 0;
    sum = (uint32_t)sz[0] + sz[1] + sz[2] + sz[3];
    *out = otz_saturate((AE_PROP_GTE_ZSF4 * sum) >> 12);
    return 1;
}

int ae_prop_dpcs(const uint8_t source[4], const uint8_t far[3], uint16_t factor, uint8_t out[4])
{
    uint8_t result[4];
    uint32_t inv;
    int i;
    if (!source || !far || !out || factor > AE_PROP_GTE_DPCS_ONE)
        return 0;
    inv = AE_PROP_GTE_DPCS_ONE - factor;
    for (i = 0; i < 3; i++)
        result[i] = (uint8_t)(((uint32_t)source[i] * inv + (uint32_t)far[i] * factor) >> 12);
    result[3] = source[3];
    memcpy(out, result, sizeof result);
    return 1;
}
