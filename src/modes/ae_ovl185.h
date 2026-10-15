#ifndef AE_OVL185_H
#define AE_OVL185_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"
#include "../sound/ae_sound.h"
#include "../world/ae_misc.h"
#include "../world/ae_stage.h"
#include "../world/ae_zako2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_OVL185_BASE 0x80146000u
#define AE_OVL185_SIZE 0x1000u
#define AE_OVL185_G_MODE (AE_OVL185_BASE + 0x0770u)
#define AE_OVL185_G_SCENE (AE_OVL185_BASE + 0x0774u)
#define AE_OVL185_G_SCENE_LIST (AE_OVL185_BASE + 0x0878u)
#define AE_OVL185_G_DRAW_STRINGS (AE_OVL185_BASE + 0x0908u)
#define AE_OVL185_G_DRAW_ROWS (AE_OVL185_BASE + 0x0910u)
#define AE_OVL185_G_CLEAR_RECT (AE_OVL185_BASE + 0x0760u)
#define AE_OVL185_G_FRAME_CURSOR (AE_OVL185_G_FRAME_COUNTER + 0x04u)
#define AE_OVL185_G_OT_BUFFER (AE_OVL185_G_FRAME_COUNTER + 0x4080u)
#define AE_OVL185_G_DRAW_ENV_SECOND (AE_OVL185_G_FRAME_COUNTER + 0x4088u)
#define AE_OVL185_G_SCENE_VARIANTS (AE_OVL185_BASE + 0x06778u)
#define AE_OVL185_G_FRAME_COUNTER 0x800EC350u
#define AE_OVL185_G_OT_INDEX (AE_OVL185_G_FRAME_COUNTER + 0x0Cu)
#define AE_OVL185_G_DISP_ENV (AE_OVL185_G_FRAME_COUNTER + 0x6Cu)
#define AE_OVL185_G_DRAW_ENV (AE_OVL185_G_FRAME_COUNTER + 0x10u)
#define AE_OVL185_G_ORDER_TABLES (AE_OVL185_G_FRAME_COUNTER + 0x80u)
#define AE_OVL185_FRAME_STRIDE 0x4078u
#define AE_OVL185_OT_END_OFFSET 0x3FFCu
#define AE_OVL185_STATE_FLAGS (AE_OVL185_G_FRAME_COUNTER + 0x28u)
#define AE_OVL185_PACKET_FLAGS (AE_OVL185_G_FRAME_COUNTER + 0x40A0u)
#define AE_OVL185_PAD_STATE 0x800E5320u
#define AE_OVL185_PAD_HELD 0x0158u
#define AE_OVL185_PAD_CHANGED 0x0160u
#define AE_OVL185_PAD_CURRENT 0x0168u
#define AE_OVL185_START_BITS 0x0860u
#define AE_OVL185_BUTTON_LEFT 0x1000u
#define AE_OVL185_BUTTON_RIGHT 0x4000u
#define AE_OVL185_SCENE_COUNT 12u
#define AE_OVL185_SCENE_RECORD_STRIDE 12u
#define AE_OVL185_SCENE_RECORD_AREA 0x00u
#define AE_OVL185_SCENE_RECORD_ENTRY 0x04u
#define AE_OVL185_SCENE_RECORD_ARCHIVE 0x08u
#define AE_OVL185_ENV_RED_OFFSET 0x19u
#define AE_OVL185_ENV_GREEN_OFFSET 0x1Au
#define AE_OVL185_ENV_BLUE_OFFSET 0x1Bu
#define AE_OVL185_BUTTON_CANCEL 0x0100u
#define AE_OVL185_ENV_BLUE 127u
#define AE_OVL185_SELECTION_PACKET_BYTES 36u
#define AE_OVL185_DRAW_TEXT 0x8014666Cu
#define AE_OVL185_DRAW_SELECTION 0x80146358u

typedef int32_t (*AeOvl185NamedHook)(void *user, AeRam *r,
                                    const char *name, uint32_t address,
                                    uint32_t argc, const uint32_t *argv);

typedef struct AeOvl185Hooks {
    void *user;
    uint32_t stack_pointer;
    const AePadHooks *pad;
    const AeSoundHw *sound_hw;
    const AeZako2Hooks *text;
    const AeStageHooks *stage;
    const AeMiscHooks *misc;
    AeOvl185NamedHook native_call;
    AeOvl185NamedHook renderer;
} AeOvl185Hooks;

int32_t ae_ovl185_entry_80146000(AeRam *r,
                                 const AeOvl185Hooks *h); /* 80146000 */
int32_t ae_Ovl185_Run(AeRam *r, const AeOvl185Hooks *h); /* 8014607C */
int32_t ae_Ovl185_Update(AeRam *r, const AeOvl185Hooks *h); /* 80146230 */
int32_t ae_Ovl185_LoadScene(AeRam *r,
                            const AeOvl185Hooks *h); /* 80146500 */

/* Ovl185_DrawText @8014666C is native glyph/GPU packet work and stays a named renderer hook. */

#ifdef __cplusplus
}
#endif
#endif
