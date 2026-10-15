#ifndef AE_OVL189_H
#define AE_OVL189_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"
#include "../effects/ae_effects.h"
#include "../player/ae_animdraw.h"
#include "../player/ae_player_spawn.h"
#include "../render/ae_vram_anim.h"
#include "../ui/ae_hud.h"
#include "../world/ae_areaset.h"
#include "../world/ae_items_init.h"
#include "../world/ae_misc.h"
#include "../world/ae_misc_batch.h"
#include "../world/ae_progress.h"
#include "../world/ae_stagefade.h"
#include "../world/ae_zako2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The caller owns CD/archive, GPU, display and unported EXE services. The
 * callback arguments are the original MIPS argument words in call order. */
typedef int32_t (*AeOvl189CallHook)(void *user, AeRam *ram,
                                    const char *name, uint32_t address,
                                    uint32_t count, const uint32_t *args);
typedef int32_t (*AeOvl189RenderHook)(void *user, AeRam *ram,
                                      const char *name, uint32_t count,
                                      const uint32_t *args);

typedef struct AeOvl189Hooks {
    void *user;
    AeOvl189CallHook call;
    AeOvl189RenderHook render;
    const AeAreaSetHooks *area_set;
    const AeItemsInitHooks *items_init;
    const AeVramAnimHooks *vram_anim;
    const AeProgressHooks *progress;
    const AeMiscHooks *misc;
    const AePadHooks *pad;
    const AeEffectsHooks *effects;
    const AeHudHooks *hud;
    const AeZako2Hooks *debug_text;
    const AeStageFadeHooks *stage_fade;
    const AeSoundHw *sound_hw;
} AeOvl189Hooks;

/* Overlay member 189.bin is loaded at this address. */
#define AE_OVL189_BASE                         0x80136000u
#define AE_OVL189_SIZE                         0x4000u

/* Destination-screen state and packed menu tables. */
#define AE_OVL189_DESTINATION_TABLE            0x80139914u
#define AE_OVL189_DESTINATION_NAME_INDEX       0x80139978u
#define AE_OVL189_DESTINATION_PROGRESS_IDS     0x801399A8u
#define AE_OVL189_SPECIAL_PROGRESS_IDS         0x801399D4u
#define AE_OVL189_DESTINATION_AREA_INFO        0x80138A20u
#define AE_OVL189_DESTINATION_NAME_DATA        0x801399E8u
#define AE_OVL189_STAGE_NAME_DATA              0x80139B20u
#define AE_OVL189_EXIT_ROUTE_STRIDE             9u
#define AE_OVL189_EXIT_ROUTE_SIZE               2u
#define AE_OVL189_AREA_INFO_STRIDE              0x15Cu
#define AE_OVL189_AREA_INFO_LEVEL_ID            0x12Cu
#define AE_OVL189_DESTINATION_COUNT             11u
#define AE_OVL189_SPECIAL_DESTINATION_COUNT     9u

#define AE_OVL189_G_SCROLL_POSITION             0x80139BC0u
#define AE_OVL189_G_SELECTION                    0x80139BC4u
#define AE_OVL189_G_ROUTE_SELECTION              0x80139BC8u
#define AE_OVL189_G_DESTINATION_SLOT             0x80139BCCu
#define AE_OVL189_G_CAMERA_ANGLE                 0x80139BD0u
#define AE_OVL189_G_CAMERA_DISTANCE              0x80139BD4u
#define AE_OVL189_G_TRANSITION_STATE             0x80139BD8u
#define AE_OVL189_G_SCREEN_STATE                 0x80139BDCu
#define AE_OVL189_G_ANIMATION_FRAME              0x80139BE0u
#define AE_OVL189_G_TEXT_ROW                     0x80139BE4u
#define AE_OVL189_G_ROUTE_SCAN_INDEX              0x80139BE8u
#define AE_OVL189_G_TEXT_DRAW_STATE               0x80139CF0u
#define AE_OVL189_G_SELECTED_LEVEL               0x80139BC8u

