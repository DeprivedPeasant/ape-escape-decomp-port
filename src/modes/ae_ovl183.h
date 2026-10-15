#ifndef AE_OVL183_H
#define AE_OVL183_H

#include "../core/ae_pad.h"
#include "../core/ae_ram.h"
#include "../sound/ae_sound.h"
#include "../world/ae_progress.h"
#include "../world/ae_stagefade.h"
#include "../world/ae_zako2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_OVL183_BASE 0x80136000u
#define AE_OVL183_SIZE 0x1000u

#define AE_OVL183_G_FRAME 0x800EC350u
#define AE_OVL183_G_PACKET_FLAGS (AE_OVL183_G_FRAME + 0x40A0u)
#define AE_OVL183_G_RENDER_FLAGS (AE_OVL183_G_FRAME + 0x28u)
#define AE_OVL183_G_OT_INDEX (AE_OVL183_G_FRAME + 0x0Cu)
#define AE_OVL183_G_DRAW_ENV (AE_OVL183_G_FRAME + 0x10u)
#define AE_OVL183_G_DISPLAY_ENV (AE_OVL183_G_FRAME + 0x6Cu)
#define AE_OVL183_G_ORDER_TABLES (AE_OVL183_G_FRAME + 0x80u)
#define AE_OVL183_FRAME_STRIDE 0x4078u
#define AE_OVL183_OT_END 0x3FFCu
#define AE_OVL183_G_STAGE_STATE 0x800F4350u
#define AE_OVL183_STAGE_MODE 0x0120u

#define AE_OVL183_G_SOUND_SELECTION 0x80136B40u
#define AE_OVL183_G_COMMON_SELECTION 0x80136B44u
#define AE_OVL183_G_SIGHT_MUTE 0x80136B48u
#define AE_OVL183_G_PAGE 0x80136B4Cu
#define AE_OVL183_G_MENU_SELECTION 0x80136B50u
#define AE_OVL183_SOUND_RECORDS 0x80136B54u
#define AE_OVL183_G_ARCHIVE_ENTRY 0x80136FB4u
#define AE_OVL183_MENU_LABELS 0x80136FC4u
#define AE_OVL183_G_COMMON_LABEL 0x80136FD0u
#define AE_OVL183_G_COMMON_VOICE 0x80136FD8u
#define AE_OVL183_G_SIGHT_LABEL 0x80136FDCu
#define AE_OVL183_G_SIGHT_VOICE 0x80136FE4u
#define AE_OVL183_SOUND_RECORD_STRIDE 0x1Au

#define AE_OVL183_PAD_STATE 0x800E5320u
#define AE_OVL183_PAD_REPEAT 0x0168u
#define AE_OVL183_PAD_HELD 0x0158u
#define AE_OVL183_PAD_CHANGED 0x0160u

typedef int32_t (*AeOvl183NamedHook)(void *user, AeRam *r,
                                    const char *name, uint32_t address,
                                    uint32_t argc, const uint32_t *argv);

typedef struct AeOvl183Hooks {
    void *user;
    uint32_t stack_pointer;
    const AePadHooks *pad;
    const AeSoundHw *sound_hw;
    const AeProgressHooks *progress;
    const AeStageFadeHooks *stage_fade;
    const AeZako2Hooks *text;
    AeOvl183NamedHook native_call;
    AeOvl183NamedHook renderer;
} AeOvl183Hooks;

int32_t ae_ovl183_entry_80136000(AeRam *r,
                                 const AeOvl183Hooks *h); /* 80136000 */
int32_t ae_SoundTest_Update(AeRam *r,
                            const AeOvl183Hooks *h); /* 801361BC */
int32_t ae_SoundTest_DrawMainMenu(AeRam *r,
                                  const AeOvl183Hooks *h); /* 80136244 */
int32_t ae_SoundTest_DrawCommonTest(AeRam *r,
                                    const AeOvl183Hooks *h); /* 80136698 */
int32_t ae_SoundTest_DrawSightTest(AeRam *r,
                                   const AeOvl183Hooks *h); /* 8013684C */

#ifdef __cplusplus
}
#endif
#endif
