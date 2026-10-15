#ifndef AE_PLAYER1_H
#define AE_PLAYER1_H

#include "../core/ae_ram.h"
#include "../sound/ae_sound.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Original globals used by the first PlayerUpdate call-tree batch. */
#define AE_PLAYER1_PLAYER 0x800EC1E0u
#define AE_PLAYER1_AREA_ID 0x800F4476u
#define AE_PLAYER1_HUD 0x800F4DB0u
#define AE_PLAYER1_AIM_STATE 0x800E68D0u
#define AE_PLAYER1_CAMERA_MODE 0x800B4D44u
#define AE_PLAYER1_CAMERA_FLAGS 0x800E6908u
#define AE_PLAYER1_EVENT_CAMERA_SEED 0x800AE8F0u
#define AE_PLAYER1_TRAIL_RING 0x800B8AB0u
#define AE_PLAYER1_CURRENT_LEVEL 0x800F4474u
#define AE_PLAYER1_ARC_MEMBER_TABLE 0x800B7E50u
#define AE_PLAYER1_XA_ENTRY_TABLE 0x800A8530u
#define AE_PLAYER1_IDLE_MODEL_BLOCK 0x800B4D70u
#define AE_PLAYER1_PLAYER_STATE_SETS 0x800F4DE0u
#define AE_PLAYER1_CD_MANAGER 0x800E3890u
#define AE_PLAYER1_MODEL_COLOR_CYCLE_SOURCE 0x800AE860u
#define AE_PLAYER1_MODEL_COLOR_CYCLE_TIME 0x800EC350u
#define AE_PLAYER1_MODEL_COLOR_CYCLE_OUTPUT 0x800EA060u

/* Player fields. */
#define AE_PLAYER1_PLAYER_HEALTH 0x0E8u
#define AE_PLAYER1_PLAYER_INVULN_TIMER 0x0EAu
#define AE_PLAYER1_PLAYER_HURT 0x0F0u
#define AE_PLAYER1_PLAYER_IDLE_TIMER 0x148u
#define AE_PLAYER1_PLAYER_IDLE_LOADING 0x14Au
#define AE_PLAYER1_PLAYER_IDLE_READY 0x14Bu

/* HUD fields. */
#define AE_PLAYER1_HUD_DAMAGE 0x014u
#define AE_PLAYER1_HUD_DURATION 0x016u
#define AE_PLAYER1_HUD_HEALTH_TIMER 0x01Cu
#define AE_PLAYER1_HUD_FLAGS 0x020u

/* Aim/camera fields. */
#define AE_PLAYER1_AIM_REQUEST 0x032u
#define AE_PLAYER1_AIM_MODE 0x046u
#define AE_PLAYER1_EVENT_CAMERA_ACTIVE 0x041u
#define AE_PLAYER1_EVENT_CAMERA_TIMER 0x030u
#define AE_PLAYER1_EVENT_CAMERA_FLAGS 0x038u

/* Idle model block and PlayerStateSet entries. */
#define AE_PLAYER1_IDLE_BLOCK_MODEL_INSTANCE 0x010u
#define AE_PLAYER1_IDLE_BLOCK_SECONDARY_FRAMES 0x028u
#define AE_PLAYER1_IDLE_BLOCK_PLAYER_FRAMES 0x000u
#define AE_PLAYER1_IDLE_BLOCK_STATES 0x004u
#define AE_PLAYER1_IDLE_BLOCK_TRANSITIONS 0x008u
#define AE_PLAYER1_IDLE_BLOCK_EVENTS 0x00Cu
#define AE_PLAYER1_IDLE_BLOB_PLAYER_FRAMES_FIELD 0x004u
#define AE_PLAYER1_IDLE_BLOB_STATES_FIELD 0x008u
#define AE_PLAYER1_IDLE_BLOB_TRANSITIONS_FIELD 0x00Cu
#define AE_PLAYER1_IDLE_BLOB_EVENTS_FIELD 0x010u
#define AE_PLAYER1_IDLE_BLOB_MODEL_HEADER_FIELD 0x014u
#define AE_PLAYER1_IDLE_BLOB_SECONDARY_FRAMES_FIELD 0x018u
#define AE_PLAYER1_IDLE_STATESET_PLAYER_FRAMES 0x0A0u
#define AE_PLAYER1_IDLE_STATESET_STATES 0x0A4u
#define AE_PLAYER1_IDLE_STATESET_TRANSITIONS 0x0A8u
#define AE_PLAYER1_IDLE_STATESET_EVENTS 0x0ACu

