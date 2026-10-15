#ifndef AE_OVL186B_H
#define AE_OVL186B_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_OVL186B_BASE 0x80136000u
#define AE_OVL186B_SCOPE_END 0x8013CD1Cu

/* Save-menu globals in the overlay's writable data block. */
#define AE_OVL186B_G_SAVE_UI_STATE       0x8013AF56u
#define AE_OVL186B_G_SAVE_UI_SELECTION   0x8013AF52u
#define AE_OVL186B_G_SAVE_UI_CONFIRM_MODE 0x8013AF54u
#define AE_OVL186B_G_SAVE_UI_YES_NO      0x8013AF5Au
#define AE_OVL186B_G_SAVE_UI_NAME_LENGTH 0x8013AF50u
#define AE_OVL186B_G_SAVE_UI_CHOICE      0x8013AF5Eu
#define AE_OVL186B_G_SAVE_UI_FADE_STATE  0x8013AF66u
#define AE_OVL186B_G_SAVE_UI_FADE_TIMER  0x8013AF68u
#define AE_OVL186B_G_SAVE_UI_ANGLE        0x8013AF70u
#define AE_OVL186B_G_SAVE_UI_MODEL_INDEX  0x8013AF74u
#define AE_OVL186B_G_SAVE_UI_MODEL_ANGLE  0x8013AF78u
#define AE_OVL186B_G_SAVE_UI_MODEL_DRAW_FIRST 0x8013AF6Cu
#define AE_OVL186B_G_CARDMGR_STATE        0x8013F280u
#define AE_OVL186B_G_CARDMGR_FLAGS        0x8013F284u
#define AE_OVL186B_G_CARDMGR_TIMER         0x8013F288u

#define AE_OVL186B_G_PAD_STATE            0x800E5320u
#define AE_OVL186B_G_PAD_HELD              (AE_OVL186B_G_PAD_STATE + 0x158u)
#define AE_OVL186B_G_PAD_CHANGED           (AE_OVL186B_G_PAD_STATE + 0x160u)
#define AE_OVL186B_G_PAD_REPEAT            (AE_OVL186B_G_PAD_STATE + 0x168u)
#define AE_OVL186B_G_DISPLAY_STATE         0x800EC350u
#define AE_OVL186B_G_PACKET_CURSOR         (AE_OVL186B_G_DISPLAY_STATE + 4u)
#define AE_OVL186B_G_DISPLAY_INDEX         (AE_OVL186B_G_DISPLAY_STATE + 0x0Cu)
#define AE_OVL186B_G_VIEW_MODEL            0x8014B214u
#define AE_OVL186B_G_MENU_MODELS           0x8014B214u
#define AE_OVL186B_MENU_MODEL_STRIDE       0x20u
#define AE_OVL186B_G_SCREEN_VERTICES       0x800E7D00u
#define AE_OVL186B_G_MENU_STATE_BLOCK      0x800F4470u
#define AE_OVL186B_MENU_STATE_RENDER       21u
#define AE_OVL186B_G_SAVE_UI_ROTATION      0x8013AF70u
#define AE_OVL186B_G_SAVE_UI_MODEL_STEP    0x8013AF74u
#define AE_OVL186B_G_MENU_MODEL_BASE_ROTATION 0x800F06A0u
#define AE_OVL186B_G_SAVE_SLOTS            0x800E1880u
#define AE_OVL186B_SAVE_SLOT_STRIDE        0x7A0u
#define AE_OVL186B_SAVE_ENTRY_USED         0x1A8u
#define AE_OVL186B_G_SAVE_UI_DATA          0x800E3880u
#define AE_OVL186B_G_CARDMGR_DISPLAY      0x800F4350u
#define AE_OVL186B_CARDMGR_DISPLAY_BUFFER2 0x13Cu
#define AE_OVL186B_CARDMGR_DISPLAY_FIELD3  0x154u
#define AE_OVL186B_CARDMGR_DISPLAY_FIELD4  0x156u
#define AE_OVL186B_CARDMGR_STATS_BASE      0x8013F5E4u
#define AE_OVL186B_CARDMGR_HEADER_BASE     0x800F5120u
#define AE_OVL186B_CARDMGR_HEADER_STATUS   0xA1u
#define AE_OVL186B_CARDMGR_HEADER_FLAGS    0xA2u
#define AE_OVL186B_CARDMGR_DISPLAY_ACTIVE  0x40A0u
#define AE_OVL186B_CARDMGR_OT_ACTIVE      0x28u
#define AE_OVL186B_CARDMGR_OT_CLEAR_START 0x29u
#define AE_OVL186B_CARDMGR_OT_CLEAR_END   0x2Bu
#define AE_OVL186B_CARDMGR_OT_TAGS        0x40A1u
#define AE_OVL186B_CARDMGR_OT_TAGS_END    0x40A3u
#define AE_OVL186B_DISPLAY_RECORD_STRIDE  16504u
#define AE_OVL186B_DISPLAY_ENV_OFFSET     0x6Cu
#define AE_OVL186B_DISPLAY_DRAWENV_OFFSET 0x10u
#define AE_OVL186B_DISPLAY_OTAG_OFFSET    0x407Cu
#define AE_OVL186B_DISPLAY_MESH_OFFSET    0x407Cu
#define AE_OVL186B_DISPLAY_ENV_TEMPLATE  0x8014EEE0u
#define AE_OVL186B_SAVE_SLOT_NAME         0x198u
#define AE_OVL186B_SAVE_NAME_LENGTH       16u
#define AE_OVL186B_SAVE_DEFAULT_CHAR_BIAS 0x6Bu
#define AE_OVL186B_SAVE_NAME_CHAR_TABLE   0x8013C550u
#define AE_OVL186B_SAVE_DATA_SLOT          0x00u
#define AE_OVL186B_SAVE_DATA_CHOICE         0x06u
#define AE_OVL186B_G_SAVE_FONT_TABLE       0x8013B024u
#define AE_OVL186B_G_SAVE_ICON_TABLE       0x8013B124u
#define AE_OVL186B_G_SAVE_PORTRAIT_TABLE   0x8013AFACu
#define AE_OVL186B_G_SAVE_NAME_BUFFER      0x8013C5C0u
#define AE_OVL186B_G_SAVE_NAME_GRID        0x8013C540u
#define AE_OVL186B_G_SAVE_NAME_TIMERS      0x8013C5D0u
#define AE_OVL186B_G_SAVE_NAME_CURSOR_X    0x8013C5A8u
#define AE_OVL186B_G_SAVE_NAME_CURSOR_Y    0x8013C5AAu
#define AE_OVL186B_G_SAVE_NAME_PAGE        0x8013C5ACu
#define AE_OVL186B_G_SAVE_NAME_FIELD_5AE   0x8013C5AEu
#define AE_OVL186B_G_SAVE_NAME_FIELD_5B0   0x8013C5B0u
#define AE_OVL186B_G_SAVE_NAME_FIELD_5B2   0x8013C5B2u
#define AE_OVL186B_G_SAVE_NAME_LENGTH      0x8013C5B4u
#define AE_OVL186B_G_SAVE_UI_SELECTED_SLOT 0x800E3886u
#define AE_OVL186B_G_SAVE_UI_CARD_AVAILABLE 0x8013AF64u
#define AE_OVL186B_G_FILE_SELECT_OPTIONS   0x8013AFE8u
#define AE_OVL186B_FILE_SELECT_OPTION_COUNT 5u
#define AE_OVL186B_SAVE_ENTRY_TYPE         0x180u
#define AE_OVL186B_CARDMGR_RESULT_FIELD    0x800F4470u

