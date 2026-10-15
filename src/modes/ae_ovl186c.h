#ifndef AE_OVL186C_H
#define AE_OVL186C_H

#include "ae_overlay_hooks.h"

/* Overlay member 186.bin, loaded at 0x80136000. */

/* Card manager state in the EXE and member BSS. */
#define AE_OVL186_CARD_MODE                 0x800E3884u
#define AE_OVL186_CARD_CONTEXT               0x800E388Cu
#define AE_OVL186_CARD_OPERATION             0x800E3880u
#define AE_OVL186_CARD_BUFFER_POINTER        0x800E3870u
#define AE_OVL186_CARD_RESULT                0x800E1990u
#define AE_OVL186_SCREEN_FADE_STATE          0x800E38B0u
#define AE_OVL186_UI_STATE                   0x8013AF56u
#define AE_OVL186_UI_STATE_FLAG              0x8013AF5Au
#define AE_OVL186_MENU_FADE_STATE            0x8013AF66u
#define AE_OVL186_UI_BUSY                    0x8013F284u
#define AE_OVL186_UI_OPERATION               0x8013F288u
#define AE_OVL186_UI_TIMEOUT                 0x8013F294u
#define AE_OVL186_UI_REPEAT_COUNT            0x8013F28Cu
#define AE_OVL186_UI_POLL_TIMER              0x8013F290u
#define AE_OVL186_UI_PRIMARY_VALUES          0x8013F5E4u
#define AE_OVL186_UI_SECONDARY_VALUES        0x8013F5FCu
#define AE_OVL186_UI_ADJUSTED_VALUES         0x8013F614u
#define AE_OVL186_UI_OPERATION_VALUES        0x8013F5FCu
#define AE_OVL186_UI_CARD_FRAMES             0x8013F298u
#define AE_OVL186_UI_CARD_DIRECTIONS         0x8013F29Cu
#define AE_OVL186_UI_ANIMATION_RECORDS       0x8013F55Cu
#define AE_OVL186_UI_FILE_ROWS               0x8013F454u
#define AE_OVL186_UI_RENDER_METADATA         0x8013F594u
#define AE_OVL186_UI_THREAD_STATE            0x80141B8Cu
#define AE_OVL186_UI_THREAD_HANDLE           0x80141B88u
#define AE_OVL186_UI_THREAD_ENTRY            0x80141B80u
#define AE_OVL186_UI_THREAD_ENTRY_CELL       0x80141B84u
#define AE_OVL186_UI_THREAD_TABLE            0x80141B70u
#define AE_OVL186_UI_MENU_CHOICE             0x8013FB20u
#define AE_OVL186_UI_MESSAGE_RESULT          0x8013F280u
#define AE_OVL186_UI_STATE_AUX               0x8013AF68u
#define AE_OVL186_MCX_CALLBACK_TABLE         0x8013F118u
#define AE_OVL186_UI_FRAME_NUMBER            0x800EC350u
#define AE_OVL186_UI_FRAME_CURSOR_OFFSET    0x04u
#define AE_OVL186_UI_FRAME_NUMBER_OFFSET    0x0Cu
#define AE_OVL186_TEXT_ENTRY_SIZE             36u
#define AE_OVL186_STATE_TEXT_RECORDS          0x80140470u
#define AE_OVL186_STATE_TEXT_DESCRIPTORS      0x80141940u
#define AE_OVL186_STATE_TEXT_RECORD_STRIDE    36u
#define AE_OVL186_STATE_TEXT_X_OFFSET         0u
#define AE_OVL186_STATE_TEXT_Y_OFFSET         2u
#define AE_OVL186_STATE_TEXT_STRING_OFFSET    4u
#define AE_OVL186_STATE_TEXT_DESCRIPTOR_SIZE  4u
#define AE_OVL186_STATE_OVERLAY_DISPATCH_TABLE 0x801403B0u
#define AE_OVL186_STATE_OVERLAY_DRAW_HANDLER  0x801401D4u
#define AE_OVL186_CARD_TEXT_BUFFER            0x800E1A18u
#define AE_OVL186_CARD_TEXT_VARIANT           0x800E3886u
#define AE_OVL186_STATE_TEXT_FIRST_STATE       2u
#define AE_OVL186_STATE_TEXT_LAST_STATE        49u
#define AE_OVL186_MENU_LABELS                0x8014147Cu
#define AE_OVL186_MENU_ITEM_LABELS           0x80141578u
#define AE_OVL186_MENU_ITEM_STATUS            0x80141704u
#define AE_OVL186_MENU_TEXT_LABELS           0x80141704u
#define AE_OVL186_MENU_TEXT_COORDINATES      0x80141700u
#define AE_OVL186_MENU_TEXT_STATUS            0x80141820u
#define AE_OVL186_MENU_TEXT_LABELS_2          0x80141824u
#define AE_OVL186_UI_COLOR_BUFFER             0x80141554u
#define AE_OVL186_UI_BUFFER_2                 0x80141800u
#define AE_OVL186_DISPLAY_ENV_SOURCE          0x8013FB10u
#define AE_OVL186_FRAME_RESET_BYTE            0x800F03F0u
#define AE_OVL186_FRAME_DRAW_BYTE             0x800EC378u
#define AE_OVL186_FRAME_SECONDARY_BYTE        0x800F03F1u
#define AE_OVL186_RUNMESSAGE_CLEAR_IMAGE_LOCAL 0x801FFED8u
#define AE_OVL186_MCX_INIT_REQUEST            0x801450CCu
#define AE_OVL186_MCX_INIT_RESULT             0x801450C8u
#define AE_OVL186_MCX_INIT_ARGUMENT           0x801450D0u
#define AE_OVL186_MCX_INIT_RETRY               0x801450D8u
#define AE_OVL186_MCX_INIT_MODE                0x80145270u
#define AE_OVL186_MCX_OBJECTS                  0x80142008u
#define AE_OVL186_MCX_THREAD_LIST              0x8014200Cu
#define AE_OVL186_MCX_CALLBACK_ROOT            0x80142010u
#define AE_OVL186_MCX_THREAD_CONFIG            0x80142014u
#define AE_OVL186_MCX_START_CONFIG             0x80142024u
#define AE_OVL186_MCX_NODE_HEAD_OFFSET         0x00u
#define AE_OVL186_MCX_NODE_FLAGS_OFFSET        0x04u
#define AE_OVL186_MCX_OBJECT_CONFIG_OFFSET     0x08u
#define AE_OVL186_MCX_OBJECT_READY_OFFSET      0x0Au
#define AE_OVL186_MCX_OBJECT_READY_DETAIL_OFFSET 0x0Eu
#define AE_OVL186_MCX_OBJECT_RECORD_SIZE       0x40u
#define AE_OVL186_MCX_THREAD_RECORD_SIZE       0xC0u
#define AE_OVL186_MCX_THREAD_RECORD_FLAGS      0x94u
#define AE_OVL186_THREAD_HOOK_CONTEXT          0x801EFFF0u

