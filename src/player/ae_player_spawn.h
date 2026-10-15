#ifndef AE_PLAYER_SPAWN_H
#define AE_PLAYER_SPAWN_H

#include "../core/ae_ram.h"
#include "../core/ae_sdk.h"
#include "../bodies/ae_bodies.h"
#include "../sound/ae_sound.h"
#include "../camera/ae_camera.h"
#include "../core/ae_pad.h"
#include "../world/ae_world.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Player/area fields consumed by the original entry spawn path. */
#define AE_SPAWN_G_STAGE_STATE              0x800F4350u
#define AE_SPAWN_G_PLAYER                   0x800EC1E0u
#define AE_SPAWN_G_CAMERA_SLOTS             0x800B4D40u
#define AE_SPAWN_G_CAMERA_RADAR_TARGET       0x800B4D44u
#define AE_SPAWN_G_CAMERA_STATE             0x800E68D0u
#define AE_SPAWN_G_STAGE_EXIT_FLAGS         0x800B8CC0u
#define AE_SPAWN_G_PLAYER_BONE_BUFFER       0x800E9C60u
#define AE_SPAWN_G_CAMERA_SHAKE              0x800E6950u
#define AE_SPAWN_G_CAMERA_NAME_ERROR         0x800A4CC0u
#define AE_SPAWN_G_CAMERA_FALLBACK_DATA      0x800BF120u

#define AE_SPAWN_STATE_CURRENT_AREA          0x126u
#define AE_SPAWN_STATE_ENTRY_INDEX           0x12Au
#define AE_SPAWN_STATE_ENTRY_TABLE           0x234u
#define AE_SPAWN_STATE_CAMERA_DATA           0x250u
#define AE_SPAWN_STATE_CAMERA_SCRIPTS        0x254u
#define AE_SPAWN_STATE_CAMERA_EYE            0x258u
#define AE_SPAWN_STATE_CAMERA_TARGET         0x260u
#define AE_SPAWN_STATE_PLAYER_DATA           0x1A0u

#define AE_SPAWN_PLAYER_POSITION_X           0x24u
#define AE_SPAWN_PLAYER_POSITION_Y           0x28u
#define AE_SPAWN_PLAYER_POSITION_Z           0x2Cu
#define AE_SPAWN_PLAYER_VELOCITY_X           0x30u
#define AE_SPAWN_PLAYER_VELOCITY_Z           0x38u
#define AE_SPAWN_PLAYER_FACING_X             0x60u
#define AE_SPAWN_PLAYER_BODY_RADIUS           0x44u
#define AE_SPAWN_PLAYER_BODY_TYPE             0x48u
#define AE_SPAWN_PLAYER_BODY_FLAGS            0x4Au
#define AE_SPAWN_PLAYER_UNKNOWN_58             0x58u
#define AE_SPAWN_PLAYER_ANIM_FRAME             0x5Cu
#define AE_SPAWN_PLAYER_STATE                  0x5Eu
#define AE_SPAWN_PLAYER_STATE_FLAGS            0x70u
#define AE_SPAWN_PLAYER_UP_X                   0x68u
#define AE_SPAWN_PLAYER_UP_Y                   0x6Au
#define AE_SPAWN_PLAYER_UP_Z                   0x6Cu
#define AE_SPAWN_PLAYER_ANIM_LERP              0x8Cu
#define AE_SPAWN_PLAYER_UNKNOWN_102            0x102u
#define AE_SPAWN_PLAYER_UNKNOWN_108            0x108u
#define AE_SPAWN_PLAYER_UNKNOWN_110            0x110u
#define AE_SPAWN_PLAYER_UNKNOWN_114            0x114u
#define AE_SPAWN_PLAYER_UNKNOWN_11C            0x11Cu
#define AE_SPAWN_PLAYER_GROUND_HEIGHT          0x12Cu
#define AE_SPAWN_PLAYER_POSITION_X_LOW         0x1Cu
#define AE_SPAWN_PLAYER_POSITION_Y_LOW         0x1Eu
#define AE_SPAWN_PLAYER_POSITION_Z_LOW         0x20u
#define AE_SPAWN_PLAYER_DATA_POINTER            0x18u
#define AE_SPAWN_PLAYER_BONE_BUFFER             0x14u

