#ifndef AE_PROGRESS_H
#define AE_PROGRESS_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AeSoundHw;
struct AeGimmickAHooks;
struct AeAreaSetHooks;

/* Calls outside this batch keep the original four argument registers.  The
 * callbacks cover a game sound routine, one overlay routine, and the PsyQ
 * SetDrawTPage entry point used by DrawTextSprites. */
typedef int32_t (*AeProgressHookFn)(void *, AeRam *, uint32_t, uint32_t,
                                    uint32_t, uint32_t);

typedef struct AeProgressHooks {
    void *user;
    const struct AeSoundHw *sound_hw;
    const struct AeGimmickAHooks *gimmick_a;
    const struct AeAreaSetHooks *area_set;
    AeProgressHookFn SoundLoopsSetPan; /* 80086788 */
    AeProgressHookFn FUN_8013FB20;     /* overlay entry */
    AeProgressHookFn SoundBankSetVolume; /* 800A2640 */
    AeProgressHookFn SetDrawTPage;     /* PsyQ SDK, 8002B410 */
} AeProgressHooks;

/* Save block and saved progress arrays. */
#define AE_PROGRESS_SAVE_BLOCK                 0x800E0018u
#define AE_PROGRESS_SAVE_BLOCK_RESUME_AREA     0x000u
#define AE_PROGRESS_SAVE_BLOCK_OPTIONS         0x014u
#define AE_PROGRESS_SAVE_BLOCK_AREA_RECORDS    0x068u
#define AE_PROGRESS_SAVE_BLOCK_PROGRESS_FLAGS  0x5E4u
#define AE_PROGRESS_SAVE_BLOCK_GATE_FLAGS      0x664u
#define AE_PROGRESS_SAVE_BLOCK_STAGE_RESULTS   0x668u
#define AE_PROGRESS_SAVE_BLOCK_PROGRESS_FLAGS2 0x66Cu
#define AE_PROGRESS_SAVE_BLOCK_TIME_RECORDS    0x690u
#define AE_PROGRESS_SAVE_BLOCK_UNKNOWN_68C     0x68Cu
#define AE_PROGRESS_SAVE_BLOCK_UNKNOWN_710     0x710u
#define AE_PROGRESS_SAVE_BLOCK_HUB_DEST_B      0x714u
#define AE_PROGRESS_SAVE_BLOCK_HUB_DEST_A      0x716u
#define AE_PROGRESS_SAVE_BLOCK_UNKNOWN_718     0x718u
#define AE_PROGRESS_SAVE_BLOCK_SLOT_INFO       0x168u
#define AE_PROGRESS_SAVE_SLOT_SIZE             0x084u
#define AE_PROGRESS_SAVE_BLOCK_LEVEL_PLAY_MODE 0x028u
#define AE_PROGRESS_SAVE_BLOCK_LEVEL_COLLECTED 0x048u
#define AE_PROGRESS_SAVE_BLOCK_GIMMICK_STATES  0x2F4u
#define AE_PROGRESS_SAVE_HEADER_LIVES          0x002u
#define AE_PROGRESS_SAVE_HEADER_GADGET_MASK    0x004u
#define AE_PROGRESS_SAVE_HEADER_GADGET_SLOTS   0x008u
#define AE_PROGRESS_SAVE_HEADER_AMMO           0x00Cu
#define AE_PROGRESS_SAVE_HEADER_GADGET_STATE   0x010u

#define AE_PROGRESS_G_SAVED_AREA_RECORDS       0x800DFB70u
#define AE_PROGRESS_G_LEVEL_PLAY_MODE           0x800DFC70u
#define AE_PROGRESS_G_TIME_ATTACK_LEVEL_COUNT  0x800DFC88u
#define AE_PROGRESS_G_LEVEL_COLLECTED           0x800DFC98u
#define AE_PROGRESS_G_TIME_RECORDS              0x800DFD40u
#define AE_PROGRESS_G_GIMMICK_STATES            0x800DF820u
#define AE_PROGRESS_G_FLAGS                     0x800DFCC0u
#define AE_PROGRESS_G_SPECIAL_GATE_FLAGS        0x800DFDCCu
#define AE_PROGRESS_G_STAGE_RESULT_FLAGS         0x800DFDD0u
#define AE_PROGRESS_G_STAGE_RESULT_WORDS         0x800DFDD8u
#define AE_PROGRESS_G_STAGE_RESULT_AUX_WORD      0x800DFDFCu
#define AE_PROGRESS_G_AREA_KIND_VISITED          0x800DFDF8u

