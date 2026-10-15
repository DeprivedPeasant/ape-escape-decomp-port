#include "ae_misc.h"
#include "ae_misc_batch.h"

#include "ae_areaset.h"
#include "ae_items_init.h"
#include "ae_solid.h"
#include "../bodies/ae_bodies.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/native_player.h"
#include "../render/ae_tex_copy.h"
#include "../sound/ae_bgm.h"
#include "../sound/ae_sound.h"
#include "../world/ae_world.h"
#include "../effects/ae_spawn.h"
#include "../collision/ae_collprobe.h"
#include "../bodies/ae_probe.h"
#include <string.h>

#define AE_MISC_G_CAMERA_SLOT_POINTER       0x800E68D8u
#define AE_MISC_G_CAMERA_MANAGER             0x800E68D0u
#define AE_MISC_CAMERA_FIXED_FLAG          0x38u
#define AE_MISC_CAMERA_EYE_X               0x0Au
#define AE_MISC_CAMERA_EYE_Y               0x0Cu
#define AE_MISC_CAMERA_EYE_Z               0x0Eu
#define AE_MISC_CAMERA_TARGET_X            0x12u
#define AE_MISC_CAMERA_TARGET_Y            0x14u
#define AE_MISC_CAMERA_TARGET_Z            0x16u