/* Card manager context fields. */
#define AE_OVL186_CONTEXT_SLOT_ARRAY_OFFSET  0x110u
#define AE_OVL186_CONTEXT_SLOT_COUNT         5u
#define AE_OVL186_CONTEXT_CARD_TYPE_OFFSET   0x128u
#define AE_OVL186_CONTEXT_WORD_SIZE          4u
#define AE_OVL186_CARD_OPERATION_STATUS      0x800E3888u
#define AE_OVL186_CARD_OPERATION_BUFFER      0x800E388Cu
#define AE_OVL186_CARD_SLOT_WORD_COUNT       6u
#define AE_OVL186_UI_PRIMARY_VALUE_COUNT     12u
#define AE_OVL186_UI_PRIMARY_LIMIT_OFFSET    0x0Cu
#define AE_OVL186_UI_DRAW_ANIM_COUNT         4u
#define AE_OVL186_UI_CALL_WORK               0x801FE000u
#define AE_OVL186_CALL_WORK_EVENT_OFFSET     0x00u
#define AE_OVL186_CALL_WORK_RESULT_OFFSET    0x04u
#define AE_OVL186_CALL_WORK_CARD_INFO_OFFSET 0x08u
#define AE_OVL186_MENU_PACKET_BASE_A          0x800F4350u
#define AE_OVL186_MENU_PACKET_A_PROGRESS_0    0x13Cu
#define AE_OVL186_MENU_PACKET_A_PROGRESS_1    0x154u
#define AE_OVL186_MENU_PACKET_A_PROGRESS_2    0x156u
#define AE_OVL186_MENU_PACKET_BASE_B          0x800F5120u
#define AE_OVL186_MENU_PACKET_B_PROGRESS_0    0x0A1u
#define AE_OVL186_MENU_PACKET_B_PROGRESS_1    0x0A2u

