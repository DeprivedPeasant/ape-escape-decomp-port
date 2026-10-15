#ifndef AE_OVL18A_H
#define AE_OVL18A_H

#include "../core/ae_pad.h"
#include "../core/ae_ram.h"
#include "../player/ae_player1.h"
#include "../world/ae_areaset.h"
#include "../world/ae_items_init.h"
#include "../world/ae_progress.h"
#include "../world/ae_stagefade.h"
#include "../world/ae_zako2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_OVL18A_BASE 0x80136000u
#define AE_OVL18A_SIZE 0x1800u
#define AE_OVL18A_G_FRAME 0x800EC350u
#define AE_OVL18A_G_OT_INDEX (AE_OVL18A_G_FRAME + 0x0Cu)
#define AE_OVL18A_G_FRAME_CURSOR (AE_OVL18A_G_FRAME + 0x04u)
#define AE_OVL18A_G_DRAW_ENV (AE_OVL18A_G_FRAME + 0x10u)
#define AE_OVL18A_G_DISPLAY_ENV (AE_OVL18A_G_FRAME + 0x6Cu)
#define AE_OVL18A_G_ORDER_TABLES (AE_OVL18A_G_FRAME + 0x80u)
#define AE_OVL18A_FRAME_STRIDE 0x4078u
#define AE_OVL18A_OT_END 0x3FFCu
#define AE_OVL18A_G_STAGE_STATE 0x800F4350u
#define AE_OVL18A_STAGE_MODE 0x0120u
#define AE_OVL18A_G_AREA_BASE 0x80154000u
#define AE_OVL18A_G_TIMEOUT 0x80136E30u
#define AE_OVL18A_G_CLIP_TABLE 0x80136E4Cu
#define AE_OVL18A_G_XA_FILE 0x80136E34u
#define AE_OVL18A_G_XA_OFFSET 0x80136E3Cu
#define AE_OVL18A_G_ACTOR 0x80136F00u
#define AE_OVL18A_G_ACTOR_SECOND (AE_OVL18A_G_ACTOR + 0x88u)
#define AE_OVL18A_G_SCENE_CONFIG 0x80137010u
#define AE_OVL18A_G_SCENE_INDEX 0x80137058u
#define AE_OVL18A_G_XA_INDEX 0x8013705Cu
#define AE_OVL18A_G_FRAME_COUNTER 0x800EC350u
#define AE_OVL18A_G_RENDER_MATRIX 0x800E78C0u
#define AE_OVL18A_G_BASE_MATRIX 0x800AF0A0u
#define AE_OVL18A_G_MODEL_BUFFER 0x800B4D64u
#define AE_OVL18A_G_MODEL_BUFFER_COUNT 0x800F45D0u
#define AE_OVL18A_G_LEVEL_GRID 0x800EBFA0u
#define AE_OVL18A_G_SYSTEM_STATE 0x800E3890u
#define AE_OVL18A_LEVEL_GRID_FOG_PARAMETER 0x24u
#define AE_OVL18A_LEVEL_GRID_FOG_START 0x28u
#define AE_OVL18A_LEVEL_GRID_FOG_END 0x2Cu
#define AE_OVL18A_PAD_STATE 0x800E5320u
#define AE_OVL18A_PAD_HELD 0x0158u
#define AE_OVL18A_PAD_CHANGED 0x0160u
#define AE_OVL18A_MESH_INSTANCE_STRIDE 0x20u
#define AE_OVL18A_ACTOR_MODEL 0x08u
#define AE_OVL18A_ACTOR_ANIMATION_DATA 0x18u
#define AE_OVL18A_ADVANCE_DIRECTION_OFFSET 0x10u

typedef int32_t (*AeOvl18aNamedHook)(void *user, AeRam *r,
                                    const char *name, uint32_t address,
                                    uint32_t argc, const uint32_t *argv);

typedef struct AeOvl18aHooks {
    void *user;
    uint32_t stack_pointer;
    const AePadHooks *pad;
    const AeSoundHw *sound_hw;
    const AePlayer1Hooks *player1;
    const AeAreaSetHooks *area_set;
    const AeItemsInitHooks *items;
    const AeProgressHooks *progress;
    const AeStageFadeHooks *stage_fade;
    const AeZako2Hooks *text;
    AeOvl18aNamedHook native_call;
    AeOvl18aNamedHook renderer;
} AeOvl18aHooks;

void ae_StageClearUi_LookAt_18A(AeRam *r, const AeOvl18aHooks *h,
                                uint32_t eye, uint32_t target); /* 80136000 */
uint32_t ae_Ovl18A_StringTableIndex(AeRam *r, uint32_t strings,
                                    int32_t index); /* 801361D8 */
int32_t ae_ovl18A_entry_80136208(AeRam *r,
                                 const AeOvl18aHooks *h); /* 80136208 */
int32_t ae_Ovl18A_WaitInputTimeout(AeRam *r); /* 80136724 */
int32_t ae_Ovl18A_SceneInit(AeRam *r,
                            const AeOvl18aHooks *h); /* 80136780 */
void ae_Ovl18A_UpdateSpin(AeRam *r,
                          const AeOvl18aHooks *h); /* 801368CC */
void ae_Ovl18A_AdvanceAnim(AeRam *r, const AeOvl18aHooks *h); /* 80136AA0 */
void ae_Ovl18A_StartClip(AeRam *r); /* 80136C04 */
int32_t ae_Ovl18A_PlayXa(AeRam *r, const AeOvl18aHooks *h); /* 80136C6C */
void ae_Ovl18A_PickScene(AeRam *r); /* 80136C94 */

#ifdef __cplusplus
}
#endif
#endif
