#ifndef AE_POSE_H
#define AE_POSE_H

#include "ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Actor frame decoding and model bone posing, translated from the original instructions. */
uint32_t ae_ActorAnimDecode(AeRam *r, uint32_t actor); /* 80014E34 */
uint32_t ae_AnimDecodeLerp(AeRam *r, uint32_t actor, uint32_t frame_a, uint32_t frame_b, uint32_t fraction); /* 80014FA0 */
void ae_ModelBuildBoneMatrices(AeRam *r, uint32_t actor, uint32_t flags); /* 80015504 */
void ae_ModelPoseBones(AeRam *r, uint32_t actor, uint32_t flags); /* 80015780 */

#ifdef __cplusplus
}
#endif
#endif
