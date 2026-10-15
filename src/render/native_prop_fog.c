#include "native_prop_fog.h"
#include <string.h>

#include <stddef.h>

int32_t ae_prop_fog_factor(int32_t depth, uint16_t near, uint16_t coefficient)
{
    /* subu: modulo 2^32. MULT of signed delta by a non-negative coefficient has
     * the same low 32 bits as the unsigned modular product (MFLO). */
    uint32_t delta = (uint32_t)depth - (uint32_t)near;
    uint32_t low = delta * (uint32_t)coefficient;
    /* sra 12 on the low word: a set sign bit means the shifted value is negative,
     * which the original clamps to 0. Otherwise the logical shift is exact. */
    if ((low & 0x80000000u) != 0u)
        return 0;
    uint32_t f = low >> 12;
    return f > (uint32_t)AE_PROP_FOG_ONE ? AE_PROP_FOG_ONE : (int32_t)f;
}

int ae_prop_fog_config(const uint8_t *area_header, size_t header_size, AePropFog *out)
{
    if (area_header == NULL || out == NULL || header_size < AE_PROP_FOG_HEADER_MIN)
        return 0;
    AePropFog cfg;
    memset(&cfg, 0, sizeof(cfg));
    int enabled = (area_header[0x28] | area_header[0x29]) != 0; /* s16: nonzero only */
    cfg.rgb[0] = area_header[0x20];
    cfg.rgb[1] = area_header[0x21];
    cfg.rgb[2] = area_header[0x22];
    cfg.coefficient = (uint16_t)AE_PROP_FOG_COEFFICIENT;
    cfg.near_distance = enabled ? (uint16_t)(area_header[0x2C] | ((unsigned)area_header[0x2D] << 8))
                                : (uint16_t)AE_PROP_FOG_DEFAULT_NEAR;
    cfg.draw_distance = (uint32_t)cfg.near_distance * 4u + 0x400u;
    memcpy(out, &cfg, sizeof(cfg));
    return 1;
}