#define AE_SPAWN_ENTRY_POSITION                0x00u
#define AE_SPAWN_ENTRY_POSITION_Y              0x04u
#define AE_SPAWN_ENTRY_POSITION_Z              0x08u
#define AE_SPAWN_ENTRY_CAMERA                  0x0Cu
#define AE_SPAWN_ENTRY_CAMERA_Y                0x10u
#define AE_SPAWN_ENTRY_CAMERA_Z                0x14u
#define AE_SPAWN_ENTRY_FACING                  0x18u
#define AE_SPAWN_ENTRY_FACING_Y                0x1Cu
#define AE_SPAWN_ENTRY_FACING_Z                0x20u
#define AE_SPAWN_ENTRY_SIZE                    0x24u
#define AE_SPAWN_ENTRY_WORD_SIZE               4u
#define AE_SPAWN_PACKED_VECTOR_COMPONENT_SIZE  2u

/* CAM section fields used by CameraResetAll and EventCamSetScript. */
#define AE_SPAWN_CAM_MAGIC                     0x00u
#define AE_SPAWN_CAM_MAGIC_SUFFIX_OFFSET       0x02u
#define AE_SPAWN_CAM_REGION_COUNT              0x04u
#define AE_SPAWN_CAM_UNKNOWN_HEADER_FIELD       0x06u
#define AE_SPAWN_CAM_SCRIPT_GROUP_COUNT        0x08u
#define AE_SPAWN_CAM_SCRIPT_GROUP_B_OFFSET     0x0Au
#define AE_SPAWN_CAM_SCRIPT_GROUP_A_OFFSET     0x0Cu
#define AE_SPAWN_CAM_NAME_TABLE_OFFSET         0x0Eu
#define AE_SPAWN_CAM_HEADER_SIZE               0x10u
#define AE_SPAWN_CAM_REGION_SIZE               0x30u
#define AE_SPAWN_CAM_REGION_NAME_INDEX         0x00u
#define AE_SPAWN_CAM_REGION_FLAGS              0x02u
#define AE_SPAWN_CAM_REGION_EYE                0x04u
#define AE_SPAWN_CAM_REGION_NAME               0x10u
#define AE_SPAWN_CAM_NAME_COUNT                0x00u
#define AE_SPAWN_CAM_NAME_ENTRIES              0x08u
#define AE_SPAWN_CAM_SCRIPT_HEADER_SIZE        0x08u
#define AE_SPAWN_CAM_NAME_ENTRY_SIZE           0x28u
#define AE_SPAWN_CAM_NAME_FLAGS                0x00u
#define AE_SPAWN_CAM_NAME_SCRIPT_OFFSET        0x02u
#define AE_SPAWN_CAM_NAME_BYTES                0x08u
#define AE_SPAWN_CAM_NAME_LENGTH               0x20u

#define AE_SPAWN_CAMERA_STATE_SIZE             0x7Cu
#define AE_SPAWN_CAMERA_PROJECTION              0x2Cu
#define AE_SPAWN_CAMERA_PRIORITY                0x04u
#define AE_SPAWN_CAMERA_ACTIVE_KIND             0x06u
#define AE_SPAWN_CAMERA_ACTIVE_SLOT             0x08u
#define AE_SPAWN_CAMERA_AIM_MODE                0x46u
#define AE_SPAWN_CAMERA_REGION_COUNT            0x00u
#define AE_SPAWN_CAMERA_SLOT_COUNT              0x02u
#define AE_SPAWN_CAMERA_ACTIVE_INDEX            0x0Eu
#define AE_SPAWN_CAMERA_FOLLOW_INDEX            0x18u
#define AE_SPAWN_CAMERA_AIM_INDEX               0x19u
#define AE_SPAWN_CAMERA_FIXED_INDEX             0x1Au
#define AE_SPAWN_CAMERA_SCRIPT_BYTES            0x50u
#define AE_SPAWN_CAMERA_NAME_TABLE              0x54u
#define AE_SPAWN_CAMERA_SCRIPT_INDEXES          0x58u
#define AE_SPAWN_CAMERA_NAME_ENTRIES            0x5Cu
#define AE_SPAWN_CAMERA_SCRIPT_GROUP_A           0x60u
#define AE_SPAWN_CAMERA_SCRIPT_GROUP_B           0x64u
#define AE_SPAWN_CAMERA_SCRIPT_COUNT            0x68u
#define AE_SPAWN_CAMERA_UNKNOWN_6A              0x6Au
#define AE_SPAWN_CAMERA_REGIONS                  0x4Cu