/* Live fields consumed by SaveHeaderFill / RecordBestTime. */
#define AE_PROGRESS_G_GAME_MODE                 0x800F4470u
#define AE_PROGRESS_G_CURRENT_LEVEL             0x800F4474u
#define AE_PROGRESS_G_CURRENT_AREA              0x800F4476u
#define AE_PROGRESS_G_OPTION_FLAG_A             0x800F4488u
#define AE_PROGRESS_G_OPTION_FLAG_B             0x800F448Au
#define AE_PROGRESS_G_LIVES                     0x800F448Cu
#define AE_PROGRESS_G_PLAY_TIME                 0x800F4490u
#define AE_PROGRESS_G_PREVIOUS_PLAY_TIME        0x800F4494u
#define AE_PROGRESS_G_SAVE_HEADER_WORD_154      0x800F44A4u
#define AE_PROGRESS_G_SAVE_HEADER_BYTE_156      0x800F44A6u
#define AE_PROGRESS_G_SAVE_HEADER_WORD_158      0x800F44A8u
#define AE_PROGRESS_G_SAVE_HEADER_BYTE_168      0x800F44B8u
#define AE_PROGRESS_G_PICKUP_COUNTER            0x800F44BAu
#define AE_PROGRESS_G_SAVED_OPTION_BYTE         0x800EC2D2u
#define AE_PROGRESS_G_SLING_AMMO1               0x800F51C1u
#define AE_PROGRESS_G_SLING_AMMO2               0x800F51C2u
#define AE_PROGRESS_G_SFX_VOLUME                0x800BF680u
#define AE_PROGRESS_G_PAUSE_MENU_REQUEST        0x800BEF74u
#define AE_PROGRESS_G_PAUSE_MENU_CLEAR          0x800BEF70u
#define AE_PROGRESS_G_PAUSE_MENU_FLAG           0x800BEF75u
#define AE_PROGRESS_G_PAUSE_MENU_STATE          0x800BEF76u
#define AE_PROGRESS_G_PAUSE_MENU_VALUE           0x800BEF80u

