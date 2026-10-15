#ifndef AE_MISC_BATCH_H
#define AE_MISC_BATCH_H

#include "ae_misc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frame helpers added in the miscellaneous batch. */
void ae_PadRumbleReset(AeRam *r, int32_t port); /* 80014208 */
int32_t ae_GetZoneMask(AeRam *r, int32_t x, int32_t y, int32_t z,
                       int32_t fallback); /* 8004626C */
int32_t ae_CameraRcCarActive(AeRam *r); /* 8004A978 */
int32_t ae_StageExitPending(AeRam *r); /* 8004D104 */
int32_t ae_BossUpdate(AeRam *r, const AeMiscHooks *h,
                      uint32_t actor); /* 8005AC28 */
void ae_VramScrollUpdate(AeRam *r, const AeMiscHooks *h); /* 800691F8 */
int32_t ae_PlayerIsSubmerged(AeRam *r); /* 8006B9F8 */
int32_t ae_Indicator_Register(AeRam *r, const AeMiscHooks *h,
                           int32_t kind, uint32_t data,
                           int32_t angle); /* 800716EC */
void ae_MenuItemSetRange(AeRam *r, uint32_t item, uint32_t minimum,
                         uint32_t maximum); /* 8007D290 */
void ae_MenuItemSetStep(AeRam *r, uint32_t item,
                        uint32_t step); /* 8007D2A4 */
void ae_HudFindGadgetIconPos(AeRam *r); /* 8008197C */
void ae_ColorFadeReset(AeRam *r); /* 8008C390 */
int32_t ae_ProgressFlagGet(AeRam *r, uint32_t flag); /* 8009632C */
void ae_AreaFeature30Update(AeRam *r,
                            const AeMiscHooks *h); /* 8009B7FC */

/* Collider-script handlers reached through the stage motion table. */
int32_t ae_ColliderScriptFollowPath(AeRam *r, const AeMiscHooks *h,
                                    uint32_t collider,
                                    int32_t index); /* 800892C4, slot 2 */
int32_t ae_ColliderScriptPlayerMove(AeRam *r, const AeMiscHooks *h,
                                    uint32_t collider,
                                    int32_t index); /* 8008A4B0, slot 9 */
int32_t ae_ColliderScriptCarry(AeRam *r, const AeMiscHooks *h,
                               uint32_t collider,
                               int32_t index); /* 8008A91C, slot 10 */
int32_t ae_ColliderScriptCommand(AeRam *r, const AeMiscHooks *h,
                                 uint32_t collider, uint32_t command,
                                 int32_t distance,
                                 int32_t index); /* 8008B980 */
void ae_ColliderResetToStart(AeRam *r,
                             uint32_t collider); /* 8008BDB4 */

#ifdef __cplusplus
}
#endif
#endif
