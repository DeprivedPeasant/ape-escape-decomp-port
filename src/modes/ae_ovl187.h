#ifndef AE_OVL187_H
#define AE_OVL187_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"
#include "../core/ae_pose.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/ae_player1.h"
#include "../sound/ae_sound.h"
#include "../ui/ae_hud.h"
#include "../world/ae_areaset.h"
#include "../world/ae_items_init.h"
#include "../world/ae_misc.h"
#include "../world/ae_stage.h"
#include "../world/ae_stagefade.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The overlay uses these original EXE routines outside the native port. A
 * call keeps its original name, address, and o32 argument words. Render hooks
 * receive the values computed by the game logic; they own GPU packet work. */
typedef int32_t (*AeOvl187CallFn)(void *user, AeRam *ram, const char *name,
                                  uint32_t address, uint32_t argc,
                                  const uint32_t *argv);

typedef struct AeOvl187Hooks {
    void *user;
    uint32_t stack_pointer;
    AeOvl187CallFn call;
    const AePadHooks *pad;
    const AeAreaSetHooks *area;
    const AeItemsInitHooks *items;
    const AeMiscHooks *misc;
    const AeStageHooks *stage;
    const AeStageFadeHooks *stage_fade;
    const AePlayer1Hooks *player1;
    const AeHudHooks *hud;
    const AeSoundHw *sound;
} AeOvl187Hooks;

uint32_t ae_StageClearUi_LookAt(AeRam *r, const AeOvl187Hooks *h,
                                uint32_t eye, uint32_t target); /* 80136000 */
uint32_t ae_StageClearUi_StringTableIndex(AeRam *r, uint32_t strings,
                                          int32_t index); /* 801361D8 */
int32_t ae_Mode_StageClear(AeRam *r, const AeOvl187Hooks *h); /* 80136208 */
int32_t ae_StageClearUi_DrawWipe(AeRam *r, const AeOvl187Hooks *h); /* 801368FC */
void ae_StageClearUi_DrawText(AeRam *r, const AeOvl187Hooks *h,
                              uint32_t string); /* 801372D0 */
void ae_StageClearUi_DrawMessage(AeRam *r, const AeOvl187Hooks *h,
                                 uint32_t string); /* 801373F8 */
void ae_StageClearUi_RunScript(AeRam *r, const AeOvl187Hooks *h); /* 801374C8 */
void ae_StageClearUi_SetDrawStp(AeRam *r, const AeOvl187Hooks *h); /* 80137CA8 */
void ae_StageClearUi_CopyStrip(AeRam *r, const AeOvl187Hooks *h,
                               int32_t index); /* 80137D20 */
void ae_StageClearUi_DrawBlinkIcons(AeRam *r, const AeOvl187Hooks *h); /* 80137DC4 */
void ae_StageClearUi_UpdateObjects(AeRam *r, const AeOvl187Hooks *h,
                                   uint32_t objects); /* 80137F4C */
void ae_StageClearUi_DrawBox(AeRam *r, const AeOvl187Hooks *h,
                             int32_t x, int32_t y, int32_t width,
                             int32_t height); /* 80138258 */
void ae_StageClearUi_DrawBackdrop(AeRam *r, const AeOvl187Hooks *h); /* 8013836C */
void ae_StageClearUi_DrawTimes(AeRam *r, const AeOvl187Hooks *h); /* 801384A0 */
int32_t ae_StageClearUi_PlayXa(AeRam *r, const AeOvl187Hooks *h); /* 80138660 */
void ae_StageClearUi_DrawScanlines(AeRam *r, const AeOvl187Hooks *h); /* 801386B8 */
void ae_StageClearUi_CheckRequirements(AeRam *r,
                                       const AeOvl187Hooks *h); /* 80138A9C */
uint32_t ae_RippleGrid_Init(AeRam *r, const AeOvl187Hooks *h); /* 80138B0C */
void ae_RippleGrid_Smooth(AeRam *r); /* 80138C24 */
void ae_RippleGrid_Draw(AeRam *r, const AeOvl187Hooks *h); /* 80138E20 */
void ae_StageClearUi_Setup(AeRam *r, const AeOvl187Hooks *h); /* 80138F38 */
uint32_t ae_StageClearUi_ObjMatrix(AeRam *r, const AeOvl187Hooks *h,
                                   int32_t index, uint32_t pose); /* 801392B0 */
void ae_StageClearUi_DrawStars(AeRam *r, const AeOvl187Hooks *h); /* 8013939C */

#ifdef __cplusplus
}
#endif
#endif