/* Animation entry / fixed-point fields. */
#define AE_PLAYER1_ACTOR_ANIM_FRAME 0x05Cu
#define AE_PLAYER1_ANIM_ENTRY_FIRST_FRAME 0x000u
#define AE_PLAYER1_ANIM_ENTRY_DURATION 0x004u
#define AE_PLAYER1_ANIM_ENTRY_FRAME_BASE 0x006u
#define AE_PLAYER1_ANIM_FRACTION_BITS 12u
#define AE_PLAYER1_ANIM_FRACTION_MASK 0x0FFFu
#define AE_PLAYER1_ANIM_NEGATIVE_BIAS 0x0FFFu
#define AE_PLAYER1_ANIM_ONE 0x1000u

#define AE_PLAYER1_ARC_MEMBER_STRIDE 0x004u
#define AE_PLAYER1_ARC_MEMBER_SECTOR_MASK 0x000FFFFFu
#define AE_PLAYER1_ARC_MEMBER_OFFSET_SHIFT 20u
#define AE_PLAYER1_ARC_SECTOR_BYTES_SHIFT 11u
#define AE_PLAYER1_ARC_MEMBER_OFFSET_FIELD 0x004u

/* XA table and original PlayerUpdate/XaPlay stack locals. */
#define AE_PLAYER1_XA_ENTRY_STRIDE 0x00Cu
#define AE_PLAYER1_XA_ENTRY_SECTOR 0x000u
#define AE_PLAYER1_XA_ENTRY_CHANNEL 0x008u
#define AE_PLAYER1_XA_ENTRY_FILE 0x009u
#define AE_PLAYER1_XA_ENTRY_FLAGS 0x00Au
#define AE_PLAYER1_XA_STACK_FRAME 0x801FFEC0u
#define AE_PLAYER1_XA_STACK_PARAMS (AE_PLAYER1_XA_STACK_FRAME + 0x010u)
#define AE_PLAYER1_XA_STACK_LOCATION (AE_PLAYER1_XA_STACK_FRAME + 0x018u)
#define AE_PLAYER1_XA_STACK_MODE (AE_PLAYER1_XA_STACK_FRAME + 0x020u)

/* CD manager state and ring queue fields. */
#define AE_PLAYER1_CD_MANAGER_STATE 0x000u
#define AE_PLAYER1_CD_MANAGER_LAST_SECTOR 0x030u
#define AE_PLAYER1_CD_MANAGER_SUBSTATE 0x034u
#define AE_PLAYER1_CD_MANAGER_RETRY_COUNT 0x02Cu
#define AE_PLAYER1_CD_MANAGER_LAST_STATUS 0x038u
#define AE_PLAYER1_CD_MANAGER_VSYNC_RESULT 0x03Cu
#define AE_PLAYER1_CD_MANAGER_XA_INDEX 0x040u
#define AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_A 0x04Cu
#define AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_B 0x050u
#define AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_C 0x058u
#define AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_D 0x05Cu
#define AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_E 0x060u
#define AE_PLAYER1_CD_MANAGER_LAST_COMMAND 0x064u
#define AE_PLAYER1_CD_MANAGER_POSITION 0x068u
#define AE_PLAYER1_CD_MANAGER_POSITION_INT 0x06Cu
#define AE_PLAYER1_CD_MANAGER_READ_INDEX 0x074u
#define AE_PLAYER1_CD_MANAGER_WRITE_INDEX 0x076u
#define AE_PLAYER1_CD_QUEUE 0x078u
#define AE_PLAYER1_CD_QUEUE_CAPACITY 16u
#define AE_PLAYER1_CD_QUEUE_ENTRY_SIZE 0x01Cu
#define AE_PLAYER1_CD_QUEUE_COMMAND 0x000u
#define AE_PLAYER1_CD_QUEUE_PAYLOAD 0x004u
#define AE_PLAYER1_CD_QUEUE_RESULT 0x010u
#define AE_PLAYER1_CD_QUEUE_DONE_FLAG 0x014u
#define AE_PLAYER1_CD_QUEUE_PAYLOAD_POINTER 0x018u
#define AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_B 0x008u
#define AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_C 0x00Cu
#define AE_PLAYER1_CD_INPUT_WORD_B 0x004u
#define AE_PLAYER1_CD_INPUT_WORD_C 0x008u

