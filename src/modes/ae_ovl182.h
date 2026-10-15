#ifndef AE_OVL182_H
#define AE_OVL182_H

#include "ae_overlay_hooks.h"

/* Overlay member 182.bin, loaded at 0x80136000. */
int32_t ae_Mode_03(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_DrawScene3D(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_UpdateCamera(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_Init(AeRam *ram, const AeOverlayHooks *hooks, uint32_t descriptor);
int32_t ae_Ovl182_StateMachine(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_DrawIntroState(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_MenuInput(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_FadeTick(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_Ovl182_DrawFade(AeRam *ram, const AeOverlayHooks *hooks);

#endif
