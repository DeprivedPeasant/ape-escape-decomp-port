/* Restricted GTE RTPS depth (SZ3) helper; see native_prop_sz.md. */
#include "native_prop_sz.h"

#include <stddef.h>

#define MAC44_MASK ((UINT64_C(1) << 44) - 1)
#define MAC44_SIGN (UINT64_C(1) << 43)

int ae_prop_sz(const int16_t depth_row[3], int32_t translation_z, const int16_t vertex[3],
               uint16_t *out)
{
    if (depth_row == NULL || vertex == NULL || out == NULL)
        return 0;

    /* |each term| <= 2^43 and 4 terms: the sum fits int64 with room to spare. */
    int64_t sum = (int64_t)translation_z * 4096 + (int64_t)depth_row[0] * vertex[0] +
                  (int64_t)depth_row[1] * vertex[1] + (int64_t)depth_row[2] * vertex[2];

    /* Signed 44-bit wrap: int64 -> uint64 is defined modulo 2^64, so the low 44 bits are exact. */
    uint64_t word = (uint64_t)sum & MAC44_MASK;

    uint16_t sz;
    if (word & MAC44_SIGN) {
        sz = 0; /* negative MAC3: floor(MAC3 / 4096) is negative, SZ clamps to 0 */
    } else {
        uint64_t depth = word >> 12;
        sz = depth > AE_PROP_SZ_MAX ? (uint16_t)AE_PROP_SZ_MAX : (uint16_t)depth;
    }

    *out = sz;
    return 1;
}