typedef int32_t (*AeOvl186bCallFn)(void *user, AeRam *ram,
                                   const char *name, uint32_t address,
                                   uint32_t argc, const uint32_t *argv);

typedef struct AeOvl186bHooks {
    void *user;
    uint32_t stack_pointer;
    uint32_t caller_a1;
    uint32_t caller_a2;
    uint32_t caller_s0;
    const AePadHooks *pad;
    AeOvl186bCallFn overlay_call;
    AeOvl186bCallFn native_call;
    AeOvl186bCallFn renderer;
} AeOvl186bHooks;

/* Native drawing and card I/O remain renderer/platform hooks. */
void ae_SaveUi_DrawDetail(AeRam *r, const AeOvl186bHooks *h); /* 801391B0 */
int32_t ae_SaveUi_LastUsedEntry(AeRam *r, int32_t slot); /* 801399E4 */
int32_t ae_SaveUi_DrawMainWindow(AeRam *r, const AeOvl186bHooks *h); /* 80139A3C */
void ae_SaveUi_DrawBackground3D(AeRam *r, const AeOvl186bHooks *h); /* 80139A68 */
int32_t ae_SaveUi_YesNoUpdate(AeRam *r, const AeOvl186bHooks *h); /* 80139F08 */
int32_t ae_SaveUi_FileSelectUpdate(AeRam *r, const AeOvl186bHooks *h); /* 80139FC8 */
int32_t ae_SaveUi_ConfirmPressed(AeRam *r); /* 8013A400 */
int32_t ae_SaveUi_CancelPressed(AeRam *r); /* 8013A43C */
int32_t ae_SaveUi_FadeTick(AeRam *r, const AeOvl186bHooks *h); /* 8013A634 */
int32_t ae_SaveUi_OnCardResult_7(AeRam *r, const AeOvl186bHooks *h); /* 8013B400 */
int32_t ae_SaveUi_ConfirmDialogUpdate(AeRam *r, const AeOvl186bHooks *h); /* 8013B47C */
int32_t ae_SaveUi_NameEntryInit(AeRam *r, const AeOvl186bHooks *h); /* 8013B550 */
int32_t ae_SaveUi_NameEntryUpdate(AeRam *r, const AeOvl186bHooks *h); /* 8013B6C4 */
void ae_SaveUi_NameEntryDraw(AeRam *r, const AeOvl186bHooks *h); /* 8013BC64 */
int32_t ae_CardMgr_Run(AeRam *r, const AeOvl186bHooks *h); /* 8013C7F0 */
int32_t ae_CardMgr_Init(AeRam *r, const AeOvl186bHooks *h); /* 8013C8E8 */
int32_t ae_CardMgr_RunFileList(AeRam *r, const AeOvl186bHooks *h); /* 8013C954 */
int32_t ae_CardMgr_RunFileDetail(AeRam *r, const AeOvl186bHooks *h); /* 8013CB30 */

#ifdef __cplusplus
}
#endif
#endif
