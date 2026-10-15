#ifndef AE_OVL186A_H
#define AE_OVL186A_H

#include "ae_overlay_hooks.h"

/* Overlay member 186.bin, loaded at 0x80136000. */
#define AE_OVL186A_BASE 0x80136000u
#define AE_OVL186A_SIZE 0xF800u

/* Save menu data and lookup tables in member 186. */
#define AE_OVL186A_G_DIGIT_TABLE 0x8013B018u
#define AE_OVL186A_G_TEXT_WIDTHS 0x8013B024u
#define AE_OVL186A_G_WINDOW_PIECES 0x8014A990u
#define AE_OVL186A_G_BORDER_PIECES 0x8014A9A8u
#define AE_OVL186A_G_STATE_TABLE 0x8013AB20u
#define AE_OVL186A_G_MENU_STATE 0x8013AF56u
#define AE_OVL186A_G_SLOT_SELECTION 0x8013AF52u
#define AE_OVL186A_G_MENU_MODE 0x8013AF50u
#define AE_OVL186A_G_MENU_FADE_STATE 0x8013AF66u
#define AE_OVL186A_G_MENU_FADE_TIMER 0x8013AF68u
#define AE_OVL186A_G_MENU_SELECTION 0x8013AF6Au
#define AE_OVL186A_G_FILE_LIST_FLAGS 0x8013AF6Cu
#define AE_OVL186A_G_FILE_LIST_COUNT 0x8013AF70u
#define AE_OVL186A_G_FILE_LIST_CURSOR 0x8013AF74u
#define AE_OVL186A_G_MENU_WORK 0x8013AEF0u
#define AE_OVL186A_G_MODE_ARGUMENT 0x8013AF6Au
#define AE_OVL186A_G_MODE_FLAGS 0x8013AF54u
#define AE_OVL186A_G_MODE_SUBSTATE 0x8013AF60u
#define AE_OVL186A_G_MODE_TIMER 0x8013AF62u
#define AE_OVL186A_G_MENU_ACTIVE 0x8013AF5Au
#define AE_OVL186A_G_MENU_CARD_FLAG 0x8013AF64u
#define AE_OVL186A_G_MENU_SCREEN 0x8013AF78u
#define AE_OVL186A_G_MENU_SCENE 0x8013B210u
#define AE_OVL186A_G_MENU_MESH_LIST 0x8013B214u
#define AE_OVL186A_G_MENU_COLOR_BUFFER 0x80141554u
#define AE_OVL186A_G_MENU_TEXT_BUFFER 0x80141800u
#define AE_OVL186A_G_MODE_DISPATCH 0x8013A9D8u
#define AE_OVL186A_G_STATE_SETUP_DISPATCH 0x8013AA68u
#define AE_OVL186A_G_CARD_STATE_DISPATCH 0x8013ABD8u
#define AE_OVL186A_G_CARD_RESULT_DISPATCH 0x8013AC90u
#define AE_OVL186A_G_FILE_ENTRIES 0x8013B218u
#define AE_OVL186A_G_SAVE_SLOT_RECORDS 0x800E1880u
#define AE_OVL186A_G_SAVE_IMAGES 0x800E1A00u
#define AE_OVL186A_G_CURRENT_MODE 0x800F4470u

/* Shared PS1 engine state touched by the save menu. */
#define AE_OVL186A_G_FRAME_COUNTER 0x800EC350u
#define AE_OVL186A_G_PRIMITIVE_CURSOR (AE_OVL186A_G_FRAME_COUNTER + 4u)
#define AE_OVL186A_G_OT_INDEX (AE_OVL186A_G_FRAME_COUNTER + 12u)
#define AE_OVL186A_G_VIEW_MATRIX 0x800E78C0u
#define AE_OVL186A_G_MEMCARD_STATE 0x800E3880u
#define AE_OVL186A_G_SAVE_SLOT_IMAGES 0x800E1A00u
#define AE_OVL186A_G_LEVEL_TABLE 0x80102400u

/* Memory-card manager state words at 0x800E3880. */
enum AeOvl186aCardStateField {
    AE_OVL186A_CARD_RESULT = 0x00,
    AE_OVL186A_CARD_SLOT = 0x02,
    AE_OVL186A_CARD_BUSY = 0x04,
    AE_OVL186A_CARD_PROGRESS = 0x06,
    AE_OVL186A_CARD_RESULT_FLAGS = 0x08
};

/* The two menu runners share this word-sized state block. */
enum AeOvl186aMenuWorkField {
    AE_OVL186A_WORK_CURSOR_X = 0x00,
    AE_OVL186A_WORK_CURSOR_Y = 0x02,
    AE_OVL186A_WORK_SLOT_COUNT = 0x04,
    AE_OVL186A_WORK_FILE_FLAGS = 0x08,
    AE_OVL186A_WORK_SELECTED_SLOT = 0x0C,
    AE_OVL186A_WORK_SELECTED_ACTION = 0x0E,
    AE_OVL186A_WORK_FILE_NAME = 0x10,
    AE_OVL186A_WORK_SIZE = 0x40
};

int32_t ae_SaveUi_DrawText(AeRam *ram, const AeOverlayHooks *hooks,
                           uint32_t text, int32_t x, int32_t y,
                           int32_t texture_page_row);
int32_t ae_SaveUi_TextWidth(AeRam *ram, uint32_t text);
void ae_SaveUi_LookAt(AeRam *ram, uint32_t eye, uint32_t target);
uint32_t ae_SaveUi_StringTableIndex(AeRam *ram, uint32_t strings,
                                    int32_t index);
int32_t ae_Mode_SaveMenu(AeRam *ram, const AeOverlayHooks *hooks,
                         uint32_t caller_a0);
void ae_SaveUi_FrameReset(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_SceneInit(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_RunMenuA(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_RunMenuB(AeRam *ram, const AeOverlayHooks *hooks);
void ae_SaveUi_FileSelectInit(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_TotalMonkeyCount(AeRam *ram);
int32_t ae_SaveUi_MenuUpdate(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_RunMenuC(AeRam *ram, const AeOverlayHooks *hooks,
                           int32_t argument);
int32_t ae_SaveUi_CountEntries(AeRam *ram, uint32_t list, uint32_t flags);
int32_t ae_SaveUi_SetState(AeRam *ram, const AeOverlayHooks *hooks,
                           int32_t state, int32_t argument);
int32_t ae_SaveUi_FormatNumber3(AeRam *ram, int32_t value,
                                uint32_t output);
int32_t ae_SaveUi_MemCardStart(AeRam *ram, const AeOverlayHooks *hooks,
                               int32_t operation);
int32_t ae_SaveUi_StateTick(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_StateMachine(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_0(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_1(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_2(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_3(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_4(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_5(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_OnCardResult_6(AeRam *ram, const AeOverlayHooks *hooks);
void ae_SaveUi_DrawFileSelect(AeRam *ram, const AeOverlayHooks *hooks);
void ae_SaveUi_DrawSlot(AeRam *ram, const AeOverlayHooks *hooks,
                        int32_t mode, int32_t slot);
void ae_SaveUi_CardStateThread(AeRam *ram, const AeOverlayHooks *hooks);
void ae_SaveUi_CardResultThread(AeRam *ram, const AeOverlayHooks *hooks);

#endif