#define AE_MISC_G_GAME_STATE               0x800F4350u
#define AE_MISC_GAME_STATE_FLAGS            0x100u
#define AE_MISC_GAME_STATE_MODE             0x150u
#define AE_MISC_G_INDICATOR_HINT_BUFFER     0x800BBD94u
#define AE_MISC_INDICATOR_HINT_TEXT         0x00u
#define AE_MISC_INDICATOR_HINT_FLAGS        0x04u
#define AE_MISC_INDICATOR_HINT_READY        0x05u
#define AE_MISC_G_INDICATOR_GAME_FLAGS      0x80104450u
#define AE_MISC_G_STAGE_TIMER              0x800B01C2u
#define AE_MISC_G_PLAYER                   0x800EC1E0u
#define AE_MISC_PLAYER_HEALTH              0xE8u
#define AE_MISC_PLAYER_STATE               0x70u
#define AE_MISC_PLAYER_VELOCITY_X          0x30u
#define AE_MISC_PLAYER_VELOCITY_Z          0x38u
#define AE_MISC_G_FRAME_COUNTER             0x800EC350u
#define AE_MISC_G_SPECIAL_ACTOR             0x800B4DC4u
#define AE_MISC_SPECIAL_ACTOR_NEAR_COUNT   0x8Fu
#define AE_MISC_SPECIAL_ACTOR_POSITION_X   0x24u
#define AE_MISC_SPECIAL_ACTOR_POSITION_Y   0x28u
#define AE_MISC_SPECIAL_ACTOR_POSITION_Z   0x2Cu
#define AE_MISC_SPECIAL_ACTOR_MODE          0x86u
#define AE_MISC_G_HUD_PANEL_B              0x800F4DB0u
#define AE_MISC_HUD_PANEL_B_COUNTER        0x00u
#define AE_MISC_HUD_PANEL_B_MODE           0x04u
#define AE_MISC_HUD_PANEL_B_TIMER           0x0Cu
#define AE_MISC_HUD_PANEL_B_STATE           0x0Eu
#define AE_MISC_HUD_PANEL_B_VALUE           0x10u
#define AE_MISC_HUD_PANEL_B_FLAGS           0x20u
#define AE_MISC_G_MESSAGE_BOX               0x800F5A00u
#define AE_MISC_MESSAGE_BOX_BUSY            0x00u
#define AE_MISC_MESSAGE_BOX_TEXT            0x04u
#define AE_MISC_MESSAGE_BOX_TEXT_COPY       0x08u
#define AE_MISC_MESSAGE_BOX_STATE           0x16u
#define AE_MISC_G_HINT_STATE                0x800BBD99u
#define AE_MISC_G_HINT_BUFFER               0x800BBD94u
#define AE_MISC_G_INDICATOR_COUNT            0x800BBC50u
#define AE_MISC_G_INDICATOR_SLOTS            0x800BBD34u
#define AE_MISC_INDICATOR_SLOT_SIZE          0x18u
#define AE_MISC_INDICATOR_KIND               0x00u
#define AE_MISC_INDICATOR_POSITION_X         0x04u
#define AE_MISC_INDICATOR_POSITION_Y         0x06u
#define AE_MISC_INDICATOR_POSITION_Z         0x08u
#define AE_MISC_INDICATOR_TIMER              0x0Au
#define AE_MISC_INDICATOR_FRAME              0x14u
#define AE_MISC_INDICATOR_STATE              0x16u
#define AE_MISC_G_INDICATOR_MODEL_A          0x800BBC54u
#define AE_MISC_G_INDICATOR_MODEL_B          0x800BBCC4u
#define AE_MISC_INDICATOR_MODEL_A_FRAME      0x800BBCB0u
#define AE_MISC_INDICATOR_MODEL_B_FRAME      0x800BBD20u
#define AE_MISC_G_HUD_FLAGS                 0x800F4DD0u
#define AE_MISC_G_PICKUP_TABLE              0x800BEFA0u
#define AE_MISC_PICKUP_RECORD_SIZE           0x18u
#define AE_MISC_PICKUP_ITEM_POINTER         0x00u
#define AE_MISC_PICKUP_POSITION_X           0x04u
#define AE_MISC_PICKUP_POSITION_Y           0x06u
#define AE_MISC_PICKUP_POSITION_Z           0x08u
#define AE_MISC_PICKUP_PARAMETER            0x0Au
#define AE_MISC_PICKUP_KIND                 0x0Au
#define AE_MISC_PICKUP_ROTATION             0x0Bu
#define AE_MISC_PICKUP_VELOCITY_X           0x0Cu
#define AE_MISC_PICKUP_VELOCITY_Y           0x0Eu
#define AE_MISC_PICKUP_VELOCITY_Z           0x10u
#define AE_MISC_PICKUP_FLOOR_HEIGHT          0x12u
#define AE_MISC_PICKUP_MODE                 0x14u
#define AE_MISC_PICKUP_AGE                  0x15u
#define AE_MISC_PICKUP_PHASE                0x16u
#define AE_MISC_PICKUP_TIMER                0x17u
#define AE_MISC_PICKUP_COUNT                 32u
#define AE_MISC_PICKUP_BODY_POSITION_X       0x00u
#define AE_MISC_PICKUP_BODY_POSITION_Y       0x04u
#define AE_MISC_PICKUP_BODY_POSITION_Z       0x08u
#define AE_MISC_PICKUP_BODY_CONTACT          0x2Cu
#define AE_MISC_G_PICKUP_POOL_FREE           0x800BEDA0u
#define AE_MISC_G_COLLISION_SLOTS            0x800BBDA0u
#define AE_MISC_COLLISION_SLOT_SIZE          0x14u
#define AE_MISC_COLLISION_SLOT_COUNT         256u
#define AE_MISC_G_COLLISION_HEAD             0x800B20D8u
#define AE_MISC_G_COLLISION_TAIL             0x800B20DCu
#define AE_MISC_G_RC_CAR                     0x800F5A20u
#define AE_MISC_RC_CAR_ACTIVE                0x70u
#define AE_MISC_RC_CAR_POSITION_X            0x24u
#define AE_MISC_RC_CAR_POSITION_Y            0x28u
#define AE_MISC_RC_CAR_POSITION_Z            0x2Cu
#define AE_MISC_PICKUP_SCALE                0x20u
#define AE_MISC_PICKUP_FIELD_24             0x24u
#define AE_MISC_PICKUP_FIELD_26             0x26u
#define AE_MISC_G_COLLECTED_ITEM_COUNT      0x800F44BAu
#define AE_MISC_G_COLLECTED_ITEMS           0x800DFB70u
#define AE_MISC_COLLECTED_ITEM_CAPACITY     0x7Fu
#define AE_MISC_COLLECTED_ITEM_STRIDE        2u
#define AE_MISC_COLLECTED_ITEM_TERMINATOR   0xFFu
#define AE_MISC_G_SEQ_TRACKS                 0x800F5AC0u
#define AE_MISC_SEQ_TRACK_SIZE               0x120u
#define AE_MISC_SEQ_TRACK_ACTIVE             0x24u
#define AE_MISC_SEQ_TRACK_CHANNELS           0x10u
#define AE_MISC_SEQ_CHANNEL_SIZE             0x10u
#define AE_MISC_SEQ_CHANNEL_PROGRAM          0x12u
#define AE_MISC_SEQ_CHANNEL_COUNT            16u
#define AE_MISC_G_SEQ_TRACKS_TABLE           0x800F5AC0u
#define AE_MISC_G_BOSS_UPDATE_TABLE          0x800A5390u
#define AE_MISC_G_OVERLAY_UPDATE_DISABLED    0x800AF840u
#define AE_MISC_AREA_OVERLAY_INVALID_RESULT   0x800A0000u
#define AE_MISC_AREA44_UPDATE_ENTRY           0x80140D60u
#define AE_MISC_AREA53_UPDATE_ENTRY           0x8013FD74u
#define AE_MISC_AREA56_UPDATE_ENTRY           0x80140A60u
#define AE_MISC_AREA46_UPDATE_ENTRY           0x8013FA2Cu
#define AE_MISC_AREA57_UPDATE_ENTRY           0x8013F464u
#define AE_MISC_COLLIDER_SCRIPT_MOTION_ENTRY  0x8008B8B4u
#define AE_MISC_COLLIDER_SCRIPT_UPDATE_ENTRY  0x8008B8D4u
#define AE_MISC_G_AREA_FEATURE_30_STATE       0x800F5AA0u
#define AE_MISC_ITEM_ENVIRONMENT_SCRATCH      0x1F800080u
#define AE_MISC_ITEM_ENVIRONMENT_MATRIX       0x800E7D00u
#define AE_MISC_ITEM_RENDER_MODE              0x1F8003F4u
#define AE_MISC_DEFAULT_STACK_POINTER         0x801FFF00u
#define AE_MISC_MATRIX_STACK_TEMP_SIZE        48u
#define AE_MISC_MATRIX_FALLBACK_STACK         (AE_MISC_DEFAULT_STACK_POINTER - AE_MISC_MATRIX_STACK_TEMP_SIZE)
#define AE_MISC_ROTATION_00                  0x00u
#define AE_MISC_ROTATION_01                  0x02u
#define AE_MISC_ROTATION_02                  0x04u
#define AE_MISC_ROTATION_10                  0x06u
#define AE_MISC_ROTATION_11                  0x08u
#define AE_MISC_ROTATION_12                  0x0Au
#define AE_MISC_ROTATION_20                  0x0Cu
#define AE_MISC_ROTATION_21                  0x0Eu
#define AE_MISC_ROTATION_22                  0x10u
#define AE_MISC_ROTATION_PAD                 0x12u
#define AE_MISC_VECTOR32_X                   0x00u
#define AE_MISC_VECTOR32_Y                   0x04u
#define AE_MISC_VECTOR32_Z                   0x08u
#define AE_MISC_SCREEN_POINT_X               0x00u
#define AE_MISC_SCREEN_POINT_Y               0x02u
#define AE_MISC_G_ENV_UVS                   0x800EA060u
#define AE_MISC_G_CAMERA_MATRIX             0x800E78C0u
#define AE_MISC_G_RENDER_OT_LIMIT           0x800EBFC8u
#define AE_MISC_G_CURRENT_OT                 0x800FC358u
#define AE_MISC_G_PICKUP_EFFECT              0x800B28B2u
#define AE_MISC_G_PICKUP_EFFECT_MODEL        0x800BF2A0u
#define AE_MISC_PICKUP_EFFECT_FRAME          0x0Bu
#define AE_MISC_PICKUP_EFFECT_ROTATION       0x0Bu
#define AE_MISC_PICKUP_EFFECT_Y              0x0Cu
#define AE_MISC_PICKUP_EFFECT_Z              0x0Eu
#define AE_MISC_G_HUD_POPUP                  0x800E07C0u
#define AE_MISC_G_COLLECTED_GOLD             0x800F448Cu
#define AE_MISC_G_GAME_MODE                  0x800F4470u
#define AE_MISC_G_TIME_ATTACK_SCORE          0x800F44B8u
#define AE_MISC_G_ITEM_PROGRESS_A            0x800F44D0u
#define AE_MISC_G_ITEM_PROGRESS_B            0x800F44D2u
#define AE_MISC_G_ITEM_TARGET_A              0x800F44A4u
#define AE_MISC_G_ITEM_TARGET_B              0x800F44A6u
#define AE_MISC_G_PLAYER                     0x800EC1E0u
#define AE_MISC_PLAYER_POS_X                 0x00u
#define AE_MISC_PLAYER_POS_Y                 0x04u
#define AE_MISC_PLAYER_POS_Z                 0x08u
#define AE_MISC_PLAYER_POS_X_S16             0x24u
#define AE_MISC_PLAYER_POS_Y_S16             0x28u
#define AE_MISC_PLAYER_POS_Z_S16             0x2Cu
#define AE_MISC_G_ITEM_SLOT_TABLE            0x800F4C30u
#define AE_MISC_ITEM_SLOT_SIZE               0x10u
#define AE_MISC_ITEM_SLOT_POINTER            0x00u
#define AE_MISC_ITEM_MODEL_POINTER           0x04u
#define AE_MISC_ITEM_FRAME                   0x0Au
#define AE_MISC_ITEM_POSITION_X              0x04u
#define AE_MISC_ITEM_POSITION_Y              0x06u
#define AE_MISC_ITEM_POSITION_Z              0x08u
#define AE_MISC_ITEM_KIND                    0x0Au
#define AE_MISC_ITEM_ROTATION                0x0Bu
#define AE_MISC_ITEM_ACTIVE                  0x0Au
#define AE_MISC_ITEM_DRAW_KIND               0x17u
#define AE_MISC_ITEM_EFFECT_SCALE             0x24u
#define AE_MISC_ITEM_EFFECT_MASK              0x01u
#define AE_MISC_ITEM_COUNT                   32u
#define AE_MISC_G_STAGE_EFFECT_COUNT         0x800B28B2u
#define AE_MISC_G_PLAYER_STATE                0x800F43C0u
#define AE_MISC_PLAYER_MODE                  0x70u
#define AE_MISC_PLAYER_HEALTH                0xE8u
#define AE_MISC_PLAYER_STATE_MODE            0x70u
#define AE_MISC_PLAYER_CLEAR_FIELD           0x152u
#define AE_MISC_GAME_MODE_OFFSET             0x120u
#define AE_MISC_GAME_MODE_TIME_ATTACK        19u
#define AE_MISC_GAME_STATE_GOLD              0x13Cu
#define AE_MISC_GAME_STATE_SCORE              0x168u
#define AE_MISC_GAME_STATE_RELIC_COUNT        0x16Au
#define AE_MISC_GAME_STATE_RELIC_ID           0x126u
#define AE_MISC_GAME_STATE_TALLY_TARGET_A     0x154u
#define AE_MISC_GAME_STATE_TALLY_TARGET_B     0x156u
#define AE_MISC_HUD_PICKUP_PANEL              0x800F4DB0u
#define AE_MISC_HUD_PICKUP_AMOUNT             0x02u
#define AE_MISC_HUD_PICKUP_KIND               0x03u
#define AE_MISC_G_STAGE_TALLY                 0x800F5120u
#define AE_MISC_STAGE_TALLY_A                 0xA1u
#define AE_MISC_STAGE_TALLY_B                 0xA2u
#define AE_MISC_SPECIAL_ACTOR_POINTER         0x800B4D70u
#define AE_MISC_SPECIAL_ACTOR_NEAR_TIMER      0x8Fu
#define AE_MISC_SPECIAL_ACTOR_POSITION         0x24u
#define AE_MISC_SPECIAL_ACTOR_PLAYER_NEAR      0x86u
#define AE_MISC_COLLISION_PROBE_TABLE         0x800B20E0u
#define AE_MISC_COLLISION_PROBE_COUNT         8u
#define AE_MISC_COLLISION_BODY_TYPE           0x800B20CCu
#define AE_MISC_COLLISION_BODY_HEAD           0x800B20D8u
#define AE_MISC_COLLISION_BODY_TAIL           0x800B20DCu
#define AE_MISC_COLLISION_WALL_PROBE          0x800F45E6u
#define AE_MISC_COLLISION_RAY_SCRATCH         0x1F8000F0u
#define AE_MISC_COLLISION_RAY_NORMAL          0x1F800100u
#define AE_MISC_COLLISION_RAY_AUX0            0x1F800108u
#define AE_MISC_COLLISION_RAY_AUX1            0x1F800110u
#define AE_MISC_COLLISION_RAY_FLAGS           0x1F80011Cu
#define AE_MISC_COLLISION_RAY_RESULT          0x1F800120u
#define AE_MISC_COLLISION_RAY_PARAMS           0x1F800140u
#define AE_MISC_COLLISION_RAY_PROBE_OFFSET     0x800B20B0u
#define AE_MISC_G_COLLISION_RAY_CURRENT_POSITION 0x800B20B4u
#define AE_MISC_G_COLLISION_RAY_COLLIDER_COPY  0x800B20BCu
#define AE_MISC_G_COLLISION_RAY_COLLIDER_FLAGS 0x800B20C0u
#define AE_MISC_G_COLLISION_RAY_COLLIDER_TABLE 0x800B2108u
#define AE_MISC_G_COLLISION_RAY_COLLIDERS      0x800F455Cu
#define AE_MISC_G_COLLISION_RAY_COLLIDER_COUNT 0x800F45D4u
#define AE_MISC_COLLISION_RAY_PARAMS_SCRATCH   0x1F800114u
#define AE_MISC_COLLISION_RAY_OUTPUT_SCRATCH   0x1F800124u
#define AE_MISC_COLLISION_RAY_GROUND_HIT       0x1F8003F0u
#define AE_MISC_COLLISION_RAY_GROUND_RESULT    0x0Au
#define AE_MISC_COLLISION_RAY_GRID_STRIDE       0x10u
#define AE_MISC_COLLISION_RAY_COLLIDER_SIZE     0x38u
#define AE_MISC_COLLISION_RAY_SHAPE_TYPE        0x12u
#define AE_MISC_COLLISION_RAY_SHAPE_FLAGS      0x0Cu
#define AE_MISC_COLLISION_RAY_CONTACT_ID       0x0Eu
#define AE_MISC_COLLISION_RAY_COLLIDER_FLAGS   0x14u
#define AE_MISC_COLLISION_RAY_COLLIDER_CONTACT 0x16u
#define AE_MISC_COLLISION_RAY_COPY_LOOP_INDEX  0x1Eu
#define AE_MISC_COLLISION_RAY_PARAM_X          0x00u
#define AE_MISC_COLLISION_RAY_PARAM_Y          0x04u
#define AE_MISC_COLLISION_RAY_PARAM_Z          0x08u
#define AE_MISC_COLLISION_RAY_PARAM_RADIUS      0x0Cu
#define AE_MISC_COLLISION_RAY_PARAM_HEIGHT      0x10u
#define AE_MISC_COLLISION_RAY_PARAM_COUNT       5u
#define AE_MISC_COLLISION_RAY_PARAM_WORD_SIZE   4u
#define AE_MISC_COLLISION_RAY_OUTPUT_SIZE       6u
#define AE_MISC_COLLISION_RAY_EXCLUDE_FLAG      0x2000u
#define AE_MISC_COLLISION_RAY_ENABLED_FLAG      0x8000u
#define AE_MISC_COLLISION_RAY_LOOP_GRID_ID      1
#define AE_MISC_COLLISION_RAY_LOOP_COLLIDER_ID  2
#define AE_MISC_COLLISION_RAY_LOOP_GROUND_ID    0
#define AE_MISC_COLLISION_RAY_MODE_GRID_FILTER  2u
#define AE_MISC_COLLISION_RAY_MODE_SLOT_COPY   1u
#define AE_MISC_COLL_PROBE_BOX                  0x80073BC8u
#define AE_MISC_COLL_PROBE_THIN_WALL            0x80075F48u
#define AE_MISC_COLL_PROBE_RAMP                 0x80073FD4u
#define AE_MISC_COLL_PROBE_RADIAL               0x80074BBCu
#define AE_MISC_COLL_PROBE_TRIANGLE             0x8007449Cu
#define AE_MISC_COLL_PROBE_EDGE_WALL            0x800761ACu
#define AE_MISC_COLL_PROBE_DIAGONAL_WALL        0x80076488u
#define AE_MISC_COLL_PROBE_NO_CONTACT           0x80077490u
#define AE_MISC_COLL_PROBE_STRIP                0x8007734Cu
#define AE_MISC_G_COLLIDER_SCRIPTS            0x800B3330u
#define AE_MISC_COLLIDER_SCRIPT_COUNT         0x800B3330u
#define AE_MISC_COLLIDER_SCRIPT_TABLE         0x800B3340u
#define AE_MISC_COLLIDER_SCRIPT_ENTRY_SIZE     0x20u
#define AE_MISC_COLLIDER_SCRIPT_TYPE           0x00u
#define AE_MISC_COLLIDER_SCRIPT_ACTOR          0x04u
#define AE_MISC_COLLIDER_SCRIPT_ACTIVE         0x08u
#define AE_MISC_COLLIDER_SCRIPT_ARGUMENT        0x0Cu
#define AE_MISC_COLLIDER_SCRIPT_OBJECT          0x10u
#define AE_MISC_COLLIDER_SCRIPT_BUFFER          0x800B33A0u
#define AE_MISC_OVERLAY_ACTIVE_FLAG             0x800AF840u
#define AE_MISC_G_POPUP_STATE                  0x800E07C0u
#define AE_MISC_POPUP_MODE                     0x00u
#define AE_MISC_POPUP_COUNTER                  0x01u
#define AE_MISC_POPUP_ANIMATION                0x02u
#define AE_MISC_POPUP_X                        0x04u
#define AE_MISC_POPUP_Y                        0x06u
#define AE_MISC_POPUP_COLOR                    0x08u
#define AE_MISC_POPUP_SCALE                    0x0Au
#define AE_MISC_POPUP_DRAW_X                   0x04u
#define AE_MISC_POPUP_DRAW_Y                   0x05u
#define AE_MISC_POPUP_DRAW_WIDTH               0x06u
#define AE_MISC_POPUP_DRAW_HEIGHT              0x07u
#define AE_MISC_POPUP_COLOR_HI                 0x09u
#define AE_MISC_SPECIAL_ACTOR_POSITION_X       0x24u
#define AE_MISC_SPECIAL_ACTOR_POSITION_Y       0x28u
#define AE_MISC_SPECIAL_ACTOR_POSITION_Z       0x2Cu
#define AE_MISC_SPECIAL_ACTOR_VELOCITY_X       0x30u
#define AE_MISC_SPECIAL_ACTOR_VELOCITY_Z       0x38u
#define AE_MISC_SOUND_TYPE                    0x00u
#define AE_MISC_SOUND_HANDLE                  0x02u
#define AE_MISC_SOUND_POSITION_X              0x04u
#define AE_MISC_SOUND_POSITION_Y              0x06u
#define AE_MISC_SOUND_POSITION_Z              0x08u
#define AE_MISC_SOUND_POSITION_POINTER        0x18u
#define AE_MISC_SOUND_POSITION2_X             0x0Au
#define AE_MISC_SOUND_POSITION2_Y             0x0Cu
#define AE_MISC_SOUND_RANGE_LINEAR            0x0Au
#define AE_MISC_SOUND_RANGE_RADIAL            0x0Eu
#define AE_MISC_SOUND_ID                      0x00u
#define AE_MISC_SOUND_PARAMETER               0x10u
#define AE_MISC_SOUND_ACTIVE                  0x12u
#define AE_MISC_SOUND_ACTOR_INDEX             0x10u
#define AE_MISC_SOUND_RECORD_SIZE             0x1Cu
#define AE_MISC_SOUND_RECORD_COUNT            24u
#define AE_MISC_SOUND_ATTENUATION_STATE       0x800EC250u
#define AE_MISC_SOUND_BGM_TRACK               0x800BF670u
#define AE_MISC_SOUND_LISTENER                0x800BF688u
#define AE_MISC_SOUND_LISTENER_Y              0x02u
#define AE_MISC_SOUND_LISTENER_Z              0x04u
#define AE_MISC_SOUND_LISTENER_X              0x00u
#define AE_MISC_SOUND_POINT_X                 0x00u
#define AE_MISC_SOUND_POINT_Y                 0x02u
#define AE_MISC_SOUND_POINT_Z                 0x04u
#define AE_MISC_SOUND_CURRENT_TRACK           0x800F5AC0u
#define AE_MISC_SOUND_CHANNEL_EFFECT           0x08u
#define AE_MISC_SOUND_CHANNEL_PROGRAM          0x12u
#define AE_MISC_SOUND_CHANNEL_BASE             0x10u
#define AE_MISC_SOUND_CHANNEL_STRIDE           0x10u
#define AE_MISC_SOUND_CHANNEL_COUNT            16u
#define AE_MISC_SOUND_BGM_ACTIVE               0x24u
#define AE_MISC_SOUND_BGM_TRACK_INDEX          0x06u
#define AE_MISC_SOUND_VOICE_VOLUME_LEFT        0xEAu
#define AE_MISC_SOUND_VOICE_VOLUME_RIGHT       0xECu
#define AE_MISC_SOUND_VOICE_ACTIVE             0x101u
#define AE_MISC_SOUND_BGM_PROGRAM_TABLE        0x10u
#define AE_MISC_SOUND_BGM_VOLUME               0x0Cu
#define AE_MISC_SOUND_EFFECT_FLAG              0x08u
#define AE_MISC_G_STAGE_STATE                  0x800F4350u
#define AE_MISC_STAGE_SUBMODE                  0x12Cu
#define AE_MISC_STAGE_TRACK_STATE              0x130u
#define AE_MISC_STAGE_TRACK_MIX                0x132u
#define AE_MISC_SPECIAL_ACTOR_TABLE             0x800E5500u
#define AE_MISC_SPECIAL_ACTOR_SIZE              0x250u
#define AE_MISC_SOUND_GLOBAL_MODE               0x800E6908u
#define AE_MISC_SOUND_LISTENER_CAMERA           0x800F45A8u
#define AE_MISC_SOUND_PAN_LEFT                  0x00u
#define AE_MISC_SOUND_PAN_RIGHT                 0x02u
#define AE_MISC_SOUND_ATTENUATION_MAX           2048u
#define AE_MISC_COLLIDER_SCRIPT_STATE           0x800F4350u
#define AE_MISC_COLLIDER_SCRIPT_COUNT_OFFSET    0x284u
#define AE_MISC_COLLIDER_SCRIPT_DATA_POINTER    0x20Cu
#define AE_MISC_COLLIDER_SCRIPT_CURRENT_INDEX   0x290u
#define AE_MISC_G_COLLIDER_SCRIPT_CURRENT_INDEX 0x800B3380u
#define AE_MISC_COLLIDER_SCRIPT_LIST_POINTER    0x218u
#define AE_MISC_COLLIDER_SCRIPT_OUTPUT_POINTER  0x214u
#define AE_MISC_COLLIDER_SCRIPT_KIND            0x18u
#define AE_MISC_COLLIDER_SCRIPT_STAGED_KIND     0x12u
#define AE_MISC_COLLIDER_SCRIPT_LENGTH          0x30u
#define AE_MISC_COLLIDER_SCRIPT_CURSOR          0x30u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_TABLE   0x800A62B0u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_NEXT    0x0Eu
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_SIZE    0x10u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_TYPE    0x00u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_MAGIC   0x02u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_INDEX   0x04u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_FIELD   0x06u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_VALUE_A 0x08u
#define AE_MISC_COLLIDER_SCRIPT_COMMAND_VALUE_B 0x0Au
#define AE_MISC_COLLIDER_SCRIPT_FUNCTION_TABLE  0x800B3344u
#define AE_MISC_COLLIDER_SCRIPT_BUFFER_POINTER  0x800B3340u
#define AE_MISC_COLLIDER_SCRIPT_BUFFER_BYTES    0x38u
#define AE_MISC_COLLIDER_SCRIPT_DATA_STRIDE     0x38u
#define AE_MISC_COLLIDER_SCRIPT_OUTPUT_STRIDE   0x10u
#define AE_MISC_COLLIDER_SCRIPT_OUTPUT_VALUE    0x0Eu
#define AE_MISC_MODEL_HEADER_ENV_MAP_FLAG       0x0Au
#define AE_MISC_PICKUP_POSITION32_X             0x00u
#define AE_MISC_PICKUP_POSITION32_Y             0x04u
#define AE_MISC_PICKUP_POSITION32_Z             0x08u
#define AE_MISC_PLAYER_EFFECT_POSITION_Y         0x02u
#define AE_MISC_PLAYER_EFFECT_POSITION_Z         0x04u
#define AE_MISC_COLLIDER_POSITION_X             0x00u
#define AE_MISC_COLLIDER_POSITION_Y             0x02u
#define AE_MISC_COLLIDER_POSITION_Z             0x04u
#define AE_MISC_MESH_VERTEX_COUNT               0x06u
#define AE_MISC_MESH_VERTEX_OFFSET              0x00u
#define AE_MISC_UV_RECORD_V                     0x06u
#define AE_MISC_COLLECTED_ITEM_FLAGS            0x01u
#define AE_MISC_COLLECTED_ITEM_KIND             0x02u
#define AE_MISC_G_SPECIAL_ACTOR_TABLE           0x800E5500u

static int16_t misc_s16(uint32_t value) {
    return (int16_t)(uint16_t)value;
}