#define AE_SPAWN_CAMERA_SLOT_SIZE               0xB0u
#define AE_SPAWN_CAMERA_SLOT_COUNT_VALUE        12u
#define AE_SPAWN_CAMERA_SLOT_KIND               0x00u
#define AE_SPAWN_CAMERA_SLOT_SCRIPT_POINTER     0x84u
#define AE_SPAWN_CAMERA_SLOT_SCRIPT_TIMER       0x88u
#define AE_SPAWN_CAMERA_SLOT_SCRIPT_STEP        0x8Au
#define AE_SPAWN_CAMERA_SLOT_SCRIPT_GROUP       0x8Eu

#define AE_SPAWN_STAGE_EXIT_FLAGS               0x20u
#define AE_SPAWN_STAGE_EXIT_PENDING              0x24u
#define AE_SPAWN_SHAKE_COUNTER                   0x08u
#define AE_SPAWN_SHAKE_ACTIVE                    0x09u
#define AE_SPAWN_CAMERA_REGION_SENTINEL          0xFFu
#define AE_SPAWN_CAMERA_NAME_MAGIC               0x4143u
#define AE_SPAWN_CAMERA_NAME_MAGIC_SUFFIX        0x4Du
#define AE_SPAWN_CAMERA_VALID_SIGNATURE          0x4Du
#define AE_SPAWN_CAMERA_MARKED_SIGNATURE         0x78u
#define AE_SPAWN_CAMERA_INITIAL_PROJECTION       0x148u
#define AE_SPAWN_CAMERA_AREA_ALLOC_BYTES         0x840u
#define AE_SPAWN_CAMERA_REGION_KIND_MASK         0x6000u
#define AE_SPAWN_CAMERA_REGION_KIND_AREA         0x2000u
#define AE_SPAWN_CAMERA_REGION_FLAGS_MASK        0xFFF0u
#define AE_SPAWN_CAMERA_REGION_SLOT_MASK         0x000Fu
#define AE_SPAWN_CAMERA_TEMP_EYE                  24u
#define AE_SPAWN_CAMERA_TEMP_ROLL                 16u
#define AE_SPAWN_CAMERA_FRAME_SIZE                72u
#define AE_SPAWN_PLAYER_FRAME_SIZE                80u
#define AE_SPAWN_STACK_ENTRY_OFFSET               16u
#define AE_SPAWN_STACK_FACING_OFFSET              56u

/* Dependencies that are still outside the requested 11-function port. The
 * camera callback is the full CameraLookAt (0x80046670), which currently has
 * only its g_CameraRotMatrix component validated in the player-input module. */
typedef struct AePlayerSpawnHooks {
    void *user;
    uint32_t stack_pointer; /* MIPS entry SP used for stack-backed pointer arguments. */
    const AeWorldHooks *world;
    const AeBodyHooks *bodies;
    const AeCameraHooks *camera;
    const AeSoundHw *sound;
    uint32_t (*AreaBumpAlloc)(void *user, AeRam *r, uint32_t bytes); /* 8008C1B8 */
    int32_t (*printf)(void *user, AeRam *r, uint32_t format,
                      uint32_t a1, uint32_t a2, uint32_t a3); /* 800A3720 */
    void (*CameraLookAt)(void *user, AeRam *r, uint32_t eye, uint32_t target,
                         uint32_t view_matrix, uint32_t rotation_matrix,
                         int32_t roll); /* 80046670, full native/render path */
    void (*SetGeomScreen)(void *user, AeRam *r, uint32_t distance); /* public SDK API */
} AePlayerSpawnHooks;

int32_t ae_AreaGetEntryPoint(AeRam *r, int32_t area, int32_t entry, uint32_t out); /* 80010C3C */
int32_t ae_PlayerSpawnAtEntry(AeRam *r, const AePlayerSpawnHooks *h); /* 80032BBC */
void ae_SetProjection(AeRam *r, const AePlayerSpawnHooks *h, int32_t distance); /* 80046D80 */
void ae_CameraActivateSlot(AeRam *r, uint32_t index); /* 80046DAC */
void ae_CameraResetAll(AeRam *r, const AePlayerSpawnHooks *h); /* 80047754 */
void ae_CameraShakeReset(AeRam *r); /* 800489EC */
void ae_AimModeLeave(AeRam *r, const AePlayerSpawnHooks *h); /* 8004B510 */
void ae_EventCamSetScript(AeRam *r, uint32_t slot_index, int32_t script_index,
                          uint32_t group, uint32_t script_set); /* 8004BAD8 */
void ae_StageExitFlagsClear(AeRam *r); /* 8004D0F0 */
int32_t ae_PlaySfx(AeRam *r, const AeSoundHw *hw, int32_t id); /* 800877F8 */

#ifdef __cplusplus
}
#endif
#endif
