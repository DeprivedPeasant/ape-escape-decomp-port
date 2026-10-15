/* Original ordinary-prop F4 fog factor and area fog configuration, integer only
 * (see native_prop_fog.md). Pure helpers: no game data, no GTE, no projection,
 * no colour blend. C11, no dependencies beyond the standard headers. */
#ifndef AE_NATIVE_PROP_FOG_H
#define AE_NATIVE_PROP_FOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_PROP_FOG_COEFFICIENT 0xF000u  /* AreaLoad normal path: gLevelGrid.fogParam */
#define AE_PROP_FOG_DEFAULT_NEAR 0x4B0u  /* fogStart when the header's fog flag is 0 */
#define AE_PROP_FOG_HEADER_MIN 0x30u     /* bytes of area header the config reads */
#define AE_PROP_FOG_ONE 4096             /* factor range is 0..4096 */

typedef struct AePropFog {
    uint8_t rgb[3];          /* header +20..+22, far colour (SetFarColor input) */
    uint16_t coefficient;    /* scratch 0x3C0: low16 of fogParam (0xF000) */
    uint16_t near_distance;  /* scratch 0x3C2: low16 of fogStart (effective near) */
    uint32_t draw_distance;  /* fogStart * 4 + 0x400 (source width / draw distance, not far) */
} AePropFog;

/* Original F4 factor (8001950C..80019564 and the two unclipped alternates):
 * delta = depth - near (modulo 2^32); product = low 32 bits of signed delta *
 * unsigned coefficient; factor = product >> 12 (arithmetic); clamped to 0..4096.
 * depth is the original OTZ value (GTE AVSZ4), not world Z. Fully defined for
 * every input; no saturation or 64-bit widening before the shift. */
int32_t ae_prop_fog_factor(int32_t depth, uint16_t near, uint16_t coefficient);

/* Parses little-endian area header fields: rgb +20..+22, s16 enabled +28 (only
 * zero / nonzero matters), u16 near +2C when enabled else 0x4B0. The far u16
 * +2E is not read. Returns 1 and fills *out on success. Returns 0 with *out
 * unchanged if area_header or out is NULL or header_size < 0x30. */
int ae_prop_fog_config(const uint8_t *area_header, size_t header_size, AePropFog *out);

#ifdef __cplusplus
}
#endif

#endif