static int32_t misc_call(AeRam *r, const AeMiscHooks *h, AeMiscCall fn,
                         const char *name, uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3) {
    if (fn == NULL) {
        ae_ram_fail(r, "AeMiscHooks.%s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

static void misc_body_printf(void *user, AeRam *r, uint32_t format, int32_t value) {
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    (void)misc_call(r, h, h != NULL ? h->Printf : NULL, "Printf",
                    format, (uint32_t)value, 0u, 0u);
}

static int32_t misc_body_sound_loop_start(void *user, AeRam *r,
                                          int32_t enabled, int32_t sound_id) {
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return misc_call(r, h, h != NULL ? h->SoundLoopStart : NULL,
                     "SoundLoopStart", (uint32_t)enabled,
                     (uint32_t)sound_id, 0u, 0u);
}

static void misc_body_sound_loop_stop(void *user, AeRam *r,
                                      int32_t sound_id, int32_t handle) {
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    (void)misc_call(r, h, h != NULL ? h->SoundLoopStop : NULL,
                    "SoundLoopStop", (uint32_t)sound_id,
                    (uint32_t)handle, 0u, 0u);
}

static const AeMiscHooks *misc_hooks_at_stack(const AeMiscHooks *hooks,
                                               AeMiscHooks *storage,
                                               uint32_t stack_pointer) {
    if (hooks == NULL) return NULL;
    *storage = *hooks;
    storage->stack_pointer = stack_pointer;
    return storage;
}

static uint32_t misc_track_address(int32_t track) {
    return AE_MISC_G_SEQ_TRACKS + (uint32_t)(int32_t)(int16_t)track *
           AE_MISC_SEQ_TRACK_SIZE;
}

/* MatrixRotYMul @80014750. The generated rotation is a local MATRIX handed to
 * the unported SDK MulMatrix entry point. The trigonometry is the verified SDK port. */
void ae_MatrixRotYMul(AeRam *r, const AeMiscHooks *h, int32_t angle,
                      uint32_t matrix) {
    uint32_t rotation = h != NULL && h->stack_pointer != 0u ?
                        h->stack_pointer - AE_MISC_MATRIX_STACK_TEMP_SIZE :
                        AE_MISC_MATRIX_FALLBACK_STACK;
    int32_t cosine = ae_sdk_rcos(r, angle);
    int32_t sine = ae_sdk_rsin(r, angle);
    ae_wr16(r, rotation + AE_MISC_ROTATION_00, (uint16_t)cosine);
    ae_wr16(r, rotation + AE_MISC_ROTATION_01, 0u);
    ae_wr16(r, rotation + AE_MISC_ROTATION_02, (uint16_t)-sine);
    ae_wr16(r, rotation + AE_MISC_ROTATION_10, 0u);
    ae_wr16(r, rotation + AE_MISC_ROTATION_11, 4096u);
    ae_wr16(r, rotation + AE_MISC_ROTATION_12, 0u);
    ae_wr16(r, rotation + AE_MISC_ROTATION_20, (uint16_t)sine);
    ae_wr16(r, rotation + AE_MISC_ROTATION_21, 0u);
    ae_wr16(r, rotation + AE_MISC_ROTATION_22, (uint16_t)cosine);
    ae_wr16(r, rotation + AE_MISC_ROTATION_PAD, 0u);
    (void)misc_call(r, h, h != NULL ? h->MulMatrix : NULL, "MulMatrix",
                    matrix, rotation, 0u, 0u);
}

/* CameraNudge @80047690. Camera coordinates are signed halfwords; addu and sh
 * make every moved coordinate wrap to 16 bits. */
void ae_CameraNudge(AeRam *r, int32_t dx, int32_t dy, int32_t dz) {
    uint32_t manager = AE_MISC_G_CAMERA_MANAGER;
    uint32_t slot = ae_rd32(r, AE_MISC_G_CAMERA_SLOT_POINTER);
    if (ae_rd8(r, manager + AE_MISC_CAMERA_FIXED_FLAG) == 1u) return;
    if ((ae_rd32(r, AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_FLAGS) & 1u) != 0u) return;

    ae_wr16(r, slot + AE_MISC_CAMERA_EYE_X,
            (uint16_t)((uint32_t)ae_rd16(r, slot + AE_MISC_CAMERA_EYE_X) + (uint32_t)dx));
    ae_wr16(r, slot + AE_MISC_CAMERA_EYE_Y,
            (uint16_t)((uint32_t)ae_rd16(r, slot + AE_MISC_CAMERA_EYE_Y) + (uint32_t)dy));
    ae_wr16(r, slot + AE_MISC_CAMERA_EYE_Z,
            (uint16_t)((uint32_t)ae_rd16(r, slot + AE_MISC_CAMERA_EYE_Z) + (uint32_t)dz));
    ae_wr16(r, slot + AE_MISC_CAMERA_TARGET_X,
            (uint16_t)((uint32_t)ae_rd16(r, slot + AE_MISC_CAMERA_TARGET_X) + (uint32_t)dx));
    ae_wr16(r, slot + AE_MISC_CAMERA_TARGET_Y,
            (uint16_t)((uint32_t)ae_rd16(r, slot + AE_MISC_CAMERA_TARGET_Y) + (uint32_t)dy));
    ae_wr16(r, slot + AE_MISC_CAMERA_TARGET_Z,
            (uint16_t)((uint32_t)ae_rd16(r, slot + AE_MISC_CAMERA_TARGET_Z) + (uint32_t)dz));
}

/* AreaOverlayUpdate @8005ACE8. Overlay bodies are per-area members and are
 * represented by named host hooks until those member functions are ported. */
int32_t ae_AreaOverlayUpdate(AeRam *r, const AeMiscHooks *h) {
    if (ae_rd8(r, AE_MISC_G_OVERLAY_UPDATE_DISABLED) != 0u) {
        return (int32_t)ae_rd8(r, AE_MISC_G_OVERLAY_UPDATE_DISABLED);
    }

    int32_t kind = ae_BossKindForArea(r, NULL);
    uint32_t slot = (uint32_t)(kind - 1);
    if (slot >= 5u) return (int32_t)AE_MISC_AREA_OVERLAY_INVALID_RESULT;

    static const struct {
        AeMiscCall *unused;
        uint32_t address;
        const char *name;
    } dispatch[5] = {
        {NULL, AE_MISC_AREA44_UPDATE_ENTRY, "FUN_80140d60"},
        {NULL, AE_MISC_AREA53_UPDATE_ENTRY, "FUN_8013fd74"},
        {NULL, AE_MISC_AREA56_UPDATE_ENTRY, "FUN_80140a60"},
        {NULL, AE_MISC_AREA46_UPDATE_ENTRY, "FUN_8013fa2c"},
        {NULL, AE_MISC_AREA57_UPDATE_ENTRY, "FUN_8013f464"}
    };
    const AeMiscCall handler[] = {
        h != NULL ? h->FUN_80140d60 : NULL,
        h != NULL ? h->FUN_8013fd74 : NULL,
        h != NULL ? h->FUN_80140a60 : NULL,
        h != NULL ? h->FUN_8013fa2c : NULL,
        h != NULL ? h->FUN_8013f464 : NULL
    };
    return misc_call(r, h, handler[slot], dispatch[slot].name,
                     0u, 0u, 0u, 0u);
}

/* StageTimerAdd @8005DD78. The argument is first sign-extended from s16, then
 * multiplied by 60 with 32-bit wrapping before the halfword timer is updated. */
void ae_StageTimerAdd(AeRam *r, int32_t seconds) {
    uint32_t narrowed = (uint32_t)(int32_t)misc_s16((uint32_t)seconds);
    uint32_t frames = ((narrowed << 4) - narrowed) << 2;
    ae_wr16(r, AE_MISC_G_STAGE_TIMER,
            ae_rd16(r, AE_MISC_G_STAGE_TIMER) + frames);
}

/* PlayerHeal @8005E794. The HUD update occurs before the health write. */
void ae_PlayerHeal(AeRam *r, const AeMiscHooks *h, int32_t amount) {
    uint32_t health = AE_MISC_G_PLAYER + AE_MISC_PLAYER_HEALTH;
    int32_t next = (int32_t)ae_rds16(r, health) + amount;
    (void)misc_call(r, h, h != NULL ? h->HudShowHealth : NULL,
                    "HudShowHealth", 1u, 0u, 0u, 0u);
    ae_wr16(r, health, (uint16_t)(next < 6 ? next : 5));
}

/* PlayerEyeBlinkUpdate @80067AF0. The frame counter is unsigned and the
 * original schedules open, blink and closed textures at four points per 120 frames. */
void ae_PlayerEyeBlinkUpdate(AeRam *r, const AeMiscHooks *h) {
    int32_t eye = -1;
    if (ae_AreaFeature30GetState(r) == 1) {
        eye = 0;
    } else if (ae_rds16(r, AE_MISC_G_PLAYER + AE_MISC_PLAYER_HEALTH) <= 0) {
        eye = 1;
    } else {
        uint32_t current_mode = ae_rd16(r, AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_MODE);
        uint32_t actor = ae_rd32(r, AE_MISC_SPECIAL_ACTOR_POINTER);
        if ((uint16_t)current_mode == 10u &&
            ae_rds16(r, actor + AE_MISC_SPECIAL_ACTOR_MODE) == 7) {
            eye = 2;
        }
    }

    if (eye >= 0) {
        (void)misc_call(r, h, h != NULL ? h->CopyPlayerEyeTexture : NULL,
                        "CopyPlayerEyeTexture", (uint32_t)eye, 0u, 0u, 0u);
        return;
    }

    uint32_t remainder = ae_rd32(r, AE_MISC_G_FRAME_COUNTER) % 120u;
    if (remainder == 0u) eye = 0;
    else if (remainder == 114u || remainder == 118u) eye = 1;
    else if (remainder == 116u) eye = 2;
    if (eye >= 0) {
        (void)misc_call(r, h, h != NULL ? h->CopyPlayerEyeTexture : NULL,
                        "CopyPlayerEyeTexture", (uint32_t)eye, 0u, 0u, 0u);
    }
}

/* IndicatorHintFree @800715B4. HeapFree is an already-verified RAM function. */
void ae_IndicatorHintFree(AeRam *r, const AeMiscHooks *h) {
    (void)h;
    (void)ae_HeapFree(r, ae_rd32(r, AE_MISC_G_HINT_BUFFER));
    ae_wr8(r, AE_MISC_G_HINT_STATE, 0u);
    ae_wr32(r, AE_MISC_G_HUD_FLAGS,
            ae_rd32(r, AE_MISC_G_HUD_FLAGS) | 0x10u);
}

/* IndicatorHintReady @800715FC. */
int32_t ae_IndicatorHintReady(AeRam *r, const AeMiscHooks *h) {
    (void)h;
    if (ae_rd8(r, AE_MISC_G_HINT_STATE) != 2u) return 0;
    return ae_MessageBoxBusy(r) == 0 ? 1 : 0;
}

/* IndicatorsUpdateDraw @800719F8. Animation and the hint-ready protocol stay
 * in game RAM. The per-slot projection, model pose, shadow and OT packet block
 * is the native renderer's RenderIndicator operation. */
void ae_IndicatorsUpdateDraw(AeRam *r, const AeMiscHooks *h) {
    uint32_t count = ae_rd32(r, AE_MISC_G_INDICATOR_COUNT);

    for (uint32_t index = 0; index < count; ++index) {
        uint32_t slot = AE_MISC_G_INDICATOR_SLOTS + index * AE_MISC_INDICATOR_SLOT_SIZE;
        (void)misc_call(r, h, h != NULL ? h->SetRotMatrix : NULL,
                        "SetRotMatrix", AE_MISC_G_CAMERA_MATRIX, 0u, 0u, 0u);
        (void)misc_call(r, h, h != NULL ? h->SetTransMatrix : NULL,
                        "SetTransMatrix", AE_MISC_G_CAMERA_MATRIX, 0u, 0u, 0u);
        int32_t kind = ae_rds16(r, slot + AE_MISC_INDICATOR_KIND);
        uint32_t frame = (uint32_t)ae_rd16(r, slot + AE_MISC_INDICATOR_FRAME) + 1u;
        uint16_t state = ae_rd16(r, slot + AE_MISC_INDICATOR_STATE);
        uint32_t model;
        uint32_t model_frame;
        int32_t render_mode = 0;

        if (kind >= 0 && kind <= 1) {
            if (state == 0u) {
                if (frame > 28u) frame = 0u;
            } else if (state == 1u && frame > 88u) {
                frame = 0u;
                ae_wr16(r, slot + AE_MISC_INDICATOR_STATE, 0u);
            }
            model = AE_MISC_G_INDICATOR_MODEL_A;
            model_frame = AE_MISC_INDICATOR_MODEL_A_FRAME;
            if (kind == 1) render_mode = 64;
        } else {
            if (kind == 2) {
                if (state == 0u) {
                    if (frame > 57u) frame = 0u;
                } else if (state == 1u && frame > 87u) {
                    frame = 0u;
                    ae_wr16(r, slot + AE_MISC_INDICATOR_STATE, 0u);
                }
                int32_t timer = ae_rds16(r, slot + AE_MISC_INDICATOR_TIMER);
                if (timer > 0) {
                    ae_wr16(r, slot + AE_MISC_INDICATOR_TIMER,
                            (uint16_t)(timer - 1));
                }
            }
            model = kind < 2 ? AE_MISC_G_INDICATOR_MODEL_A : AE_MISC_G_INDICATOR_MODEL_B;
            model_frame = kind < 2 ? AE_MISC_INDICATOR_MODEL_A_FRAME :
                                     AE_MISC_INDICATOR_MODEL_B_FRAME;
        }

        if (kind >= 0 && kind <= 1) {
            ae_wr16(r, model_frame, (uint16_t)frame);
        } else if (kind == 2) {
            ae_wr16(r, model_frame, (uint16_t)frame);
        }
        ae_wr16(r, slot + AE_MISC_INDICATOR_FRAME, (uint16_t)frame);
        (void)misc_call(r, h, h != NULL ? h->RenderIndicator : NULL,
                        "RenderIndicator", slot, model, (uint32_t)render_mode,
                        (uint32_t)kind);
    }

    if (count == 0u) return;

    if (ae_rd8(r, AE_MISC_G_HINT_STATE) == 2u &&
        ae_IndicatorHintReady(r, h) == 1) {
        ae_PlayerClearDAT44A2(r);
        ae_IndicatorHintFree(r, h);
        ae_wr32(r, AE_MISC_G_INDICATOR_GAME_FLAGS, 0u);
    }

    uint32_t flags = ae_rd32(r, AE_MISC_G_INDICATOR_HINT_BUFFER + AE_MISC_INDICATOR_HINT_FLAGS);
    if ((flags & 0x00FFFF00u) == 0x00010100u) {
        ae_MessageBoxSetText(r, ae_rd32(r, AE_MISC_G_INDICATOR_HINT_BUFFER + AE_MISC_INDICATOR_HINT_TEXT));
        ae_wr8(r, AE_MISC_G_INDICATOR_HINT_BUFFER + AE_MISC_INDICATOR_HINT_READY, 2u);
    }
}

/* MessageBoxSetText @8008DF20. */
void ae_MessageBoxSetText(AeRam *r, uint32_t text) {
    ae_wr8(r, AE_MISC_G_MESSAGE_BOX + AE_MISC_MESSAGE_BOX_STATE, 1u);
    ae_wr32(r, AE_MISC_G_MESSAGE_BOX + AE_MISC_MESSAGE_BOX_TEXT, text);
    ae_wr32(r, AE_MISC_G_MESSAGE_BOX + AE_MISC_MESSAGE_BOX_TEXT_COPY, text);
}

/* MessageBoxBusy @8008DF3C. */
int32_t ae_MessageBoxBusy(AeRam *r) {
    return (int32_t)ae_rd8(r, AE_MISC_G_MESSAGE_BOX + AE_MISC_MESSAGE_BOX_BUSY);
}

/* PlayerClearDAT44A2 @8007BC9C. */
void ae_PlayerClearDAT44A2(AeRam *r) {
    ae_wr16(r, AE_MISC_G_GAME_STATE + AE_MISC_PLAYER_CLEAR_FIELD, 0u);
}

/* AreaFeature30GetState @8009B7A0. */
int32_t ae_AreaFeature30GetState(AeRam *r) {
    return (int32_t)ae_rd8(r, AE_MISC_G_AREA_FEATURE_30_STATE);
}

/* HudPanelB_Start @80095600. */
int32_t ae_HudPanelB_Start(AeRam *r, int32_t mode, int32_t argument) {
    uint32_t panel = AE_MISC_G_HUD_PANEL_B;
    ae_wr16(r, panel + AE_MISC_HUD_PANEL_B_TIMER, mode == 0 ? 40u : 0u);
    ae_wr16(r, panel + AE_MISC_HUD_PANEL_B_STATE, mode != 0 ? 2u : 0u);
    ae_wr16(r, panel + AE_MISC_HUD_PANEL_B_VALUE, argument != 0 ? 52u : 0u);
    ae_wr16(r, panel + AE_MISC_HUD_PANEL_B_MODE, mode != 0 ? (uint16_t)-20 : 0u);
    ae_wr32(r, panel + AE_MISC_HUD_PANEL_B_FLAGS,
            ae_rd32(r, panel + AE_MISC_HUD_PANEL_B_FLAGS) | 8u);
    return 0;
}

/* HudCounterC_Start @800958C0. */
int32_t ae_HudCounterC_Start(AeRam *r, int32_t argument) {
    uint32_t panel = AE_MISC_G_HUD_PANEL_B;
    uint32_t flags = ae_rd32(r, panel + AE_MISC_HUD_PANEL_B_FLAGS);
    ae_wr32(r, panel + AE_MISC_HUD_PANEL_B_FLAGS, flags | 1u);
    int32_t counter = ae_rds16(r, panel + AE_MISC_HUD_PANEL_B_COUNTER);
    if (argument != 0) {
        ae_wr16(r, panel + AE_MISC_HUD_PANEL_B_COUNTER, 75u);
        return 0;
    }
    if (counter < 14) return 0;
    ae_wr16(r, panel + AE_MISC_HUD_PANEL_B_COUNTER,
            counter < 80 ? 8u : 0u);
    return 0;
}

/* PickupSetPosition @800836E0. */
void ae_PickupSetPosition(AeRam *r, uint32_t record) {
    uint32_t pickup = ae_rd32(r, record + AE_MISC_PICKUP_ITEM_POINTER);
    ae_wr32(r, pickup + AE_MISC_PICKUP_BODY_POSITION_X,
            (uint32_t)ae_rds16(r, record + AE_MISC_PICKUP_POSITION_X));
    ae_wr32(r, pickup + AE_MISC_PICKUP_BODY_POSITION_Y,
            (uint32_t)ae_rds16(r, record + AE_MISC_PICKUP_POSITION_Y));
    ae_wr32(r, pickup + AE_MISC_PICKUP_BODY_POSITION_Z,
            (uint32_t)ae_rds16(r, record + AE_MISC_PICKUP_POSITION_Z));
    ae_wr16(r, pickup + AE_MISC_PICKUP_FIELD_24,
            ae_rd8(r, record + AE_MISC_PICKUP_PARAMETER));
    ae_wr16(r, pickup + AE_MISC_PICKUP_FIELD_26, 0u);
    ae_wr16(r, pickup + AE_MISC_PICKUP_SCALE, 0x80u);
}

/* CollectedItemsAdd @80084964. The table is the existing validated item port. */
void ae_CollectedItemsAdd(AeRam *r, uint32_t item_id) {
    uint32_t table = ae_GetCollectedItemsTable(r);
    if ((int32_t)ae_rds16(r, AE_MISC_G_COLLECTED_ITEM_COUNT) >=
        (int32_t)AE_MISC_COLLECTED_ITEM_CAPACITY) return;
    if (ae_CollectedItemsHas(r, item_id & 0xFFFFu) != 0) return;

    uint32_t index = 0u;
    while (index < AE_MISC_COLLECTED_ITEM_CAPACITY) {
        uint32_t entry = table + index * AE_MISC_COLLECTED_ITEM_STRIDE;
        uint32_t existing = ae_rd8(r, entry);
        if (existing == AE_MISC_COLLECTED_ITEM_TERMINATOR) {
            ae_wr8(r, entry, item_id);
            ae_wr8(r, entry + AE_MISC_COLLECTED_ITEM_FLAGS, 1u);
            ae_wr8(r, entry + AE_MISC_COLLECTED_ITEM_KIND, existing);
            break;
        }
        index++;
    }
}

/* HudPopupStartPickup @80098398. */
void ae_HudPopupStartPickup(AeRam *r) {
    uint32_t popup = AE_MISC_G_HUD_POPUP;
    ae_wr8(r, popup, 1u);
    ae_wr8(r, popup + AE_MISC_POPUP_COUNTER, 24u);
    ae_wr16(r, popup + AE_MISC_POPUP_ANIMATION, 250u);
    ae_wr8(r, popup + AE_MISC_POPUP_DRAW_X, 192u);
    ae_wr8(r, popup + AE_MISC_POPUP_DRAW_Y, 128u);
    ae_wr8(r, popup + AE_MISC_POPUP_DRAW_WIDTH, 62u);
    ae_wr8(r, popup + AE_MISC_POPUP_DRAW_HEIGHT, 62u);
    ae_wr16(r, popup + AE_MISC_POPUP_COLOR, 0x7E2Du);
    ae_wr16(r, popup + AE_MISC_POPUP_SCALE, 27u);
}

/* HudPopupUpdate @800983E8. The animation mode/counter are game state; the
 * inline textured-quad packet and OT insertion block is RenderHudPopup. */
void ae_HudPopupUpdate(AeRam *r, const AeMiscHooks *h) {
    uint32_t popup = AE_MISC_G_POPUP_STATE;
    int32_t pickup_effect = ae_rd8(r, AE_MISC_G_PICKUP_EFFECT);
    int32_t time_attack = 0;

    if (pickup_effect != 0) {
        time_attack = 1;
    } else {
        time_attack = misc_call(r, h, h != NULL ? h->IsTimeAttack : NULL,
                                "IsTimeAttack", 0u, 0u, 0u, 0u);
    }
    if (time_attack == 0) return;

    uint32_t mode = ae_rd8(r, popup + AE_MISC_POPUP_MODE);
    if (mode == 0u) return;

    uint32_t counter = (ae_rd8(r, popup + AE_MISC_POPUP_COUNTER) - 1u) & 0xFFu;
    ae_wr8(r, popup + AE_MISC_POPUP_COUNTER, counter);
    if (counter == 0u) {
        if (mode == 5u) {
            ae_wr8(r, popup + AE_MISC_POPUP_MODE, 0u);
            return;
        }
        ae_wr8(r, popup + AE_MISC_POPUP_MODE, 5u);
        ae_wr8(r, popup + AE_MISC_POPUP_COUNTER, 8u);
        ae_wr16(r, popup + AE_MISC_POPUP_ANIMATION, 0x00FFu);
        mode = 5u;
    }

    (void)misc_call(r, h, h != NULL ? h->RenderHudPopup : NULL,
                    "RenderHudPopup", popup, AE_MISC_G_GAME_STATE,
                    ae_rd32(r, AE_MISC_G_FRAME_COUNTER),
                    pickup_effect != 0 ? 1u : 0u);
}

/* SeqGetChannelProgram @800A30D8. */
int32_t ae_SeqGetChannelProgram(AeRam *r, int32_t track, int32_t channel,
                                int32_t force) {
    if (force != 0) return -1;
    uint32_t track_address = misc_track_address(track);
    if (ae_rd8(r, track_address + AE_MISC_SEQ_TRACK_ACTIVE) == 0u) return -1;
    int32_t channel_index = misc_s16((uint32_t)channel);
    if (channel_index < 0 || channel_index >= (int32_t)AE_MISC_SEQ_CHANNEL_COUNT) return -1;

    uint32_t channel_table = ae_rd32(r, track_address + AE_MISC_SEQ_TRACK_CHANNELS);
    uint32_t program = ae_rd8(r, channel_table +
                              (uint32_t)channel_index * AE_MISC_SEQ_CHANNEL_SIZE +
                              AE_MISC_SEQ_CHANNEL_PROGRAM);
    return program == 0xFFu ? -1 : (int32_t)program;
}

/* SeqFadeTrackProgram @800A25CC. The original narrows its public arguments
 * independently before calling the already-verified SeqFadeInit port. */
int32_t ae_SeqFadeTrackProgram(AeRam *r, int32_t track, int32_t program,
                               int32_t volume, int32_t frames) {
    uint32_t track_address = misc_track_address(track);
    if (ae_rd8(r, track_address + AE_MISC_SEQ_TRACK_ACTIVE) == 0u) return -1;
    int32_t result = ae_SeqFadeInit(r, misc_s16((uint32_t)track),
                                    misc_s16((uint32_t)frames),
                                    misc_s16((uint32_t)program),
                                    misc_s16((uint32_t)volume));
    return misc_s16((uint32_t)result);
}

/* SfxVoiceSetPanVol @800A3570. SpuWaitIdle is a hardware wait; the signed
 * volume math is the already-verified SpuVolumeClamp port. */
void ae_SfxVoiceSetPanVol(AeRam *r, const AeMiscHooks *h, int32_t voice,
                          int32_t left, int32_t right) {
    int16_t voice_index = misc_s16((uint32_t)voice);
    uint32_t voice_record = AE_MISC_G_SEQ_TRACKS +
                            (uint32_t)(int32_t)voice_index * AE_MISC_SEQ_TRACK_SIZE;
    int32_t ready = misc_call(r, h, h != NULL ? h->SpuWaitIdle : NULL,
                              "SpuWaitIdle", 0u, 0u, 0u, 0u);
    if (ready != 0) return;

    int32_t clamped_left = ae_SpuVolumeClamp(r, 0x1000, misc_s16((uint32_t)left));
    ae_wr16(r, voice_record + AE_MISC_SOUND_VOICE_VOLUME_LEFT,
            (uint16_t)clamped_left);
    int32_t clamped_right = ae_SpuVolumeClamp(r, 0x1000, misc_s16((uint32_t)right));
    ae_wr16(r, voice_record + AE_MISC_SOUND_VOICE_VOLUME_RIGHT,
            (uint16_t)clamped_right);
    ae_wr8(r, voice_record + AE_MISC_SOUND_VOICE_ACTIVE, 1u);
}

/* MeshCalcEnvUVs @80016D60. The renderer's GTE environment-map operation
 * returns each packed UV pair; the game writes it into the original UV record. */
void ae_MeshCalcEnvUVs(AeRam *r, const AeMiscHooks *h, uint32_t instance) {
    uint32_t mesh = ae_rd32(r, instance);
    uint32_t vertex_count = ae_rd16(r, mesh + AE_MISC_MESH_VERTEX_COUNT);
    uint32_t vertex_data = mesh +
                           ((uint32_t)ae_rd16(r, mesh + AE_MISC_MESH_VERTEX_OFFSET) << 2);
    uint32_t uv_record = AE_MISC_G_ENV_UVS;
    for (uint32_t index = 0; index < vertex_count; ++index) {
        uint32_t uv = (uint32_t)misc_call(r, h,
                                          h != NULL ? h->RenderEnvUV : NULL,
                                          "RenderEnvUV", vertex_data,
                                          uv_record, instance, index);
        ae_wr16(r, uv_record + AE_MISC_UV_RECORD_V, (uint16_t)uv);
        uv_record += 8u;
        vertex_data += 8u;
    }
}

/* PickupCollect @80078A18. The switch preserves the original halfword kind
 * dispatch and each side effect's order. */
void ae_PickupCollect(AeRam *r, const AeMiscHooks *h, int32_t kind) {
    int16_t pickup_kind = misc_s16((uint32_t)kind);
    int32_t sfx = pickup_kind == 4 ? 14 : pickup_kind == 2 ? 13 : 122;
    (void)misc_call(r, h, h != NULL ? h->PlaySfx : NULL, "PlaySfx",
                    (uint32_t)sfx, 0u, 0u, 0u);

    switch (pickup_kind) {
        case 1:
            ae_PlayerHeal(r, h, 1);
            break;
        case 2: {
            uint32_t gold = AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_GOLD;
            uint32_t next_gold = (ae_rd16(r, gold) + 1u) & 0xFFFFu;
            ae_wr16(r, gold, next_gold < 101u ? next_gold : 100u);
            (void)ae_HudPanelB_Start(r, 0, 1);
            break;
        }
        case 3:
        case 5: {
            int32_t amount = pickup_kind == 5 ? 5 : 1;
            if (ae_rd16(r, AE_MISC_G_GAME_STATE + AE_MISC_GAME_MODE_OFFSET) ==
                AE_MISC_GAME_MODE_TIME_ATTACK) {
                ae_StageTimerAdd(r, amount);
                break;
            }

            uint32_t score_address = AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_SCORE;
            int32_t score = ae_rds16(r, score_address) + amount;
            ae_wr8(r, AE_MISC_HUD_PICKUP_PANEL + AE_MISC_HUD_PICKUP_AMOUNT,
                   (uint8_t)amount);
            ae_wr8(r, AE_MISC_HUD_PICKUP_PANEL + AE_MISC_HUD_PICKUP_KIND, 3u);
            if (score >= 100) {
                uint32_t gold = AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_GOLD;
                uint32_t next_gold = (ae_rd16(r, gold) + 1u) & 0xFFFFu;
                ae_wr16(r, gold, next_gold < 101u ? next_gold : 100u);
                score -= 100;
                (void)misc_call(r, h, h != NULL ? h->PlaySfx : NULL,
                                "PlaySfx", 13u, 0u, 0u, 0u);
                (void)ae_HudPanelB_Start(r, 0, 1);
            }
            ae_wr16(r, score_address, (uint16_t)score);
            (void)ae_HudCounterC_Start(r, 0);
            break;
        }
        case 4: {
            uint32_t count = AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_RELIC_COUNT;
            int16_t next_count = misc_s16(ae_rd16(r, count) + 1u);
            ae_wr16(r, count, next_count < 61 ? (uint16_t)next_count : 60u);
            ae_CollectedItemsAdd(r, ae_rd16(r, AE_MISC_G_GAME_STATE +
                                               AE_MISC_GAME_STATE_RELIC_ID));
            break;
        }
        case 6:
        case 7: {
            uint32_t tally_offset = pickup_kind == 6 ? AE_MISC_STAGE_TALLY_A :
                                                       AE_MISC_STAGE_TALLY_B;
            uint32_t target_offset = pickup_kind == 6 ? AE_MISC_GAME_STATE_TALLY_TARGET_A :
                                                        AE_MISC_GAME_STATE_TALLY_TARGET_B;
            uint32_t tally = AE_MISC_G_STAGE_TALLY + tally_offset;
            uint32_t target = AE_MISC_G_GAME_STATE + target_offset;
            uint32_t amount = ae_rd8(r, tally);
            if (amount < ae_rd16(r, target)) {
                ae_wr8(r, tally, amount + 1u);
            }
            break;
        }
        default:
            break;
    }
}

static void misc_collision_copy(AeRam *r, uint32_t destination,
                                uint32_t source, uint32_t byte_count) {
    uint8_t *to = ae_ptr(r, destination, byte_count);
    uint8_t *from = ae_ptr(r, source, byte_count);
    if (to != NULL && from != NULL) memmove(to, from, byte_count);
}

static int32_t misc_collision_probe(AeRam *r, const AeCollProbeHooks *probe_hooks,
                                    uint32_t function, uint32_t record,
                                    uint32_t params, uint32_t output) {
    switch (function) {
    case AE_MISC_COLL_PROBE_BOX: return ae_CollProbe_Box(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_THIN_WALL: return ae_CollProbe_ThinWall(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_RAMP: return ae_CollProbe_Ramp(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_RADIAL: return ae_CollProbe_Radial(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_TRIANGLE: return ae_CollProbe_Triangle(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_EDGE_WALL: return ae_CollProbe_EdgeWall(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_DIAGONAL_WALL: return ae_CollProbe_DiagonalWall(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_NO_CONTACT: return ae_CollProbe_NoContact(r, probe_hooks, record, params, output);
    case AE_MISC_COLL_PROBE_STRIP: return ae_CollProbe_Strip(r, probe_hooks, record, params, output);
    default:
        ae_ram_fail(r, "CollisionRayTest has unknown probe target %08X", function);
        return 0;
    }
}

/* CollisionRayTest @80078C9C. The probe handlers are the already ported
 * CollProbe_* routines; this function sequences their shared grid and collider
 * scans and keeps the original scratchpad and loop-slot side effects. */
int32_t ae_CollisionRayTest(AeRam *r, const AeMiscHooks *h, uint32_t mode,
                            uint32_t params, uint32_t out) {
    AeCollProbeHooks probe_hooks;
    memset(&probe_hooks, 0, sizeof(probe_hooks));
    /* Copied callbacks retain their caller's user context; never pass the
     * owning hook table itself. The game and module checkers use this contract. */
    probe_hooks.user = h != NULL ? h->user : NULL;
    if (h != NULL) {
        probe_hooks.PointInBoxXZ = h->PointInBoxXZ;
        probe_hooks.printf = h->Printf;
        probe_hooks.HalfPipeRangeTest = h->HalfPipeRangeTest;
        probe_hooks.ColliderSlopeContactDir = h->ColliderSlopeContactDir;
        probe_hooks.DirIndexFromFlags = h->DirIndexFromFlags;
        probe_hooks.PlayerSetClimbTarget = h->PlayerSetClimbTarget;
        probe_hooks.PlayerSetRope = h->PlayerSetRope;
    }

    uint32_t mode_kind = mode >> 16;
    uint32_t current_position = ae_rd32(r, AE_MISC_G_COLLISION_RAY_CURRENT_POSITION);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr32(r, current_position + axis * AE_MISC_COLLISION_RAY_PARAM_WORD_SIZE,
                ae_rd32(r, params + axis * AE_MISC_COLLISION_RAY_PARAM_WORD_SIZE));
    }
    ae_wr32(r, AE_MISC_COLLISION_RAY_PARAMS_SCRATCH,
            ae_rd32(r, params + AE_MISC_COLLISION_RAY_PARAM_RADIUS));
    ae_wr32(r, AE_MISC_COLLISION_RAY_PARAMS_SCRATCH + AE_MISC_COLLISION_RAY_PARAM_Z,
            ae_rd32(r, params + AE_MISC_COLLISION_RAY_PARAM_HEIGHT));
    ae_wr16(r, out, 0u);

    int32_t result = -1;
    uint32_t probes = ae_rd32(r, AE_MISC_COLLISION_RAY_PROBE_OFFSET);
    for (int32_t index = (int32_t)AE_MISC_COLLISION_PROBE_COUNT - 1;
         index >= 0; --index) {
        uint32_t record = probes + (uint32_t)index * AE_MISC_COLLISION_RAY_GRID_STRIDE;
        uint32_t shape = ae_rd32(r, record + AE_PROBE_SHAPE);
        if (shape == 0u) continue;

        int32_t type = ae_rds16(r, shape + AE_MISC_COLLISION_RAY_SHAPE_TYPE);
        uint32_t raw_type = (uint16_t)type;
        uint32_t scratch_params = AE_MISC_COLLISION_RAY_PARAMS_SCRATCH;
        uint32_t scratch_output = AE_MISC_COLLISION_RAY_OUTPUT_SCRATCH;
        ae_wr32(r, scratch_params + AE_MISC_COLLISION_RAY_PARAM_Y,
                (uint32_t)type);
        for (uint32_t field = 0; field < AE_MISC_COLLISION_RAY_OUTPUT_SIZE; ++field) {
            ae_wr32(r, scratch_output + field * AE_MISC_COLLISION_RAY_PARAM_WORD_SIZE, 0u);
        }
        uint32_t table_entry = AE_MISC_COLLISION_PROBE_TABLE +
                               ((raw_type >> 12) * AE_MISC_COLLISION_RAY_PARAM_WORD_SIZE);
        uint32_t function = ae_rd32(r, table_entry);
        if (misc_collision_probe(r, &probe_hooks, function, record,
                                 scratch_params, scratch_output) == 0) continue;

        uint16_t flags = ae_rd16(r, shape + AE_MISC_COLLISION_RAY_SHAPE_FLAGS);
        if (mode_kind == AE_MISC_COLLISION_RAY_MODE_GRID_FILTER &&
            (flags & AE_MISC_COLLISION_RAY_EXCLUDE_FLAG) != 0u) continue;
        if (ae_ColliderTypeActive(r, (uint32_t)type,
                                  (int16_t)(flags & AE_MISC_COLLISION_RAY_ENABLED_FLAG)) == 0) {
            continue;
        }
        result = ae_rds16(r, shape + AE_MISC_COLLISION_RAY_CONTACT_ID);
        if (mode_kind != 0u &&
            ae_LoopSlotRef(r, AE_MISC_COLLISION_RAY_LOOP_GRID_ID, shape) == 0) continue;
        ae_wr16(r, out, 1u);
        if (mode_kind != 0u) {
            (void)ae_LoopSlotAdd(r, AE_MISC_COLLISION_RAY_LOOP_GRID_ID, shape);
        }
    }

    int16_t collider_count = (int16_t)ae_rd16(r, AE_MISC_G_COLLISION_RAY_COLLIDER_COUNT);
    uint32_t collider_flags = ae_rd32(r, AE_MISC_G_COLLISION_RAY_COLLIDER_FLAGS);
    uint32_t colliders = ae_rd32(r, AE_MISC_G_COLLISION_RAY_COLLIDERS);
    uint32_t copied_collider = ae_rd32(r, AE_MISC_G_COLLISION_RAY_COLLIDER_COPY);
    for (int32_t index = (int32_t)collider_count - 1; index >= 0; --index) {
        if (ae_rd8(r, collider_flags + (uint32_t)index) != 0u) continue;
        uint32_t collider = colliders + (uint32_t)index * AE_MISC_COLLISION_RAY_COLLIDER_SIZE;
        misc_collision_copy(r, copied_collider, collider,
                            AE_MISC_COLLISION_RAY_COLLIDER_SIZE);
        uint16_t raw_type = ae_rd16(r, copied_collider + AE_COL_TYPE);
        ae_wr32(r, AE_MISC_COLLISION_RAY_PARAMS_SCRATCH + AE_MISC_COLLISION_RAY_PARAM_Y,
                raw_type);
        for (uint32_t field = 0; field < AE_MISC_COLLISION_RAY_OUTPUT_SIZE; ++field) {
            ae_wr32(r, AE_MISC_COLLISION_RAY_OUTPUT_SCRATCH +
                    field * AE_MISC_COLLISION_RAY_PARAM_WORD_SIZE, 0u);
        }
        uint32_t table_entry = AE_MISC_G_COLLISION_RAY_COLLIDER_TABLE +
                               (((uint32_t)raw_type & 0xF000u) >> 10);
        uint32_t function = ae_rd32(r, table_entry);
        if (misc_collision_probe(r, &probe_hooks, function, copied_collider,
                                 AE_MISC_COLLISION_RAY_PARAMS_SCRATCH,
                                 AE_MISC_COLLISION_RAY_OUTPUT_SCRATCH) == 0) continue;
        uint16_t flags = ae_rd16(r, copied_collider + AE_MISC_COLLISION_RAY_COLLIDER_FLAGS);
        if (ae_ColliderTypeActive(r, (uint32_t)(int32_t)(int16_t)raw_type,
                                  (int16_t)(flags & AE_MISC_COLLISION_RAY_ENABLED_FLAG)) == 0) {
            continue;
        }
        if (mode_kind == AE_MISC_COLLISION_RAY_MODE_GRID_FILTER &&
            (flags & AE_MISC_COLLISION_RAY_EXCLUDE_FLAG) != 0u) continue;

        result = ae_rds16(r, copied_collider + AE_MISC_COLLISION_RAY_COLLIDER_CONTACT);
        if (mode_kind == AE_MISC_COLLISION_RAY_MODE_SLOT_COPY) {
            int32_t missing = ae_LoopSlotRef(r, AE_MISC_COLLISION_RAY_LOOP_COLLIDER_ID,
                                             collider);
            if (missing == 0) {
                if (result == 10) break;
                continue;
            }
            ae_wr16(r, out, 1u);
            (void)ae_LoopSlotAdd(r, AE_MISC_COLLISION_RAY_LOOP_COLLIDER_ID,
                                 collider);
            ae_wr16(r, copied_collider + AE_MISC_COLLISION_RAY_COPY_LOOP_INDEX,
                    (uint16_t)index);
            misc_collision_copy(r, collider, copied_collider,
                                AE_MISC_COLLISION_RAY_COLLIDER_SIZE);
            if (result == 10) break;
        } else if (mode_kind >= 2u && mode_kind <= 3u) {
            if (ae_LoopSlotRef(r, AE_MISC_COLLISION_RAY_LOOP_COLLIDER_ID,
                               collider) == 0) break;
            ae_wr16(r, out, 1u);
            (void)ae_LoopSlotAdd(r, AE_MISC_COLLISION_RAY_LOOP_COLLIDER_ID,
                                 collider);
            break;
        } else {
            break;
        }
    }

    if (result >= 0) return result;

    int32_t ray_y = ae_rds32(r, params + AE_MISC_COLLISION_RAY_PARAM_Y);
    int32_t radius = ae_rds32(r, params + AE_MISC_COLLISION_RAY_PARAM_RADIUS);
    int32_t ground = misc_call(r, h, h != NULL ? h->GetGroundHeight : NULL,
                               "GetGroundHeight",
                               ae_rd32(r, params + AE_MISC_COLLISION_RAY_PARAM_X),
                               (uint32_t)(ray_y - radius),
                               ae_rd32(r, params + AE_MISC_COLLISION_RAY_PARAM_Z), 0u);
    if (ground >= ray_y - radius ||
        ae_rds16(r, AE_MISC_COLLISION_RAY_GROUND_HIT) >= 0) return result;

    result = ae_rds16(r, AE_MISC_COLLISION_RAY_GROUND_HIT +
                         AE_MISC_COLLISION_RAY_GROUND_RESULT);
    if (mode_kind != 0u) {
        if (ae_LoopSlotRef(r, AE_MISC_COLLISION_RAY_LOOP_GROUND_ID, 0u) == 0) return result;
        ae_wr16(r, out, 1u);
        (void)ae_LoopSlotAdd(r, AE_MISC_COLLISION_RAY_LOOP_GROUND_ID, 0u);
    } else {
        ae_wr16(r, out, 1u);
    }
    return result;
}

static int misc_pickup_near(AeRam *r, uint32_t object, uint32_t position) {
    int32_t pickup_x = ae_rds16(r, position + AE_MISC_PICKUP_POSITION_X);
    int32_t pickup_y = ae_rds16(r, position + AE_MISC_PICKUP_POSITION_Y) + 128;
    int32_t pickup_z = ae_rds16(r, position + AE_MISC_PICKUP_POSITION_Z);
    int32_t object_x = (int32_t)ae_rd32(r, object + AE_MISC_PLAYER_POS_X_S16);
    int32_t object_y = (int32_t)ae_rd32(r, object + AE_MISC_PLAYER_POS_Y_S16);
    int32_t object_z = (int32_t)ae_rd32(r, object + AE_MISC_PLAYER_POS_Z_S16);
    uint32_t dx = (uint32_t)object_x - (uint32_t)pickup_x;
    uint32_t dy = (uint32_t)object_y - (uint32_t)pickup_y;
    uint32_t dz = (uint32_t)object_z - (uint32_t)pickup_z;
    if ((int32_t)dx < 0) dx = 0u - dx;
    if ((int32_t)dy < 0) dy = 0u - dy;
    if ((int32_t)dz < 0) dz = 0u - dz;
    return dx < 512u && dy < 512u && dz < 512u;
}

static int32_t misc_half_toward_zero(int32_t value) {
    return value / 2;
}

/* PickupsUpdate @80083CB8. The pickup records, collision-body registrations,
 * motion state machine, trails, and effect requests remain on the RAM image. */
void ae_PickupsUpdate(AeRam *r, const AeMiscHooks *h) {
    AeBodyHooks body_hooks;
    memset(&body_hooks, 0, sizeof(body_hooks));
    body_hooks.user = (void *)h;
    body_hooks.printf_error = misc_body_printf;
    body_hooks.sound_loop_start = misc_body_sound_loop_start;
    body_hooks.sound_loop_stop = misc_body_sound_loop_stop;

    for (uint32_t index = 0; index < AE_MISC_PICKUP_COUNT; ++index) {
        uint32_t item = AE_MISC_G_PICKUP_TABLE + index * AE_MISC_PICKUP_RECORD_SIZE;
        uint32_t body = ae_rd32(r, item + AE_MISC_PICKUP_ITEM_POINTER);
        uint8_t kind = ae_rd8(r, item + AE_MISC_PICKUP_KIND);
        if (kind == 0u) continue;

        uint32_t mode = ae_rd8(r, item + AE_MISC_PICKUP_MODE);
        int near_player = 0;
        if (mode < 2u) {
            near_player = misc_pickup_near(r, AE_MISC_G_PLAYER, item);
            if (!near_player && ae_rd32(r, AE_MISC_G_RC_CAR + AE_MISC_RC_CAR_ACTIVE) != 0u) {
                near_player = misc_pickup_near(r, AE_MISC_G_RC_CAR, item);
            }

            if (body == 0u && near_player) {
                body = ae_CollisionBodyAlloc(r);
                if (body != 0u) {
                    ae_wr32(r, item + AE_MISC_PICKUP_ITEM_POINTER, body);
                    ae_PickupSetPosition(r, item);
                    if (ae_CollisionBodyRegister(r, &body_hooks, 9, body, 0u) < 0) {
                        (void)ae_CollisionBodyFree(r, body);
                        ae_wr32(r, item + AE_MISC_PICKUP_ITEM_POINTER, 0u);
                        body = 0u;
                    }
                }
            } else if (body != 0u && !near_player) {
                (void)ae_CollisionBodyFree(r, body);
                (void)ae_CollisionBodyUnregister(r, &body_hooks, body);
                ae_wr32(r, item + AE_MISC_PICKUP_ITEM_POINTER, 0u);
                body = 0u;
            }
        }

        if (body != 0u) {
            uint32_t contact_kind = ae_rd32(r, body + AE_MISC_PICKUP_BODY_CONTACT) >> 28;
            int consumed = contact_kind == 1u || contact_kind == 2u ||
                           contact_kind == 3u || contact_kind == 4u ||
                           contact_kind == 8u || contact_kind == 11u ||
                           contact_kind == 12u || contact_kind == 13u;
            if (consumed) {
                (void)ae_CollisionBodyFree(r, body);
                (void)ae_CollisionBodyUnregister(r, &body_hooks, body);
                ae_wr32(r, item + AE_MISC_PICKUP_ITEM_POINTER, 0u);
                body = 0u;
                ae_wr8(r, item + AE_MISC_PICKUP_MODE, 2u);
                ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_Y, (uint16_t)-80);
                ae_wr8(r, item + AE_MISC_PICKUP_PHASE, 10u);
                ae_wr8(r, item + AE_MISC_PICKUP_TIMER, 0u);

                if (kind == 2u || kind == 4u) {
                    (void)misc_call(r, h, h != NULL ? h->TrailSpawn : NULL,
                                    "TrailSpawn", item + AE_MISC_PICKUP_POSITION_X,
                                    4u, 512u, 0u);
                    (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                    "SpawnParticle", 0x113u,
                                    item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
                    (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                    "SpawnParticle", 0x109u,
                                    item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
                    for (uint32_t particle = 0; particle < 20u; ++particle) {
                        (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                        "SpawnParticle", 0x25u,
                                        item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
                    }
                    if (kind == 4u) {
                        ae_wr8(r, AE_MISC_G_PICKUP_EFFECT, 0x20u);
                        ae_HudPopupStartPickup(r);
                        ae_wr8(r, item + AE_MISC_PICKUP_KIND, 0u);
                    }
                } else {
                    (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                    "SpawnParticle", 0x103u,
                                    item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
                }
            }
        }

        kind = ae_rd8(r, item + AE_MISC_PICKUP_KIND);
        if (kind == 0u) continue;
        mode = ae_rd8(r, item + AE_MISC_PICKUP_MODE);

        if (mode == 1u) {
            uint32_t timer = ae_rd8(r, item + AE_MISC_PICKUP_TIMER);
            if (timer != 0u) {
                --timer;
                ae_wr8(r, item + AE_MISC_PICKUP_TIMER, timer);
                if (timer == 0u) {
                    ae_wr8(r, item + AE_MISC_PICKUP_KIND, 0u);
                    body = ae_rd32(r, item + AE_MISC_PICKUP_ITEM_POINTER);
                    if (body != 0u) {
                        (void)ae_CollisionBodyFree(r, body);
                        (void)ae_CollisionBodyUnregister(r, &body_hooks, body);
                        ae_wr32(r, item + AE_MISC_PICKUP_ITEM_POINTER, 0u);
                    }
                }
            }
        } else if (mode == 0u) {
            uint32_t x = (uint32_t)ae_rd16(r, item + AE_MISC_PICKUP_POSITION_X) +
                         ae_rd16(r, item + AE_MISC_PICKUP_VELOCITY_X);
            uint32_t y = (uint32_t)ae_rd16(r, item + AE_MISC_PICKUP_POSITION_Y) +
                         ae_rd16(r, item + AE_MISC_PICKUP_VELOCITY_Y);
            uint32_t z = (uint32_t)ae_rd16(r, item + AE_MISC_PICKUP_POSITION_Z) +
                         ae_rd16(r, item + AE_MISC_PICKUP_VELOCITY_Z);
            ae_wr16(r, item + AE_MISC_PICKUP_POSITION_X, x);
            ae_wr16(r, item + AE_MISC_PICKUP_POSITION_Y, y);
            ae_wr16(r, item + AE_MISC_PICKUP_POSITION_Z, z);

            int32_t sample_x = ae_rds16(r, item + AE_MISC_PICKUP_POSITION_X);
            int32_t sample_y = ae_rds16(r, item + AE_MISC_PICKUP_POSITION_Y);
            int32_t sample_z = ae_rds16(r, item + AE_MISC_PICKUP_POSITION_Z);
            int32_t velocity_y = ae_rds16(r, item + AE_MISC_PICKUP_VELOCITY_Y) + 7;
            ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_Y, (uint16_t)velocity_y);
            int32_t ground = misc_call(r, h, h != NULL ? h->GetGroundHeight : NULL,
                                       "GetGroundHeight", (uint32_t)sample_x,
                                       (uint32_t)sample_y, (uint32_t)sample_z, 0u);
            int32_t floor_height = (int16_t)(uint16_t)((uint32_t)ground - 64u);
            ae_wr16(r, item + AE_MISC_PICKUP_FLOOR_HEIGHT, (uint16_t)floor_height);
            int32_t item_y = ae_rds16(r, item + AE_MISC_PICKUP_POSITION_Y);

            if (item_y < floor_height) {
                if (ae_rd8(r, item + AE_MISC_PICKUP_AGE) >= 90u) {
                    ae_wr8(r, item + AE_MISC_PICKUP_MODE, 1u);
                    ae_wr8(r, item + AE_MISC_PICKUP_TIMER, 0xF0u);
                }
            } else if (item_y - floor_height >= 129) {
                int32_t velocity_x = ae_rds16(r, item + AE_MISC_PICKUP_VELOCITY_X);
                int32_t velocity_z = ae_rds16(r, item + AE_MISC_PICKUP_VELOCITY_Z);
                if (velocity_x != 0 || velocity_z != 0) {
                    ae_wr16(r, item + AE_MISC_PICKUP_POSITION_X,
                            (uint16_t)(ae_rd16(r, item + AE_MISC_PICKUP_POSITION_X) - velocity_x));
                    ae_wr16(r, item + AE_MISC_PICKUP_POSITION_Z,
                            (uint16_t)(ae_rd16(r, item + AE_MISC_PICKUP_POSITION_Z) - velocity_z));
                    ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_X,
                            (uint16_t)-misc_half_toward_zero(velocity_x));
                    ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_Z,
                            (uint16_t)-misc_half_toward_zero(velocity_z));
                } else {
                    velocity_y = ae_rds16(r, item + AE_MISC_PICKUP_VELOCITY_Y);
                    velocity_y = -velocity_y / 3;
                    ae_wr16(r, item + AE_MISC_PICKUP_POSITION_Y, (uint16_t)floor_height);
                    ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_Y, (uint16_t)velocity_y);
                    if (velocity_y >= -14) {
                        ae_wr8(r, item + AE_MISC_PICKUP_MODE, 1u);
                        ae_wr8(r, item + AE_MISC_PICKUP_TIMER, 0xF0u);
                    }
                }
                (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                "SpawnParticle", 4u,
                                item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
            } else {
                velocity_y = ae_rds16(r, item + AE_MISC_PICKUP_VELOCITY_Y);
                velocity_y = -velocity_y / 3;
                ae_wr16(r, item + AE_MISC_PICKUP_POSITION_Y, (uint16_t)floor_height);
                ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_Y, (uint16_t)velocity_y);
                if (velocity_y >= -14) {
                    ae_wr8(r, item + AE_MISC_PICKUP_MODE, 1u);
                    ae_wr8(r, item + AE_MISC_PICKUP_TIMER, 0xF0u);
                }
                (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                "SpawnParticle", 4u,
                                item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
            }
        } else if (mode == 2u) {
            ae_wr16(r, item + AE_MISC_PICKUP_POSITION_Y,
                    (uint16_t)(ae_rd16(r, item + AE_MISC_PICKUP_POSITION_Y) +
                               ae_rd16(r, item + AE_MISC_PICKUP_VELOCITY_Y)));
            ae_wr16(r, item + AE_MISC_PICKUP_VELOCITY_Y,
                    (uint16_t)(ae_rd16(r, item + AE_MISC_PICKUP_VELOCITY_Y) + 12u));
            uint32_t phase = (ae_rd8(r, item + AE_MISC_PICKUP_PHASE) - 1u) & 0xFFu;
            ae_wr8(r, item + AE_MISC_PICKUP_PHASE, phase);
            if (phase == 0u) {
                ae_wr8(r, item + AE_MISC_PICKUP_MODE, 3u);
                ae_wr8(r, item + AE_MISC_PICKUP_PHASE, 8u);
            }
        } else if (mode == 3u) {
            uint32_t countdown = ae_rd8(r, item + AE_MISC_PICKUP_PHASE);
            uint32_t phase = (countdown - 1u) & 0xFFu;
            ae_wr8(r, item + AE_MISC_PICKUP_PHASE, phase);
            /* This is the delay slot of the countdown branch, so it runs for
             * both the continuing and terminating cases. */
            ae_wr8(r, item + AE_MISC_PICKUP_ROTATION,
                   ae_rd8(r, item + AE_MISC_PICKUP_ROTATION) + countdown);
            if (phase == 0u) {
                ae_wr8(r, item + AE_MISC_PICKUP_KIND, 0u);
                (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                                "SpawnParticle", 0x104u,
                                item + AE_MISC_PICKUP_POSITION_X, 0u, 0u);
            }
        }

        kind = ae_rd8(r, item + AE_MISC_PICKUP_KIND);
        if (kind == 0u) continue;
        mode = ae_rd8(r, item + AE_MISC_PICKUP_MODE);
        body = ae_rd32(r, item + AE_MISC_PICKUP_ITEM_POINTER);
        if (mode < 2u && body != 0u) {
            ae_wr32(r, body + AE_MISC_PICKUP_BODY_POSITION_X,
                    (uint32_t)ae_rds16(r, item + AE_MISC_PICKUP_POSITION_X));
            ae_wr32(r, body + AE_MISC_PICKUP_BODY_POSITION_Y,
                    (uint32_t)ae_rds16(r, item + AE_MISC_PICKUP_POSITION_Y));
            ae_wr32(r, body + AE_MISC_PICKUP_BODY_POSITION_Z,
                    (uint32_t)ae_rds16(r, item + AE_MISC_PICKUP_POSITION_Z));
        }
        ae_wr8(r, item + AE_MISC_PICKUP_AGE,
               ae_rd8(r, item + AE_MISC_PICKUP_AGE) + 1u);
        ae_wr8(r, item + AE_MISC_PICKUP_ROTATION,
               ae_rd8(r, item + AE_MISC_PICKUP_ROTATION) + 1u);
    }
}

/* ItemsUpdateDraw @80084598. */
void ae_ItemsUpdateDraw(AeRam *r, const AeMiscHooks *h) {
    if (ae_rd32(r, AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_FLAGS) == 0u) {
        (void)ae_PickupsUpdate(r, h);
    }
    ae_ItemsDraw(r, h);
}

/* ItemDrawModelEnvMap @800846D8. The model transforms and final display-list
 * submission stay behind renderer hooks; the item and matrix state remain in RAM. */
void ae_ItemDrawModelEnvMap(AeRam *r, const AeMiscHooks *h,
                            int32_t model_index, uint32_t item) {
    uint32_t sp = h != NULL && h->stack_pointer != 0u ?
                  h->stack_pointer : AE_MISC_DEFAULT_STACK_POINTER;
    uint32_t matrix = sp - AE_MISC_MATRIX_STACK_TEMP_SIZE;
    uint32_t position = sp - 32u;

    (void)ae_sdk_MatrixIdentity(r, matrix);
    ae_wr32(r, position + AE_MISC_VECTOR32_X,
            (uint32_t)ae_rds16(r, item + AE_MISC_ITEM_POSITION_X));
    ae_wr32(r, position + AE_MISC_VECTOR32_Y,
            (uint32_t)ae_rds16(r, item + AE_MISC_ITEM_POSITION_Y));
    ae_wr32(r, position + AE_MISC_VECTOR32_Z,
            (uint32_t)ae_rds16(r, item + AE_MISC_ITEM_POSITION_Z));
    AeMiscHooks nested_hooks;
    const AeMiscHooks *matrix_hooks =
        misc_hooks_at_stack(h, &nested_hooks, sp - 64u);
    ae_MatrixRotYMul(r, matrix_hooks,
                     (int32_t)ae_rd8(r, item + AE_MISC_ITEM_ROTATION) << 6,
                     matrix);
    (void)ae_sdk_CompMatrix(r, AE_MISC_G_CAMERA_MATRIX, matrix, matrix);

    uint32_t slot = AE_MISC_G_ITEM_SLOT_TABLE +
                    (uint32_t)model_index * AE_MISC_ITEM_SLOT_SIZE;
    uint32_t model_header = ae_rd32(r, slot + AE_MISC_ITEM_SLOT_POINTER);
    uint32_t environment = ae_rd16(r, model_header + AE_MISC_MODEL_HEADER_ENV_MAP_FLAG) != 0u ?
                          AE_MISC_ITEM_ENVIRONMENT_SCRATCH :
                          AE_MISC_ITEM_ENVIRONMENT_MATRIX;
    (void)misc_call(r, h, h != NULL ? h->ModelTransformRigid : NULL,
                    "ModelTransformRigid", slot, matrix, environment, 0u);
    ae_MeshCalcEnvUVs(r, h, slot);
    ae_wr8(r, AE_MISC_ITEM_RENDER_MODE, 2u);
    (void)misc_call(r, h, h != NULL ? h->ModelDrawEnvMap : NULL,
                    "ModelDrawEnvMap", ae_rd32(r, AE_MISC_G_CURRENT_OT),
                    environment, slot, 0u);
}

/* ItemsDraw @8008435C. Game-side slot visibility, distance thresholds,
 * pickup animation and particle state stay here. Projection and model packet
 * work are named SDK/renderer calls. */
void ae_ItemsDraw(AeRam *r, const AeMiscHooks *h) {
    uint32_t stack = h != NULL && h->stack_pointer != 0u ?
                     h->stack_pointer : AE_MISC_DEFAULT_STACK_POINTER;
    uint32_t limit = ae_rd32(r, AE_MISC_G_RENDER_OT_LIMIT);
    if (limit > 2048u) limit = 2048u;

    for (uint32_t index = 0; index < AE_MISC_ITEM_COUNT; ++index) {
        uint32_t item = AE_MISC_G_PICKUP_TABLE + index * AE_MISC_PICKUP_RECORD_SIZE;
        if (ae_rd8(r, item + AE_MISC_ITEM_ACTIVE) == 0u) continue;

        uint32_t draw_kind = ae_rd8(r, item + AE_MISC_ITEM_DRAW_KIND);
        if (draw_kind >= 1u && draw_kind <= 45u && (draw_kind & 1u) == 0u) continue;

        uint32_t view = AE_MISC_G_CAMERA_MATRIX;
        (void)misc_call(r, h, h != NULL ? h->SetRotMatrix : NULL,
                        "SetRotMatrix", view, 0u, 0u, 0u);
        (void)misc_call(r, h, h != NULL ? h->SetTransMatrix : NULL,
                        "SetTransMatrix", view, 0u, 0u, 0u);

        uint32_t screen = stack - 40u;
        uint32_t depth = stack - 36u;
        uint32_t flag = stack - 36u;
        int32_t otz = misc_call(r, h, h != NULL ? h->RotTransPers : NULL,
                                "RotTransPers", item + AE_MISC_ITEM_POSITION_X,
                                screen, depth, flag);
        if ((uint32_t)otz > limit || otz <= 0) continue;

        int32_t screen_x = ae_rds16(r, screen) + 32;
        int32_t screen_y = ae_rds16(r, screen + AE_MISC_SCREEN_POINT_Y);
        if (screen_x < 0 || screen_x >= 449 || screen_y < -32 || screen_y >= 273) continue;

        switch (ae_rd8(r, item + AE_MISC_ITEM_ACTIVE)) {
            case 1:
                (void)misc_call(r, h, h != NULL ? h->ModelDraw : NULL,
                                "ModelDraw", 14u, item, 0u, 0u);
                break;
            case 2:
                (void)misc_call(r, h, h != NULL ? h->ModelDraw : NULL,
                                "ModelDraw", 17u, item, 0u, 0u);
                break;
            case 3:
                {
                    AeMiscHooks nested_hooks;
                    const AeMiscHooks *item_hooks =
                        misc_hooks_at_stack(h, &nested_hooks, stack - 64u);
                    ae_ItemDrawModelEnvMap(r, item_hooks, 15, item);
                }
                break;
            case 4:
                {
                    AeMiscHooks nested_hooks;
                    const AeMiscHooks *item_hooks =
                        misc_hooks_at_stack(h, &nested_hooks, stack - 64u);
                    ae_ItemDrawModelEnvMap(r, item_hooks, 13, item);
                }
                break;
            case 5:
                {
                    AeMiscHooks nested_hooks;
                    const AeMiscHooks *item_hooks =
                        misc_hooks_at_stack(h, &nested_hooks, stack - 64u);
                    ae_ItemDrawModelEnvMap(r, item_hooks, 16, item);
                }
                break;
            case 6:
                (void)misc_call(r, h, h != NULL ? h->ModelDraw : NULL,
                                "ModelDraw", 18u, item, 0u, 0u);
                break;
            case 7:
                (void)misc_call(r, h, h != NULL ? h->ModelDraw : NULL,
                                "ModelDraw", 19u, item, 0u, 0u);
                break;
            default:
                break;
        }
    }

    uint32_t effect_count = ae_rd8(r, AE_MISC_G_STAGE_EFFECT_COUNT);
    if (effect_count == 0u) return;

    uint32_t effect_item = AE_MISC_G_PICKUP_EFFECT_MODEL;
    uint32_t player = AE_MISC_G_PLAYER;
    ae_wr8(r, effect_item + AE_MISC_PICKUP_EFFECT_ROTATION,
           effect_count << 2);
    uint32_t position = stack - 48u;
    ae_wr16(r, position, ae_rd16(r, player + AE_MISC_PLAYER_POS_X_S16));
    ae_wr16(r, position + AE_MISC_PLAYER_EFFECT_POSITION_Y,
            ae_rd16(r, player + AE_MISC_PLAYER_POS_Y_S16) +
            ((effect_count << 3) - 512u));
    ae_wr16(r, position + AE_MISC_PLAYER_EFFECT_POSITION_Z,
            ae_rd16(r, player + AE_MISC_PLAYER_POS_Z_S16));
    if ((effect_count & 7u) == 0u) {
        (void)misc_call(r, h, h != NULL ? h->SpawnParticle : NULL,
                        "SpawnParticle", 0x104u, position, 0u, 0u);
    }
    ae_wr16(r, effect_item + AE_MISC_ITEM_POSITION_X, ae_rd16(r, position));
    ae_wr16(r, effect_item + AE_MISC_ITEM_POSITION_Y,
            ae_rd16(r, position + AE_MISC_PLAYER_EFFECT_POSITION_Y));
    ae_wr16(r, effect_item + AE_MISC_ITEM_POSITION_Z,
            ae_rd16(r, position + AE_MISC_PLAYER_EFFECT_POSITION_Z));
    AeMiscHooks nested_hooks;
    const AeMiscHooks *item_hooks =
        misc_hooks_at_stack(h, &nested_hooks, stack - 64u);
    ae_ItemDrawModelEnvMap(r, item_hooks, 13, effect_item);
    ae_wr8(r, AE_MISC_G_STAGE_EFFECT_COUNT, effect_count - 1u);
}

/* SpecialActorPlayerNear @8009B6C8. Coordinate subtraction wraps like subu. */
void ae_SpecialActorPlayerNear(AeRam *r) {
    uint32_t actor = ae_rd32(r, AE_MISC_G_SPECIAL_ACTOR);
    uint32_t player = AE_MISC_G_PLAYER;
    uint32_t y_delta = ae_rd32(r, actor + AE_MISC_SPECIAL_ACTOR_POSITION_Y) -
                       ae_rd32(r, player + AE_MISC_PLAYER_POS_Y_S16);
    if ((uint32_t)(y_delta + 512u) >= 1537u) {
        ae_wr8(r, actor + AE_MISC_SPECIAL_ACTOR_NEAR_TIMER, 0u);
        return;
    }

    uint32_t actor_x = ae_rd32(r, actor + AE_MISC_SPECIAL_ACTOR_POSITION_X);
    uint32_t player_x = ae_rd32(r, player + AE_MISC_PLAYER_POS_X_S16);
    int32_t delta_x = (int32_t)(actor_x - player_x);
    uint32_t abs_x = (uint32_t)(delta_x < 0 ? (int32_t)(player_x - actor_x) : delta_x);
    uint32_t actor_z = ae_rd32(r, actor + AE_MISC_SPECIAL_ACTOR_POSITION_Z);
    uint32_t player_z = ae_rd32(r, player + AE_MISC_PLAYER_POS_Z_S16);
    int32_t delta_z = (int32_t)(actor_z - player_z);
    uint32_t abs_z = (uint32_t)(delta_z < 0 ? (int32_t)(player_z - actor_z) : delta_z);
    if (abs_x >= 65u || abs_z >= 65u) {
        ae_wr8(r, actor + AE_MISC_SPECIAL_ACTOR_NEAR_TIMER, 0u);
        return;
    }

    int32_t timer = ae_rds8(r, actor + AE_MISC_SPECIAL_ACTOR_NEAR_TIMER);
    if (timer >= 3) {
        ae_wr8(r, actor + AE_MISC_SPECIAL_ACTOR_NEAR_TIMER,
               ae_rd8(r, actor + AE_MISC_SPECIAL_ACTOR_NEAR_TIMER) - 1u);
    }
    timer = ae_rds8(r, actor + AE_MISC_SPECIAL_ACTOR_NEAR_TIMER);
    if (timer <= 0) {
        uint32_t player_mode = ae_rd32(r, player + AE_MISC_PLAYER_STATE_MODE) & 0xF0u;
        if (player_mode == 0x40u) {
            ae_wr32(r, player + AE_MISC_SPECIAL_ACTOR_VELOCITY_X, 0u);
            ae_wr32(r, player + AE_MISC_SPECIAL_ACTOR_VELOCITY_Z, 0u);
            return;
        }
        return;
    }
}

/* BgmApplyPlayerEffect @80087654. Program numbers are narrowed to halfwords
 * by the original channel and fade routines. */
int32_t ae_BgmApplyPlayerEffect(AeRam *r) {
    uint32_t track = AE_MISC_SOUND_BGM_TRACK;
    int32_t track_index = ae_rds16(r, track + AE_MISC_SOUND_BGM_TRACK_INDEX);
    if (track_index < 0) return 0;

    int32_t effect_active = ae_rds16(r, track + AE_MISC_SOUND_EFFECT_FLAG);
    uint32_t player_mode = ae_rd32(r, AE_MISC_G_PLAYER + AE_MISC_PLAYER_STATE) & 0xF0u;
    if (effect_active == 0 && player_mode == 0x80u) {
        ae_wr16(r, track + AE_MISC_SOUND_EFFECT_FLAG, 1u);
        for (int32_t channel = 0; channel < (int32_t)AE_MISC_SOUND_CHANNEL_COUNT; ++channel) {
            int32_t program = ae_SeqGetChannelProgram(r, track_index, channel, 0);
            uint32_t offset20 = (uint32_t)((uint16_t)(program - 20));
            if (offset20 < 20u) {
                (void)ae_SeqFadeTrackProgram(r, track_index, channel, 0, 15);
            } else {
                uint32_t offset40 = (uint32_t)((uint16_t)(program - 40));
                if (offset40 < 20u) {
                    (void)ae_SeqFadeTrackProgram(r, track_index, channel,
                                                 ae_rds16(r, track + AE_MISC_SOUND_BGM_VOLUME), 15);
                }
            }
        }
        return 0;
    }

    if (effect_active != 0 && player_mode != 0x80u) {
        ae_wr16(r, track + AE_MISC_SOUND_EFFECT_FLAG, 0u);
        for (int32_t channel = 0; channel < (int32_t)AE_MISC_SOUND_CHANNEL_COUNT; ++channel) {
            int32_t program = ae_SeqGetChannelProgram(r, track_index, channel, 0);
            uint32_t offset20 = (uint32_t)((uint16_t)(program - 20));
            if (offset20 < 20u) {
                (void)ae_SeqFadeTrackProgram(r, track_index, channel,
                                             ae_rds16(r, track + AE_MISC_SOUND_BGM_VOLUME), 10);
            } else {
                uint32_t offset40 = (uint32_t)((uint16_t)(program - 40));
                if (offset40 < 20u) {
                    (void)ae_SeqFadeTrackProgram(r, track_index, channel, 0, 10);
                }
            }
        }
    }
    return 0;
}

/* SoundEmitterAttenuation @80087EE0. It projects a single game-defined
 * distance/range pair into equal left and right attenuation values. */
int32_t ae_SoundEmitterAttenuation(AeRam *r, uint32_t emitter, uint32_t out) {
    ae_wr16(r, out + AE_MISC_SOUND_PAN_RIGHT, 0u);
    ae_wr16(r, out + AE_MISC_SOUND_PAN_LEFT, 0u);

    uint32_t emitter_type = ae_rd16(r, emitter + AE_MISC_SOUND_TYPE) & 0xF000u;
    uint32_t player_mode = ae_rd32(r, AE_MISC_SOUND_ATTENUATION_STATE) & 0xF0u;
    if (player_mode == 0xC0u) return 0;

    int32_t segment_y_delta = (int32_t)(uint32_t)(
        ae_rds16(r, emitter + AE_MISC_SOUND_POSITION_Y) -
        ae_rds16(r, emitter + AE_MISC_SOUND_POSITION2_Y));
    int32_t dx = (int32_t)(uint32_t)(
        ae_rds16(r, AE_MISC_SOUND_LISTENER + AE_MISC_SOUND_LISTENER_Y) - segment_y_delta);
    int32_t dz;
    int32_t maximum;
    if (emitter_type == 0x3000u) {
        dz = (int32_t)(uint32_t)(
            ae_rds16(r, AE_MISC_SOUND_LISTENER + AE_MISC_SOUND_LISTENER_Z) -
            ae_rds16(r, emitter + AE_MISC_SOUND_POSITION_Z));
        maximum = ae_rds16(r, emitter + AE_MISC_SOUND_RANGE_RADIAL);
    } else {
        dz = (int32_t)(uint32_t)(
            ae_rds16(r, AE_MISC_SOUND_LISTENER) -
            ae_rds16(r, emitter + AE_MISC_SOUND_POSITION_X));
        maximum = ae_rds16(r, emitter + AE_MISC_SOUND_RANGE_LINEAR);
    }

    uint32_t squared = (uint32_t)((int64_t)dx * dx + (int64_t)dz * dz);
    int32_t distance = (int32_t)ae_sdk_SquareRoot0(r, squared);
    if (maximum < distance) return 0;

    int32_t attenuation = 4096;
    if (distance > (int32_t)AE_MISC_SOUND_ATTENUATION_MAX) {
        int32_t divisor = maximum - (int32_t)AE_MISC_SOUND_ATTENUATION_MAX;
        if (divisor == 0) {
            ae_ram_fail(r, "SoundEmitterAttenuation divides by zero at max range");
            return 0;
        }
        int32_t numerator = (int32_t)((uint32_t)(maximum - distance) << 12);
        attenuation = numerator / divisor;
    }

    ae_wr16(r, out + AE_MISC_SOUND_PAN_RIGHT, (uint16_t)attenuation);
    ae_wr16(r, out + AE_MISC_SOUND_PAN_LEFT, (uint16_t)attenuation);
    if (attenuation >= 8) return 1;
    return attenuation >= 8 ? 1 : 0;
}

static int32_t misc_signed_divide_48(int32_t value) {
    /* The original uses the signed high word of the reciprocal multiply by
     * 0x2AAAAAAB, followed by a 3-bit correction. */
    int64_t product = (int64_t)value * (int64_t)(int32_t)0x2AAAAAABu;
    int32_t high = (int32_t)(product >> 32);
    return (high >> 3) - (value >> 31);
}

/* SoundEmitterUpdate @8008686C. Slots are processed in ascending order, and
 * stop/start requests precede pan-volume writes exactly as in the game. */
int32_t ae_SoundEmitterUpdate(AeRam *r, const AeMiscHooks *h) {
    uint32_t stack = h != NULL && h->stack_pointer != 0u ?
                     h->stack_pointer : AE_MISC_DEFAULT_STACK_POINTER;
    uint32_t pan = stack - 56u;
    uint32_t point = stack - 48u;
    uint32_t state = AE_MISC_G_STAGE_STATE;

    uint32_t camera_mode = ae_rd8(r, state + AE_MISC_STAGE_SUBMODE);
    if (camera_mode == 24u || ae_rd8(r, AE_MISC_SOUND_GLOBAL_MODE) == 1u) {
        ae_wr32(r, AE_MISC_SOUND_LISTENER,
                ae_rd32(r, AE_MISC_SOUND_LISTENER_CAMERA));
        ae_wr32(r, AE_MISC_SOUND_LISTENER + AE_MISC_SOUND_LISTENER_Z,
                ae_rd32(r, AE_MISC_SOUND_LISTENER_CAMERA + AE_MISC_VECTOR32_Y));
    } else {
        ae_wr16(r, AE_MISC_SOUND_LISTENER,
                ae_rd16(r, AE_MISC_G_PLAYER + AE_MISC_PLAYER_POS_X_S16));
        ae_wr16(r, AE_MISC_SOUND_LISTENER + AE_MISC_SOUND_LISTENER_Y,
                ae_rd16(r, AE_MISC_G_PLAYER + AE_MISC_PLAYER_POS_Y_S16));
        ae_wr16(r, AE_MISC_SOUND_LISTENER + AE_MISC_SOUND_LISTENER_Z,
                ae_rd16(r, AE_MISC_G_PLAYER + AE_MISC_PLAYER_POS_Z_S16));
    }

    for (uint32_t index = 0; index < AE_MISC_SOUND_RECORD_COUNT; ++index) {
        uint32_t emitter = AE_SOUND_LOOPS + index * AE_MISC_SOUND_RECORD_SIZE;
        int32_t packed_type = ae_rds16(r, emitter + AE_MISC_SOUND_TYPE);
        if (packed_type < 0) continue;
        uint32_t type = (uint32_t)packed_type & 0xF000u;
        int32_t audible = 0;
        int32_t forced_pan_update = 0;

        if (type == 0x2000u) {
            audible = ae_SoundEmitterCalcPan(r, emitter + AE_MISC_SOUND_POSITION_X,
                                             ae_rds16(r, emitter + AE_MISC_SOUND_PARAMETER), pan);
        } else if (type == 0x3000u || type == 0x4000u) {
            audible = ae_SoundEmitterAttenuation(r, emitter, pan);
        } else if (type == 0x6000u) {
            if ((ae_rd32(r, AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_FLAGS) & 2u) != 0u) {
                ae_wr16(r, pan, 0u);
                ae_wr16(r, pan + AE_MISC_SOUND_PAN_RIGHT, 0u);
            } else {
                int32_t actor_index = ae_rds16(r, emitter + AE_MISC_SOUND_ACTOR_INDEX);
                uint32_t actor = AE_MISC_SPECIAL_ACTOR_TABLE +
                                 (uint32_t)actor_index * AE_MISC_SPECIAL_ACTOR_SIZE;
                ae_wr16(r, point, ae_rd16(r, actor + AE_MISC_SPECIAL_ACTOR_POSITION_X));
                ae_wr16(r, point + AE_MISC_SOUND_POINT_Y,
                        ae_rd16(r, actor + AE_MISC_SPECIAL_ACTOR_POSITION_Y));
                ae_wr16(r, point + AE_MISC_SOUND_POINT_Z,
                        ae_rd16(r, actor + AE_MISC_SPECIAL_ACTOR_POSITION_Z));
                (void)ae_SoundEmitterCalcPan(r, point, 0x2000, pan);
            }
            forced_pan_update = 1;
        } else if (type == 0x7000u) {
            if ((ae_rd32(r, AE_MISC_G_GAME_STATE + AE_MISC_GAME_STATE_FLAGS) & 4u) != 0u) {
                ae_wr16(r, pan, 0u);
                ae_wr16(r, pan + AE_MISC_SOUND_PAN_RIGHT, 0u);
                forced_pan_update = 1;
            } else {
                uint32_t position = ae_rd32(r, emitter + AE_MISC_SOUND_POSITION_POINTER);
                ae_wr16(r, point + AE_MISC_SOUND_POINT_X,
                        ae_rd16(r, position + AE_MISC_VECTOR32_X));
                ae_wr16(r, point + AE_MISC_SOUND_POINT_Y,
                        ae_rd16(r, position + AE_MISC_VECTOR32_Y));
                ae_wr16(r, point + AE_MISC_SOUND_POINT_Z,
                        ae_rd16(r, position + AE_MISC_VECTOR32_Z));
                audible = ae_SoundEmitterCalcPan(r, point, 0x2000, pan);
            }
        } else if (type == 0x1000u) {
            if (ae_rd16(r, state + AE_MISC_STAGE_TRACK_STATE) == 0u) continue;
            ae_wr16(r, pan, (uint16_t)type);
            ae_wr16(r, pan + AE_MISC_SOUND_PAN_RIGHT, (uint16_t)type);
            forced_pan_update = 1;
        } else {
            continue;
        }

        if (forced_pan_update == 0) {
            if (audible == 0) {
                if (ae_rds16(r, emitter + AE_MISC_SOUND_ACTIVE) == 0) continue;
                (void)misc_call(r, h, h != NULL ? h->SoundLoopStop : NULL,
                                "SoundLoopStop",
                                (uint32_t)ae_rds16(r, emitter + AE_MISC_SOUND_ID),
                                (uint32_t)ae_rds16(r, emitter + AE_MISC_SOUND_HANDLE), 0u, 0u);
                ae_wr16(r, emitter + AE_MISC_SOUND_ACTIVE, 0u);
                ae_wr16(r, emitter + AE_MISC_SOUND_HANDLE, 0xFFFFu);
                continue;
            }

            if (ae_rds16(r, emitter + AE_MISC_SOUND_ACTIVE) == 0) {
                int32_t voice = ae_SoundLoopStart(r, h != NULL ? h->sound_hw : NULL, 1,
                                                  ae_rds16(r, emitter + AE_MISC_SOUND_ID));
                ae_wr16(r, emitter + AE_MISC_SOUND_HANDLE, (uint16_t)voice);
                if (voice < 0) {
                    ae_wr16(r, emitter + AE_MISC_SOUND_ACTIVE, 0u);
                    continue;
                }
                ae_wr16(r, emitter + AE_MISC_SOUND_ACTIVE, 1u);
                continue;
            }
        }

        if (ae_rd16(r, state + AE_MISC_STAGE_TRACK_STATE) >= 1u &&
            ae_rd16(r, state + AE_MISC_STAGE_TRACK_STATE) <= 4u) {
            int32_t factor = 48 - ae_rds16(r, state + AE_MISC_STAGE_TRACK_MIX);
            int32_t left = misc_signed_divide_48(factor * ae_rds16(r, pan));
            int32_t right = misc_signed_divide_48(
                factor * ae_rds16(r, pan + AE_MISC_SOUND_PAN_RIGHT));
            ae_wr16(r, pan, (uint16_t)left);
            ae_wr16(r, pan + AE_MISC_SOUND_PAN_RIGHT, (uint16_t)right);
        }
        ae_SfxVoiceSetPanVol(r, h, ae_rds16(r, emitter + AE_MISC_SOUND_HANDLE),
                             ae_rds16(r, pan + AE_MISC_SOUND_PAN_LEFT),
                             ae_rds16(r, pan + AE_MISC_SOUND_PAN_RIGHT));
    }
    return ae_BgmApplyPlayerEffect(r);
}

static int misc_copy_ram(AeRam *r, uint32_t destination, uint32_t source,
                         uint32_t byte_count) {
    uint8_t *destination_bytes = ae_ptr(r, destination, byte_count);
    uint8_t *source_bytes = ae_ptr(r, source, byte_count);
    if (destination_bytes == NULL || source_bytes == NULL) return 0;
    memmove(destination_bytes, source_bytes, byte_count);
    return 1;
}

/* ColliderScriptRun @8008B7F8. Commands are 16-byte entries indexed through
 * the collision script list. Only command kinds 3 and 5 have active handlers. */
int32_t ae_ColliderScriptRun(AeRam *r, const AeMiscHooks *h,
                             uint32_t collider, int32_t index) {
    int32_t script_cursor = ae_rds16(r, collider + AE_MISC_COLLIDER_SCRIPT_CURSOR);
    if (script_cursor < 0) return 0;

    uint32_t player = AE_MISC_G_PLAYER;
    int32_t dx = (int32_t)(ae_rd32(r, player + AE_MISC_PLAYER_POS_X_S16) -
                           (uint32_t)ae_rds16(r, collider + AE_MISC_COLLIDER_POSITION_X));
    int32_t dy = (int32_t)(ae_rd32(r, player + AE_MISC_PLAYER_POS_Y_S16) -
                           (uint32_t)ae_rds16(r, collider + AE_MISC_COLLIDER_POSITION_Y));
    int32_t dz = (int32_t)(ae_rd32(r, player + AE_MISC_PLAYER_POS_Z_S16) -
                           (uint32_t)ae_rds16(r, collider + AE_MISC_COLLIDER_POSITION_Z));
    int32_t distance = ae_player_vec_length3(dx, dy, dz);
    ae_wr16(r, collider + AE_MISC_COLLIDER_SCRIPT_KIND,
            ae_rd16(r, collider + AE_MISC_COLLIDER_SCRIPT_KIND) & 0xFEFFu);

    uint32_t command_list = ae_rd32(r, AE_MISC_COLLIDER_SCRIPT_STATE +
                                       AE_MISC_COLLIDER_SCRIPT_LIST_POINTER);
    uint32_t command = command_list + (uint32_t)script_cursor *
                       AE_MISC_COLLIDER_SCRIPT_COMMAND_SIZE;
    int32_t command_index = ae_rds16(r, command + AE_MISC_COLLIDER_SCRIPT_COMMAND_TYPE);
    while (command_index >= 0 && command_index < 8) {
        uint32_t action = ae_rd32(r, AE_MISC_COLLIDER_SCRIPT_COMMAND_TABLE +
                                    (uint32_t)command_index * 4u);
        if (action == AE_MISC_COLLIDER_SCRIPT_MOTION_ENTRY) {
            (void)ae_ColliderScriptCommand(r, h, collider, command, distance,
                                           misc_s16((uint32_t)index));
        } else if (action == AE_MISC_COLLIDER_SCRIPT_UPDATE_ENTRY &&
                   ae_rds16(r, command + AE_MISC_COLLIDER_SCRIPT_COMMAND_MAGIC) == 80) {
            int32_t owner = ae_rds16(r, command + AE_MISC_COLLIDER_SCRIPT_COMMAND_INDEX);
            int32_t field = ae_rds16(r, command + AE_MISC_COLLIDER_SCRIPT_COMMAND_FIELD);
            if (owner >= 0 && field >= 0) {
                uint32_t records = ae_rd32(r, AE_MISC_COLLIDER_SCRIPT_STATE +
                                              AE_MISC_COLLIDER_SCRIPT_DATA_POINTER);
                uint32_t record = records + (uint32_t)owner * AE_MISC_COLLIDER_SCRIPT_DATA_STRIDE;
                uint32_t output = ae_rd32(r, AE_MISC_COLLIDER_SCRIPT_STATE +
                                             AE_MISC_COLLIDER_SCRIPT_OUTPUT_POINTER) +
                                  (uint32_t)field * AE_MISC_COLLIDER_SCRIPT_OUTPUT_STRIDE;
                uint32_t value_offset =
                    (ae_rd16(r, record + AE_MISC_COLLIDER_SCRIPT_KIND) & 0xFu) == 1u ?
                    AE_MISC_COLLIDER_SCRIPT_COMMAND_VALUE_A :
                    AE_MISC_COLLIDER_SCRIPT_COMMAND_VALUE_B;
                ae_wr16(r, output + AE_MISC_COLLIDER_SCRIPT_OUTPUT_VALUE,
                        ae_rd16(r, command + value_offset));
            }
        }

        int32_t next = ae_rds16(r, command + AE_MISC_COLLIDER_SCRIPT_COMMAND_NEXT);
        if (next < 0) break;
        command = command_list + (uint32_t)next * AE_MISC_COLLIDER_SCRIPT_COMMAND_SIZE;
        command_index = ae_rds16(r, command + AE_MISC_COLLIDER_SCRIPT_COMMAND_TYPE);
    }
    return 0;
}

/* ColliderScriptsUpdate @80088AB8. The original stages each nontrivial record
 * through a shared 0x38-byte work buffer, calls its runtime handler, executes
 * the record's command list, then copies the edited record back. */
int32_t ae_ColliderScriptsUpdate(AeRam *r, const AeMiscHooks *h) {
    uint32_t state = AE_MISC_G_GAME_STATE;
    if ((ae_rd32(r, state + AE_MISC_GAME_STATE_FLAGS) & 8u) != 0u) return 0;

    int32_t count = ae_rds16(r, state + AE_MISC_COLLIDER_SCRIPT_COUNT_OFFSET);
    uint32_t records = ae_rd32(r, state + AE_MISC_COLLIDER_SCRIPT_DATA_POINTER);
    uint32_t work = ae_rd32(r, AE_MISC_COLLIDER_SCRIPT_BUFFER_POINTER);
    for (int32_t index = count - 1; index >= 0; --index) {
        uint32_t record = records + (uint32_t)index * AE_MISC_COLLIDER_SCRIPT_DATA_STRIDE;
        uint32_t collider_class = ae_rd16(r, record + AE_MISC_COLLIDER_SCRIPT_KIND) & 0xFu;
        ae_wr32(r, AE_MISC_G_COLLIDER_SCRIPT_CURRENT_INDEX, (uint32_t)index);
        if (collider_class == 3u) continue;
        if (collider_class == 1u) {
            if (ae_rds16(r, record + AE_MISC_COLLIDER_SCRIPT_LENGTH) >= 0) {
                (void)ae_ColliderScriptRun(r, h, record, index);
            }
            continue;
        }

        if (!misc_copy_ram(r, work, record, AE_MISC_COLLIDER_SCRIPT_BUFFER_BYTES)) return 0;
        uint32_t staged_kind = ae_rd8(r, work + AE_MISC_COLLIDER_SCRIPT_STAGED_KIND);
        if (staged_kind < 2u) {
            ae_wr16(r, state + AE_MISC_COLLIDER_SCRIPT_CURRENT_INDEX, (uint16_t)index);
        }

        uint32_t handler_index = ae_rd8(r, work + AE_MISC_COLLIDER_SCRIPT_STAGED_KIND);
        uint32_t callback = ae_rd32(r, AE_MISC_COLLIDER_SCRIPT_FUNCTION_TABLE + handler_index * 4u);
        switch (callback) {
        case 0x800892C4u:
            (void)ae_ColliderScriptFollowPath(r, h, work, index);
            break;
        case 0x8008A4B0u:
            (void)ae_ColliderScriptPlayerMove(r, h, work, index);
            break;
        case 0x8008A91Cu:
            (void)ae_ColliderScriptCarry(r, h, work, index);
            break;
        case 0x80088D78u:
            (void)misc_call(r, h, h != NULL ? h->FUN_80088d78 : NULL,
                            "FUN_80088d78", work, (uint32_t)index, 0, 0);
            break;
        case 0x80088F2Cu:
            (void)misc_call(r, h, h != NULL ? h->FUN_80088f2c : NULL,
                            "FUN_80088f2c", work, (uint32_t)index, 0, 0);
            break;
        case 0x800895F0u:
            (void)misc_call(r, h, h != NULL ? h->FUN_800895f0 : NULL,
                            "FUN_800895f0", work, (uint32_t)index, 0, 0);
            break;
        case 0x80089B10u:
            (void)misc_call(r, h, h != NULL ? h->FUN_80089b10 : NULL,
                            "FUN_80089b10", work, (uint32_t)index, 0, 0);
            break;
        case 0x80089D64u:
            (void)misc_call(r, h, h != NULL ? h->FUN_80089d64 : NULL,
                            "FUN_80089d64", work, (uint32_t)index, 0, 0);
            break;
        case 0x80089D6Cu:
            (void)misc_call(r, h, h != NULL ? h->FUN_80089d6c : NULL,
                            "FUN_80089d6c", work, (uint32_t)index, 0, 0);
            break;
        case 0x8008AA64u:
            (void)misc_call(r, h, h != NULL ? h->FUN_8008aa64 : NULL,
                            "FUN_8008aa64", work, (uint32_t)index, 0, 0);
            break;
        case 0x8013FE4Cu:
            (void)misc_call(r, h, h != NULL ? h->FUN_8013fe4c : NULL,
                            "FUN_8013fe4c", work, (uint32_t)index, 0, 0);
            break;
        case 0x800B3000u:
            (void)misc_call(r, h, h != NULL ? h->ColliderScriptUpdate : NULL,
                            "ColliderScriptUpdate", work, (uint32_t)index, 0, 0);
            break;
        default:
            break;
        }
        (void)ae_ColliderScriptRun(r, h, work, index);
        if (!misc_copy_ram(r, record, work, AE_MISC_COLLIDER_SCRIPT_BUFFER_BYTES)) return 0;
    }
    return 0;
}

