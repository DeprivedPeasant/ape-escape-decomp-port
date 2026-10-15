/* Restricted GTE AVSZ3/AVSZ4 and DPCS helpers for the original ordinary-prop
 * fog/depth path (see native_prop_gte.md). Not a GTE emulator: no flags, MAC
 * registers, FIFOs, projection or colour matrices. C11, standard headers only. */
#ifndef AE_NATIVE_PROP_GTE_H
#define AE_NATIVE_PROP_GTE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_PROP_GTE_ZSF3 341u   /* GteInitProjection ZSF3 */
#define AE_PROP_GTE_ZSF4 256u   /* GteInitProjection ZSF4 */
#define AE_PROP_GTE_DPCS_ONE 4096u /* DPCS factor range is 0..4096 */

/* AVSZ3: OTZ = (ZSF3 * (SZ1+SZ2+SZ3)) >> 12, saturated to 0..65535. sz[0..2]
 * are SZ1..SZ3. Returns 1 and writes *out; returns 0 with *out unchanged if
 * sz or out is NULL. Inputs are already-transformed SZ values, not world Z. */
int ae_prop_otz3(const uint16_t sz[3], uint16_t *out);

/* AVSZ4: OTZ = (ZSF4 * (SZ0+SZ1+SZ2+SZ3)) >> 12, saturated to 0..65535.
 * Same return/NULL contract as ae_prop_otz3. */
int ae_prop_otz4(const uint16_t sz[4], uint16_t *out);

/* DPCS with sf=1, lm=0, far colour set by SetFarColor (FC = far << 4), factor
 * 0..4096 in IR0. Per channel: out = (source*(4096-factor) + far*factor) >> 12.
 * source[3] (CODE) is copied unchanged to out[3]. out may alias source, or a far buffer with at least four writable bytes.
 * Returns 1 on success; returns 0 with out untouched if any pointer is NULL or
 * factor > 4096. */
int ae_prop_dpcs(const uint8_t source[4], const uint8_t far[3], uint16_t factor, uint8_t out[4]);

#ifdef __cplusplus
}
#endif

#endif