/* Sound settings, the two pause-loop records, and the text-sprite queue. */
#define AE_PROGRESS_G_SOUND_BANK_SFX             0x800BF670u
#define AE_PROGRESS_G_SOUND_BANK_BGM             0x800BF674u
#define AE_PROGRESS_G_SOUND_BGM_VOICE            0x800BF676u
#define AE_PROGRESS_G_BGM_VOLUME                 0x800BF67Eu
#define AE_PROGRESS_G_SAVED_BGM_VOLUME           0x800BF682u
#define AE_PROGRESS_G_SAVED_SFX_VOLUME           0x800BF684u
#define AE_PROGRESS_G_GADGET_SLOTS               0x800F51A8u
#define AE_PROGRESS_G_GADGET_UNLOCK_MASK         0x800F51C4u
#define AE_PROGRESS_G_PAUSE_LOOP_A               0x800B20D0u
#define AE_PROGRESS_G_PAUSE_LOOP_B               0x800B20D4u
#define AE_PROGRESS_SOUND_LOOP_VOICE             0x002u
#define AE_PROGRESS_G_TEXT_SPRITE_CURSOR         0x800EC354u
#define AE_PROGRESS_G_TEXT_SPRITE_OT_INDEX       0x800EC35Cu
#define AE_PROGRESS_G_TEXT_SPRITE_PACKET_TABLE   0x800F03D0u
#define AE_PROGRESS_G_TEXT_SPRITE_ADVANCE_TABLE  0x800B2734u
#define AE_PROGRESS_TEXT_SPRITE_PACKET_SIZE      0x010u
#define AE_PROGRESS_TEXT_SPRITE_OT_STRIDE        0x4078u
#define AE_PROGRESS_TEXT_SPRITE_CODE             0x003u
#define AE_PROGRESS_TEXT_SPRITE_COLOR            0x007u
#define AE_PROGRESS_TEXT_SPRITE_U                0x00Cu
#define AE_PROGRESS_TEXT_SPRITE_V                0x00Du
#define AE_PROGRESS_TEXT_SPRITE_X                0x008u
#define AE_PROGRESS_TEXT_SPRITE_Y                0x00Au
#define AE_PROGRESS_TEXT_SPRITE_TPAGE            0x00Eu
#define AE_PROGRESS_TEXT_SPRITE_TAG              0x000u
#define AE_PROGRESS_TEXT_SPRITE_CODE_VALUE       0x03u
#define AE_PROGRESS_TEXT_SPRITE_COLOR_VALUE      0x7Du
#define AE_PROGRESS_TEXT_SPRITE_PAGE_BASE        0x01Bu
#define AE_PROGRESS_TEXT_SPRITE_PAGE_OFFSET      0x100u
#define AE_PROGRESS_TEXT_SPRITE_PAGE_SHIFT       6u
#define AE_PROGRESS_TEXT_SPRITE_DRAW_PAGE        54u
#define AE_PROGRESS_ANIM_TABLE_ENTRY_SIZE        4u
#define AE_PROGRESS_TEXT_SPRITE_SCAN_STEP         1u
#define AE_PROGRESS_SAVE_ARRAY_HALFWORD_SIZE     2u
#define AE_PROGRESS_SAVE_ARRAY_WORD_SIZE         4u
#define AE_PROGRESS_SAVE_RECORD_COUNT            128u
#define AE_PROGRESS_SAVE_LEVEL_COUNT             32u
#define AE_PROGRESS_GIMMICK_STATE_BYTE_COUNT     752u
#define AE_PROGRESS_SAVE_PROGRESS_WORD_COUNT     8u
#define AE_PROGRESS_HEADER_VOLUME_STACK          0x801FFEF0u
#define AE_PROGRESS_HEADER_SFX_LOCAL             AE_PROGRESS_HEADER_VOLUME_STACK
#define AE_PROGRESS_HEADER_BGM_LOCAL             (AE_PROGRESS_HEADER_VOLUME_STACK + 4u)
#define AE_PROGRESS_HEADER_VOLUME_WORD_SIZE      4u

#define AE_PROGRESS_MESH_ANIM_DATA               0x000u
#define AE_PROGRESS_MESH_ANIM_FRAME              0x018u
#define AE_PROGRESS_MESH_ANIM_FLAGS              0x01Au
#define AE_PROGRESS_ANIM_FRAME_COUNT              0x00Cu
#define AE_PROGRESS_SOUND_VOLUME_BGM_FIELD        0x00Eu
#define AE_PROGRESS_SOUND_VOLUME_SFX_FIELD        0x010u

/* Hub gate and special-area data tables. */
#define AE_PROGRESS_G_HUB_KIND_LEVELS            0x800B37F0u
#define AE_PROGRESS_G_HUB_EXIT_GATE_AREAS        0x800B389Cu
#define AE_PROGRESS_G_HUB_SPECIAL_PROGRESS_IDS   0x800B38C0u
#define AE_PROGRESS_G_HUB_EXIT_SOURCE_AREA       0x800B38BAu
#define AE_PROGRESS_G_HUB_EXIT_DEST_AREA         0x800B38BCu
#define AE_PROGRESS_SPECIAL_AREA_FIRST           0x05Cu
#define AE_PROGRESS_SPECIAL_AREA_COUNT           7u
#define AE_PROGRESS_HUB_TABLE_ENTRY_SIZE          2u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_0   0x004u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_1   0x008u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_2   0x010u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_3   0x020u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_4   0x040u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_5   0x080u
#define AE_PROGRESS_SPECIAL_AREA_GADGET_MASK_6   0x001u
#define AE_PROGRESS_HUB_STAGE_EXIT_CODE          0x800u
#define AE_PROGRESS_HUB_DESTINATION_READY        3