/* Card record layout, relative to the card-manager slot table. */
#define AE_OVL186_SLOT_RECORD_SIZE           0x10u
#define AE_OVL186_SLOT_STATUS_OFFSET         0x00u
#define AE_OVL186_SLOT_FILE_COUNT_OFFSET     0x02u
#define AE_OVL186_SLOT_CARD_TYPE_OFFSET      0x04u
#define AE_OVL186_SLOT_FRAME_OFFSET          0x08u
#define AE_OVL186_SLOT_MODE_OFFSET           0x0Au
#define AE_OVL186_SLOT_BYTES_OFFSET          0x0Cu

/* Four 14-byte animation records drive icon / highlight frame changes. */
#define AE_OVL186_ANIM_ICON_OFFSET           0x00u
#define AE_OVL186_ANIM_FRAME_COUNT_OFFSET    0x02u
#define AE_OVL186_ANIM_FRAME_INDEX_OFFSET    0x04u
#define AE_OVL186_ANIM_LIMIT_OFFSET          0x06u
#define AE_OVL186_ANIM_TIMER_OFFSET          0x08u
#define AE_OVL186_ANIM_TIMER_LIMIT_OFFSET    0x0Au
#define AE_OVL186_ANIM_FRAME_BASE_OFFSET     0x0Cu
#define AE_OVL186_ANIM_RECORD_SIZE           0x0Eu
#define AE_OVL186_ANIM_DRAW_FRAME_STRIDE     0x16u
#define AE_OVL186_ANIM_DRAW_FRAME_OFFSET     0x14u

/* MCX thread callback table. */
#define AE_OVL186_MCX_THREAD_ROOT            0x80142010u
#define AE_OVL186_MCX_CONFIG                 0x80142014u
#define AE_OVL186_MCX_THREAD_LINK_STRIDE     8u
#define AE_OVL186_MCX_LINK_NEXT_OFFSET       0u
#define AE_OVL186_MCX_LINK_DATA_OFFSET       4u

int32_t ae_CardMgr_SetState(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t state, uint32_t option);
int32_t ae_CardMgr_StartOps(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SsGetMute(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_IsBusy(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_StateMachine(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_0(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_1(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_2(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_3(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_4(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_5(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_6(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_7(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_8(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_OnCardResult_9(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_McxCardTypeSync(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_McxAllInfoSync(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_ResetSlots(AeRam *ram, const AeOverlayHooks *hooks,
                              uint32_t reset_type);
int32_t ae_McxHideTransSync(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t operation);
int32_t ae_CardMgr_Draw(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_MenuUpdate(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_AnimTick(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t ignored);
int32_t ae_CardMgr_RunMessage(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_DrawMessage(AeRam *ram, const AeOverlayHooks *hooks,
                               uint32_t ignored_context);
int32_t ae_CardMgr_DrawMenuLabels(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t menu);
int32_t ae_CardMgr_DrawBlinkingMenu(AeRam *ram, const AeOverlayHooks *hooks,
                                    uint32_t variant);
int32_t ae_SaveUi_DrawStateOverlay(AeRam *ram, const AeOverlayHooks *hooks);
uint32_t ae_SaveUi_GetColorBuffer(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_DrawMenuItems(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t selected, uint32_t enabled);
uint32_t ae_SaveUi_GetBuffer2(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_CardMgr_DrawMenuTexts(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t enabled);
int32_t ae_CardMgr_DrawMenuItems2(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t selected, uint32_t enabled);
int32_t ae_SaveUi_DrawStringPair(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_DrawYesNo(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t selected);
int32_t ae_SaveUi_DrawFileHeader(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t page);
int32_t ae_SaveUi_DrawEmptySlotText(AeRam *ram, const AeOverlayHooks *hooks,
                                    uint32_t slot);
int32_t ae_SaveUi_ThreadSetHook(AeRam *ram, const AeOverlayHooks *hooks,
                                uint32_t function);
int32_t ae_SaveUi_ThreadClose(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_Nop(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_ThreadSwitch(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_SaveUi_ThreadSetHookReset(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_McxStartCom(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_MCXINIT_OBJ_B0(AeRam *ram, const AeOverlayHooks *hooks,
                          uint32_t index, uint32_t callback);
int32_t ae_MCXINIT_OBJ_FC(AeRam *ram, const AeOverlayHooks *hooks,
                          uint32_t unused, uint32_t object);
int32_t ae_MCXINIT_OBJ_104(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_MCXINIT_OBJ_144(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_MCXINIT_OBJ_184(AeRam *ram, const AeOverlayHooks *hooks);
int32_t ae_MCXINIT_OBJ_2BC(AeRam *ram, const AeOverlayHooks *hooks);

#endif