/* Render input and temporary GTE projection buffer used by ModelDrawWithOt. */
#define AE_PLAYER1_RENDER_CAMERA_SETTINGS 0x800EBFA0u
#define AE_PLAYER1_RENDER_CAMERA_H 0x024u
#define AE_PLAYER1_RENDER_CAMERA_DQA 0x028u
#define AE_PLAYER1_RENDER_PROJECTION_OUTPUT 0x1F800000u
#define AE_PLAYER1_RENDER_PROJECTION_STATE 0x1F8003C0u
#define AE_PLAYER1_RENDER_PROJECTION_OFFSET 0x3C0u
#define AE_PLAYER1_RENDER_PROJECTION_VALUE 0x3C2u
#define AE_PLAYER1_RENDER_PROJECTION_FLAG 0x3C4u
#define AE_PLAYER1_RENDER_PROJECTION_ZERO 0x3C6u
#define AE_PLAYER1_RENDER_PROJECTION_COMMAND 0x3F4u
#define AE_PLAYER1_MODEL_DRAW_HEADER 0x000u
#define AE_PLAYER1_MODEL_DRAW_VERTICES 0x004u
#define AE_PLAYER1_MODEL_DRAW_EMIT_DATA 0x008u
#define AE_PLAYER1_MODEL_DRAW_VERTEX_COUNT 0x008u
#define AE_PLAYER1_MODEL_DRAW_VERTEX_STRIDE 0x004u
#define AE_PLAYER1_RIBBON_DESCRIPTOR 0x1F800300u
#define AE_PLAYER1_RIBBON_NORMAL_TEMP 0x1F800340u
#define AE_PLAYER1_RIBBON_DESCRIPTOR_START 0x000u
#define AE_PLAYER1_RIBBON_DESCRIPTOR_END 0x004u
#define AE_PLAYER1_RIBBON_DESCRIPTOR_SCALE 0x008u
#define AE_PLAYER1_RIBBON_DESCRIPTOR_COLOR 0x00Cu
#define AE_PLAYER1_RIBBON_DESCRIPTOR_NORMAL_X 0x010u
#define AE_PLAYER1_RIBBON_DESCRIPTOR_NORMAL_Y 0x012u
#define AE_PLAYER1_RIBBON_DESCRIPTOR_NORMAL_Z 0x014u
#define AE_PLAYER1_RIBBON_STACK_VECTOR 0x801FFED8u
#define AE_PLAYER1_VECTOR_X 0x000u
#define AE_PLAYER1_VECTOR_Y 0x002u
#define AE_PLAYER1_VECTOR_Z 0x004u
#define AE_PLAYER1_POINT_X 0x000u
#define AE_PLAYER1_POINT_Y 0x002u
#define AE_PLAYER1_MODEL_CYCLE_RECORD_STRIDE 0x008u
#define AE_PLAYER1_MODEL_CYCLE_RECORD_INDEX 0x002u
#define AE_PLAYER1_MODEL_CYCLE_OUTPUT_STRIDE 0x004u
#define AE_PLAYER1_MODEL_CYCLE_COUNT 20
#define AE_PLAYER1_CD_STATUS 0x800E38D4u
#define AE_PLAYER1_CD_QUEUE_FULL_FORMAT 0x800A4140u
#define AE_PLAYER1_CD_READY_CALLBACK_ARG 0x800128C8u

#define AE_PLAYER1_XA_MODE_COMMAND 0x0Bu
#define AE_PLAYER1_XA_FILTER_COMMAND 0x0Du
#define AE_PLAYER1_XA_MODE_CONFIG_COMMAND 0x0Eu
#define AE_PLAYER1_XA_DEMUTE_COMMAND 0x0Cu
#define AE_PLAYER1_XA_SYNC_COMMAND 0x01u
#define AE_PLAYER1_XA_READ_COMMAND 0x1Bu
#define AE_PLAYER1_XA_PAUSE_COMMAND 0x09u
#define AE_PLAYER1_XA_MODE_VALUE 0x48u
#define AE_PLAYER1_XA_LOOP_MODE_VALUE 0xC8u
#define AE_PLAYER1_CD_SPECIAL_COMMAND 0x20u
#define AE_PLAYER1_CD_READ_COMMAND 0x06u
#define AE_PLAYER1_CD_PENDING_STATE 0x00040000u
#define AE_PLAYER1_CD_BUSY_STATE 0x00050000u
#define AE_PLAYER1_CD_READING_STATE 0x00010000u
#define AE_PLAYER1_CD_DATA_STATE 0x00020000u
#define AE_PLAYER1_CD_DATA_SUBSTATE 0x00000002u
#define AE_PLAYER1_CD_READ_SUBSTATE 0x00000002u
#define AE_PLAYER1_CD_RETRY_SUBSTATE 0x00000014u
#define AE_PLAYER1_CD_RETRY_STATE 0x00010000u
#define AE_PLAYER1_CD_POSITION_UNKNOWN 0xFFFFFFFFu
#define AE_PLAYER1_CD_SPECIAL_CONTROL_COMMAND 0x06u

#define AE_PLAYER1_CURRENT_AREA_IS_53 0x0053u