/* Main-EXE globals read or updated by this member. */
#define AE_OVL189_G_AREA_DATA_COUNT              0x80154000u
#define AE_OVL189_G_AREA_DATA_RECORDS            0x80154004u
#define AE_OVL189_G_ORDERING_TABLE_ROOT          0x800EC3D0u
#define AE_OVL189_G_ORDER_TABLE_CURSOR           0x800EC354u
#define AE_OVL189_G_FRAME_INDEX                   0x800EC350u
#define AE_OVL189_G_RENDER_CONTEXT_INDEX          0x800EC35Cu
#define AE_OVL189_G_UI_ANIMATION_COUNTER          0x800AE8F0u
#define AE_OVL189_G_PAD_HELD                      0x800E5478u
#define AE_OVL189_G_PAD_CHANGED                   0x800E5480u
#define AE_OVL189_G_PAD_REPEAT                    0x800E5488u
#define AE_OVL189_G_CURRENT_AREA                  0x800F4476u
#define AE_OVL189_G_STAGE_CONTEXT                 0x800F4350u
#define AE_OVL189_STAGE_CONTEXT_UI_FLAGS           0x02BAu
#define AE_OVL189_G_FRAME_COUNTER                 0x800EC350u
#define AE_OVL189_G_SAVE_DESTINATION               0x800E072Cu
#define AE_OVL189_G_SAVE_DESTINATION_SOURCE        0x800F461Cu
#define AE_OVL189_G_SAVE_DESTINATION_ROUTE         0x800F461Du
#define AE_OVL189_G_LEVEL_STATE                    0x800F4470u

/* Camera and view matrices. */
#define AE_OVL189_G_BASE_MATRIX                   0x800AF0A0u
#define AE_OVL189_G_VIEW_MATRIX                   0x800E78C0u
#define AE_OVL189_G_COLOR_MATRIX                  0x800E7D00u
#define AE_OVL189_TEMP_VECTOR                     0x801FFDA0u
#define AE_OVL189_TEMP_NORMALIZED                 0x801FFDB0u
#define AE_OVL189_TEMP_VIEW_ANGLES                0x801FFDC0u
#define AE_OVL189_TEMP_LOOK_AT_EYE                 0x801FFD40u
#define AE_OVL189_TEMP_LOOK_AT_TARGET              0x801FFD48u
#define AE_OVL189_TEMP_VIEW_ROTATION              0x801FFDD0u
#define AE_OVL189_TEMP_VIEW_TRANSLATION            0x801FFDF0u
#define AE_OVL189_TEMP_INIT_CLEAR_RECT             0x801FFDE0u
#define AE_OVL189_TEMP_ROUTE_POSITION              0x801FFD60u
#define AE_OVL189_TEMP_ROUTE_ROTATION              0x801FFD70u

/* Graphics data owned by the native renderer. */
#define AE_OVL189_GRAPHICS_STATE                  0x1F8003C4u
#define AE_OVL189_GRAPHICS_FLAG                   0x1F8003F4u
#define AE_OVL189_RENDER_PACKET_BASE              0x800EC350u
#define AE_OVL189_RENDER_PACKET_TABLE             0x800EC3D0u
#define AE_OVL189_RENDER_PACKET_SIZE              0x18u
#define AE_OVL189_MODEL_TRANSFORM_SIZE             0x20u
#define AE_OVL189_TEMP_MODEL_MATRIX                0x801FFE00u

int32_t ae_HubExitUi_Init(AeRam *ram, const AeOvl189Hooks *hooks,
                          uint32_t display_argument);
int32_t ae_Mode_LeaveHub(AeRam *ram, const AeOvl189Hooks *hooks,
                         uint32_t display_argument);
int32_t ae_HubExitUi_Run(AeRam *ram, const AeOvl189Hooks *hooks);
int32_t ae_HubExitUi_IsDestinationUnlocked(AeRam *ram, const AeOvl189Hooks *hooks,
                                           int32_t destination, int32_t route);
int32_t ae_HubExitUi_GetProgress(AeRam *ram, const AeOvl189Hooks *hooks,
                                 int32_t destination, int32_t route);
int32_t ae_HubExitUi_GetFlags(AeRam *ram, const AeOvl189Hooks *hooks,
                              int32_t destination, int32_t route);
void ae_HubExitUi_DrawModelA(AeRam *ram, const AeOvl189Hooks *hooks,
                             uint32_t position, uint32_t rotation,
                             uint32_t model, uint32_t model_flag);
void ae_HubExitUi_DrawModelB(AeRam *ram, const AeOvl189Hooks *hooks,
                             uint32_t position, uint32_t rotation,
                             uint32_t model, uint32_t model_flag);
void ae_HubExitUi_DrawModelC(AeRam *ram, const AeOvl189Hooks *hooks,
                             uint32_t position, uint32_t rotation,
                             uint32_t model);
void ae_SaveUi_LookAt_189(AeRam *ram, uint32_t eye, uint32_t target);
void ae_HubExitUi_DrawText(AeRam *ram, const AeOvl189Hooks *hooks,
                           uint32_t string);
uint32_t ae_SaveUi_StringTableIndex_189(AeRam *ram, uint32_t strings,
                                        int32_t index);
int32_t ae_HubExitUi_TextWidth(AeRam *ram, uint32_t string);

#ifdef __cplusplus
}
#endif
#endif
