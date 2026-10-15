#ifndef AE_OVL184_H
#define AE_OVL184_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"
#include "../sound/ae_sound.h"
#include "../world/ae_stagefade.h"
#include "../world/ae_zako2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_OVL184_BASE 0x80136000u
#define AE_OVL184_SIZE 0x0800u
#define AE_OVL184_G_PAGE_STATE 0x801363E0u
#define AE_OVL184_G_FRAME_COUNTER 0x800EC350u
#define AE_OVL184_G_OT_INDEX (AE_OVL184_G_FRAME_COUNTER + 0x0Cu)
#define AE_OVL184_G_DISP_ENV (AE_OVL184_G_FRAME_COUNTER + 0x6Cu)
#define AE_OVL184_G_DRAW_ENV (AE_OVL184_G_FRAME_COUNTER + 0x10u)
#define AE_OVL184_G_ORDER_TABLES (AE_OVL184_G_FRAME_COUNTER + 0x80u)
#define AE_OVL184_FRAME_STRIDE 0x4078u
#define AE_OVL184_OT_END_OFFSET 0x3FFCu
#define AE_OVL184_STATE_FLAGS (AE_OVL184_G_FRAME_COUNTER + 0x28u)
#define AE_OVL184_PACKET_FLAGS (AE_OVL184_G_FRAME_COUNTER + 0x40A0u)
#define AE_OVL184_G_STAGE_STATE 0x800F4350u
#define AE_OVL184_STAGE_STATE_MODE 0x0120u
#define AE_OVL184_STAGE_STATE_ACTIVE_AREA 0x0126u
#define AE_OVL184_CLEAR_RECT_SECOND 0x04u
#define AE_OVL184_LOOP_RECT_OFFSET 0x10u

#define AE_OVL184_PAD_STATE 0x800E5320u
#define AE_OVL184_PAD_HELD 0x0158u
#define AE_OVL184_PAD_CHANGED 0x0160u
#define AE_OVL184_BUTTON_START 0x0800u

typedef int32_t (*AeOvl184NamedHook)(void *user, AeRam *r,
                                    const char *name, uint32_t address,
                                    uint32_t argc, const uint32_t *argv);

typedef struct AeOvl184Hooks {
    void *user;
    uint32_t stack_pointer;
    const AePadHooks *pad;
    const AeSoundHw *sound_hw;
    const AeStageFadeHooks *stage_fade;
    const AeZako2Hooks *text;
    AeOvl184NamedHook native_call;
    AeOvl184NamedHook renderer;
} AeOvl184Hooks;

int32_t ae_ovl184_entry_80136000(AeRam *r,
                                 const AeOvl184Hooks *h); /* 80136000 */
int32_t ae_Ovl184_LoadListLoop(AeRam *r,
                               const AeOvl184Hooks *h); /* 80136038 */
int32_t ae_Ovl184_DemoLoop(AeRam *r, const AeOvl184Hooks *h,
                           int32_t prompt_index); /* 801361D0 */
int32_t ae_Ovl184_DrawDemoPrompt(AeRam *r, const AeOvl184Hooks *h,
                                 int32_t prompt_index); /* 80136350 */

#ifdef __cplusplus
}
#endif
#endif