/* Trail ring layout. The reset loop clears one word in each 0x14-byte record. */
#define AE_PLAYER1_TRAIL_RING_RESET_COUNT 12u
#define AE_PLAYER1_TRAIL_RING_FIRST_WORD 0x0F0u
#define AE_PLAYER1_TRAIL_RING_STRIDE 0x014u

#define AE_PLAYER1_LEVEL_NO_DAMAGE_A 0x001Fu
#define AE_PLAYER1_LEVEL_NO_DAMAGE_B 0x0020u
#define AE_PLAYER1_DAMAGE_INVULN_FRAMES 0x003Cu
#define AE_PLAYER1_DAMAGE_HUD_FRAMES 0x000Cu
#define AE_PLAYER1_HUD_VISIBLE_BIT 0x00000010u
#define AE_PLAYER1_HUD_TIMER_LIMIT 21
#define AE_PLAYER1_HUD_TIMER_VALUE 20

typedef uint32_t (*AePlayer1HookFn)(void *user, AeRam *r,
                                    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

typedef struct AePlayer1Hooks {
    void *user;
    AePlayer1HookFn CdIntToPos;          /* PsyQ @80025D20 */
    AePlayer1HookFn CdPosToInt;          /* PsyQ @80025E24 */
    AePlayer1HookFn CdControl;           /* PsyQ @8002408C */
    AePlayer1HookFn CdReadyCallback;     /* PsyQ @80024078 */
    AePlayer1HookFn VSync;               /* PsyQ @8002E520 */
    AePlayer1HookFn printf;              /* libc @800A3720 */
    AePlayer1HookFn SoundSetVolumeMode;  /* game function @80086248 */
    AePlayer1HookFn EventCamCreate;      /* game function @8004BB68 */
    AePlayer1HookFn ModelEmitPrimsCycleColors; /* native renderer owns GTE and GPU packet emission */
    AePlayer1HookFn DrawRibbonBetween; /* native ribbon renderer; a0 -> AePlayer1 ribbon descriptor */
    AePlayer1HookFn ModelProjectVertices; /* native GTE projection boundary */
    AePlayer1HookFn ModelEmitPrims;      /* renderer boundary @800180B8 */
    const AeSoundHw *sound_hw;
} AePlayer1Hooks;

void ae_TrailRingReset(AeRam *r);                         /* 8001F884 */
void ae_AimRequestClear(AeRam *r);                        /* 80048430 */
void ae_CameraFixedModeOff(AeRam *r);                    /* 80048748 */
int32_t ae_IsArea53(AeRam *r);                           /* 8005AB6C */
void ae_HudShowHealth(AeRam *r, int32_t mode);           /* 8005E42C */
void ae_PlayerDamage(AeRam *r, int32_t amount);          /* 8005E6F0 */
int32_t ae_XaPlay(AeRam *r, const AePlayer1Hooks *hooks, int32_t index); /* 80011F6C */
int32_t ae_CdMgrQueueCommand(AeRam *r, const AePlayer1Hooks *hooks, int32_t command,
                             uint32_t parameters, uint32_t result, uint32_t done_flag); /* 80012360 */
int32_t ae_CdMgrIssueCommand(AeRam *r, const AePlayer1Hooks *hooks, uint32_t queue_entry); /* 80012A0C */
void ae_ModelDrawWithOt(AeRam *r, const AePlayer1Hooks *hooks, uint32_t model,
                        uint32_t ordering_table, uint32_t draw_data, uint32_t flags); /* 80017D94 */
void ae_ModelPrepCycleColors(AeRam *r, uint32_t cycle_records, int32_t count); /* 8001D4A8 */
void ae_ModelEmitPrimsCycleColors(AeRam *r, const AePlayer1Hooks *hooks,
                                  uint32_t model_data, uint32_t cycle_table,
                                  uint32_t primitive_info, uint32_t flags); /* 8001D530 */
void ae_DrawRibbonBetween(AeRam *r, const AePlayer1Hooks *hooks,
                          uint32_t start, uint32_t end, int32_t scale, uint32_t color); /* 8002020C */
uint32_t ae_ArcGetMemberPos(AeRam *r, const AePlayer1Hooks *hooks, uint32_t output, uint32_t member_index); /* 80012C0C */
uint32_t ae_ActorAnimDecodeEntry(AeRam *r, uint32_t actor, uint32_t entry); /* 80014EF0 */
void ae_PlayerIdleModelSetup(AeRam *r, uint32_t blob);    /* 80032ADC */
void ae_AimModeEnter(AeRam *r, const AePlayer1Hooks *hooks); /* 8004B4B8 */
void ae_EventCamStartRandom(AeRam *r, const AePlayer1Hooks *hooks); /* 8004BC9C */

#ifdef __cplusplus
}
#endif
#endif
