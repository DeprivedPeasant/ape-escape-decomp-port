#ifndef AE_BODYMOVE_H
#define AE_BODYMOVE_H

#include "ae_bodies.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t ae_ColliderSlopeContactDir(AeRam *r, const AeBodyMoveHooks *h,
                                   int32_t type, uint32_t collider,
                                   uint32_t point, uint32_t out); /* 8004F884 */
void ae_PlayerTryLedgeGrab(AeRam *r, const AeBodyMoveHooks *h); /* 80063EC4 */
int32_t ae_PlayerVehicleSoundUpdate(AeRam *r, const AeBodyHooks *h,
                                    uint32_t body); /* 80072A14 */
int32_t ae_PlayerMoveRumbleSoundUpdate(AeRam *r, const AeBodyHooks *h,
                                       uint32_t body); /* 80072BE8 */
int32_t ae_CollisionStub_ReturnZero(AeRam *r, uint32_t body); /* 80072DC4 */
int32_t ae_BodyLandOnGround(AeRam *r, const AeBodyMoveHooks *h,
                            uint32_t body, int32_t type,
                            int32_t ceiling); /* 80072DCC */
int32_t ae_CollisionResolveProbes(AeRam *r, const AeBodyMoveHooks *h,
                                  int32_t type, uint32_t body); /* 80073450 */
int32_t ae_SignedRangeOverlap(int32_t a, int32_t b,
                              int32_t c, int32_t d); /* 800739FC */
int32_t ae_BodyRecordColliderContact(AeRam *r, const AeBodyMoveHooks *h,
                                     uint32_t body,
                                     int32_t type); /* 800792C8 */
int32_t ae_SoundEmitterPan(AeRam *r, const AeBodyMoveHooks *h,
                           int32_t sound_id, int32_t voice,
                           int32_t range, uint32_t position); /* 80087B34 */
int32_t ae_SoundEmitterPitch(AeRam *r, const AeBodyMoveHooks *h,
                             int32_t sound_id, int32_t voice,
                             int32_t pitch); /* 80087C28 */
int32_t ae_SoundLoopStop(AeRam *r, const AeSoundHw *sound_hw,
                         int32_t id, int32_t voice); /* 8008806C */

#ifdef __cplusplus
}
#endif
#endif