/* Stage-result masks and fields. */
#define AE_PROGRESS_STAGE_RESULT_SPECIAL_AREA    0x013
#define AE_PROGRESS_STAGE_RESULT_SPECIAL_BIT     0x010u
#define AE_PROGRESS_STAGE_RESULT_SPECIAL_CLEAR  0x008u
#define AE_PROGRESS_STAGE_RESULT_STANDARD_BIT    0x100u
#define AE_PROGRESS_STAGE_RESULT_STANDARD_EXTRA  0x1000u
#define AE_PROGRESS_STAGE_RESULT_STANDARD_CLEAR  0x800u
#define AE_PROGRESS_STAGE_RESULT_BONUS           5u
#define AE_PROGRESS_STAGE_RESULT_LEVELS          6u
#define AE_PROGRESS_STAGE_RESULT_TIME_ATTACK_FIRST 0x018u
#define AE_PROGRESS_TIME_ATTACK_LEVEL_FIRST       0x018u
#define AE_PROGRESS_STAGE_RESULT_CURRENT_TIME    0x140u
#define AE_PROGRESS_STAGE_RESULT_PREVIOUS_TIME   0x144u

uint32_t ae_SaveSlotInfoPtr(AeRam *r, uint32_t slot); /* 800119F4 */
void ae_MeshAnimAdvance(AeRam *r, uint32_t mesh_anim); /* 80016E68 */
void ae_SpriteEffectQueue0(AeRam *r, uint32_t spec); /* 8006D548 */
int32_t ae_SoundLoopsPause(AeRam *r, const AeProgressHooks *h); /* 8007292C */
void ae_ClearDAT_800BEF70(AeRam *r); /* 8007F400 */
void ae_OpenPauseMenu(AeRam *r, int32_t request); /* 8007F56C */
void ae_DrawTextSprites(AeRam *r, const AeProgressHooks *h, uint32_t text,
                        int32_t x, int32_t y, int32_t page); /* 80082288 */
int32_t ae_SoundSetVolumes(AeRam *r, const AeProgressHooks *h,
                           int32_t sfx_volume, int32_t bgm_volume); /* 800862E4 */
int32_t ae_SoundVolumesDefault(AeRam *r, const AeProgressHooks *h,
                               int32_t apply_to_hardware); /* 80086428 */
int32_t ae_SoundGetVolumes(AeRam *r, uint32_t out); /* 800864B4 */
int32_t ae_SoundSaveVolumes(AeRam *r); /* 800864DC */
int32_t ae_SoundRestoreVolumes(AeRam *r, const AeProgressHooks *h); /* 800864FC */
int32_t ae_SaveBufferFill(AeRam *r, const AeProgressHooks *h); /* 80095B3C */
int32_t ae_SaveCaptureState(AeRam *r, const AeProgressHooks *h); /* 80095CEC */
int32_t ae_SaveHeaderFill(AeRam *r); /* 80095D44 */
int32_t ae_RecordBestTime(AeRam *r); /* 800961D0 */
uint32_t ae_SaveBlockPtr(AeRam *r); /* 8009628C */
int32_t ae_ProgressMarkStageResult(AeRam *r, int32_t area,
                                   int32_t exit_code); /* 800963DC */
int32_t ae_AreaKindVisitedGet(AeRam *r, const struct AeAreaSetHooks *area_hooks,
                              int32_t area); /* 80096514 */
int32_t ae_LevelProgressGet(AeRam *r, uint32_t level); /* 80096580 */
int32_t ae_HubDestinationState(AeRam *r, const AeProgressHooks *h,
                               int32_t area, int32_t exit_code); /* 800976B8 */
int32_t ae_HubGateLocked(AeRam *r, int32_t area); /* 800977BC */
int32_t ae_UnlockSpecialArea(AeRam *r); /* 80097820 */
int32_t ae_HubGateBitTest(AeRam *r, uint32_t bit); /* 800978E0 */
int32_t ae_ExitSpecialArea(AeRam *r, const AeProgressHooks *h); /* 800978FC */

#ifdef __cplusplus
}
#endif
#endif
