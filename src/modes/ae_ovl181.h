#ifndef AE_OVL181_H
#define AE_OVL181_H

#include "ae_overlay_hooks.h"

/* Overlay member 181.bin, loaded at 0x80136000. */
int32_t ae_Mode_Boot(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Boot_ClearScreen(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Boot_SetupDisplay(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Boot_DrawLogoA(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Boot_DrawLogoB(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Mode_02(AeRam *ram, const AeOverlayHooks *hooks);

/* Member 181's CD integrity check and warning display state. */
int32_t ae_Ovl181_CheckCdHardwareIntegrity(AeRam *ram,
                                           const AeOverlayHooks *hooks,
                                           uint32_t arg0, uint32_t arg1,
                                           uint32_t arg2);
int32_t ae_Ovl181_DrawIntegrityWarning(AeRam *ram,
                                       const AeOverlayHooks *hooks);

#endif
