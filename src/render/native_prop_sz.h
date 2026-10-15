/* Restricted GTE RTPS depth (SZ3) helper, integer only (see native_prop_sz.md).
 * Pure helper: no game data, no matrix composition, no projection, no flags/IR/MAC state.
 * C11, no dependencies beyond the standard headers. */
#ifndef AE_NATIVE_PROP_SZ_H
#define AE_NATIVE_PROP_SZ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_PROP_SZ_MAX 65535u  /* SZ is unsigned 16-bit; larger depths saturate here */

/* GTE RTPS (sf=1) depth for one vertex:
 *   MAC3 = signed44(translation_z * 4096 + row[0]*v[0] + row[1]*v[1] + row[2]*v[2])
 *   SZ3  = clamp(MAC3 >> 12, 0, 65535)
 * depth_row is the third row of the GTE rotation matrix (signed 16), translation_z is TRZ
 * (signed 32), vertex is VX/VY/VZ (signed 16). Returns 1 and stores SZ3 in *out on success.
 * Returns 0 with *out unchanged if any pointer is NULL. All inputs are read and the result is
 * computed before *out is written, so out may alias depth_row or vertex storage. */
int ae_prop_sz(const int16_t depth_row[3], int32_t translation_z, const int16_t vertex[3],
               uint16_t *out);

#ifdef __cplusplus
}
#endif

#endif
