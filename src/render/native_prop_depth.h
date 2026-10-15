/* Original ordinary-prop F4 depth-acceptance prefix, integer only
 * (see native_prop_depth.md). Pure helper: no game data, no GTE, no projection,
 * no colour. C11, no dependencies beyond the standard headers. */
#ifndef AE_NATIVE_PROP_DEPTH_H
#define AE_NATIVE_PROP_DEPTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_PROP_F4_DEPTH_MIN 27          /* depth < 27 is rejected */
#define AE_PROP_F4_NEAR_WINDOW 257       /* unless flags bit0: reject if depth - near >= 257 */
#define AE_PROP_F4_DEPTH_LIMIT 4032      /* depth >= 4032 is stored as 4031 */
#define AE_PROP_F4_DEPTH_CLAMPED 4031
#define AE_PROP_F4_FLAG_NO_NEAR_TEST 1u  /* flags bit0: bypass the near-window test */

/* EmitPolyF4 prefix 800193F0..80019448, after AVSZ4/stOTZ and before stored NCLIP-result decisions.
 * Only bit0 of flags is used; other bits have no meaning here.
 * Returns 1 and sets *out_depth on accept: otz, or 4031 when otz >= 4032.
 * Returns 0 (original branch to 800197B8) when otz < 27, or when bit0 is clear
 * and (int32)otz - (int32)near >= 257. Returns 0 with *out_depth unchanged if
 * out_depth is NULL. This is not whole-primitive visibility. */
int ae_prop_f4_depth(uint16_t otz, uint16_t near, uint8_t flags, uint16_t *out_depth);

#ifdef __cplusplus
}
#endif

#endif
