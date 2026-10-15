#include "ae_player2.h"

#include "ae_player_act.h"
#include "ae_gadget.h"
#include "native_player.h"
#include "../core/ae_pose.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../effects/ae_spawn.h"
#include "../monkeys/ae_monkey.h"
#undef AE_PLAYER_FACING_X
#undef AE_PLAYER_FACING_Z
#include "ae_player_spawn.h"
#include "../world/ae_areaset.h"
#include "../world/ae_solid.h"

#include <limits.h>
#include <stdint.h>

typedef struct AeCameraHooks AeCameraHooks;
void ae_CameraSlotFlagBit81(AeRam *r, const AeCameraHooks *hooks);

#define AE_P2_G_PLAYER                 0x800EC1E0u
#define AE_P2_G_PLAYER_STATE_SETS      0x800F4DE0u
#define AE_P2_G_STICK_TRAVEL           0x800E54DCu
#define AE_P2_G_PLAYER_SCRATCH_STATE_TABLE 0x1F800048u
#define AE_P2_G_PLAYER_SCRATCH_TRANSITIONS 0x1F80004Cu
#define AE_P2_G_PLAYER_SCRATCH_EVENTS  0x1F800050u
#define AE_P2_G_PLAYER_HURT_BLINK      0x800F4DCEu
#define AE_P2_G_PLAYER_AIR             0x800F4DC8u
#define AE_P2_G_AIR_WARNING_PENDING    0x800F51E4u
#define AE_P2_G_PLAYER_BLINK_FX        0x800BAD90u
#define AE_P2_G_AIM_TABLE_LAST         0x800F50A0u
#define AE_P2_G_GADGET4_STATE          0x800F5120u
#define AE_P2_G_PAUSE_MENU_MODE        0x800BEF74u
#define AE_P2_G_SOUND_MONO             0x800F75F2u
#define AE_P2_G_SPU_SHADOW             0x800E1874u
#define AE_P2_G_REVERB_VOLUME          0x800BF67Eu
#define AE_P2_G_MAIN_VOLUME             0x800BF680u
#define AE_P2_G_RC_CAR                  0x800F5A20u
#define AE_P2_G_RANDOM_PARTICLES         0x800BB840u
#define AE_P2_G_RANDOM_PARTICLE_CURSOR  0x800B205Cu
#define AE_P2_G_RANDOM_PARTICLE_TABLE   0x800B2054u
#define AE_P2_G_SPLASH_FADE             0x800BAD78u
#define AE_P2_G_SPLASH_STAGE            0x800BAD79u
#define AE_P2_G_SPLASH_SURFACE           0x800BAD7Cu
#define AE_P2_G_WATER_PARTICLE_HEIGHT    0x800B4D90u
#define AE_P2_G_PLAYER_CONDITIONS        0x800B0298u
#define AE_P2_G_PLAYER_ACTIONS           0x800B03ACu
#define AE_P2_G_PLAYER_FX_POSITION       0x800B4D88u
#define AE_P2_G_SQUASH_PARTICLE_POSITION 0x800BAD60u
#define AE_P2_G_PLAYER_SQUASH_FRAME      0x800EC2E0u
#define AE_P2_SQUASH_EFFECT_RING         0x800B1A8Cu
#define AE_P2_SQUASH_EFFECT_BURST        0x800B1A38u
#define AE_P2_SQUASH_EFFECT_TRAIL        0x800B1C28u
#define AE_P2_SQUASH_EFFECT_FINAL        0x800B1990u
#define AE_P2_LOCAL_SQUASH_DIRECTION     0x801FFED8u
#define AE_P2_SCRATCH_SURFACE_TOP        0x1F8003DEu
#define AE_P2_SCRATCH_SURFACE_BOTTOM     0x1F8003E4u
#define AE_P2_LOCAL_SPLASH               0x801FF140u
#define AE_P2_LOCAL_SQUASH_SCALE         0x801FFEE0u
#define AE_P2_NESTED_SQUASH_SCALE        0x801FFEC8u
#define AE_P2_NESTED_SQUASH_DIRECTION   0x801FFEC0u
#define AE_P2_SCREEN_VERTICES            0x800E7D00u
#define AE_P2_G_ORDERING_TABLE           0x800EC358u
#define AE_P2_G_DRAW_BUFFER_INDEX        0x800EC35Cu
#define AE_P2_G_IDLE_ANIMATION           0x800B4D70u
#define AE_P2_G_IDLE_RIBBON_START        0x800E7F68u
#define AE_P2_G_IDLE_RIBBON_END          0x800E7F70u
#define AE_P2_G_IDLE_MODEL_DRAW_BASE     0x800EC3D0u
#define AE_P2_G_IDLE_MODEL_DRAW_STRIDE   0x4078u
#define AE_P2_G_IDLE_FRAME_COUNTER       0x800EC350u
#define AE_P2_G_GADGET3_DELAY            0x800F51BAu
#define AE_P2_G_GADGET3_REPEAT           0x800F51BCu
#define AE_P2_G_GADGET3_WAIT             0x800F51B8u
#define AE_P2_G_GADGET3_FRAME             0x800F51C0u
#define AE_P2_G_GADGET3_FLAG              0x800F5144u
#define AE_P2_G_GADGET4_COUNTER            0x800F5124u
#define AE_P2_G_GADGET4_PHASE              0x800F5128u
#define AE_P2_G_GADGET4_RATE               0x800F5126u
#define AE_P2_G_GADGET4_HEIGHT             0x800F512Cu
#define AE_P2_G_GADGET4_DISTANCE            0x800F5130u
#define AE_P2_G_GADGET4_FLAGS               0x800F5134u
#define AE_P2_G_GADGET4_FLAGS_2             0x800F5138u
#define AE_P2_G_GADGET4_TRAIL_A              0x800F5140u
#define AE_P2_G_GADGET4_TRAIL_B              0x800F5142u

#define AE_P2_PLAYER_POSITION_X         0x024u
#define AE_P2_PLAYER_DESIRED_DIRECTION  0x074u
#define AE_P2_PLAYER_CLIMB_TARGET       0x0B4u
#define AE_P2_PLAYER_ROPE               0x0C0u
#define AE_P2_PLAYER_ROPE_LENGTH_SQRT   0x0CEu
#define AE_P2_PLAYER_ROPE_NORMAL_X      0x0D0u
#define AE_P2_PLAYER_ROPE_NORMAL_Z      0x0D4u
#define AE_P2_PLAYER_CLIMB_TARGET_HEIGHT 0x0BAu
#define AE_P2_PLAYER_CLIMB_TARGET_FLAGS  0x0BCu
#define AE_P2_PLAYER_CLIMB_TARGET_Y_OFFSET 0x0BEu
#define AE_P2_ROPE_ENDPOINT_B             0x006u
#define AE_P2_ROPE_FRACTION               0x00Cu
#define AE_P2_ROPE_AXIS_STRIDE            0x002u
#define AE_P2_ROPE_FRACTION_SCALE         0x1000
#define AE_P2_ROPE_GESTURE_SHIFT          15u
#define AE_P2_PLAYER_ROPE_FRACTION        0x0CCu
#define AE_P2_PLAYER_BONE_MATRIX_TRANSLATION_X 0x014u
#define AE_P2_PLAYER_BONE_MATRIX_TRANSLATION_Y 0x018u
#define AE_P2_PLAYER_BONE_MATRIX_TRANSLATION_Z 0x01Cu
#define AE_P2_SCRATCH_INPUT_BLOCK         0x080u
#define AE_P2_SCRATCH_HELD_BUTTONS        0x054u
#define AE_P2_SCRATCH_CHANGED_BUTTONS     0x058u
#define AE_P2_SCRATCH_ROPE_GESTURE        0x064u
#define AE_P2_PLAYER_ATTACH_FLAGS       0x0E0u
#define AE_P2_PLAYER_HURT               0x0F0u
#define AE_P2_PLAYER_CONTACT_FRAMES     0x0EFu
#define AE_P2_PLAYER_CONTACT_COOLDOWN   0x0F1u
#define AE_P2_PLAYER_SQUASH_TIMER       0x0FEu
#define AE_P2_PLAYER_INVULN_TIMER       0x0EAu
#define AE_P2_PLAYER_VEL_FRACTION       0x108u
#define AE_P2_PLAYER_VEL_FRACTION_Y     0x10Cu
#define AE_P2_PLAYER_VEL_FRACTION_Z     0x110u
#define AE_P2_PLAYER_POSITION_Y        0x028u
#define AE_P2_PLAYER_POSITION_Z         0x02Cu
#define AE_P2_PLAYER_VELOCITY_X         0x030u
#define AE_P2_PLAYER_VELOCITY_Y         0x034u
#define AE_P2_PLAYER_VELOCITY_Z         0x038u
#define AE_P2_PLAYER_MODEL              0x000u
#define AE_P2_PLAYER_BONE_MATRICES      0x014u
#define AE_P2_PLAYER_FRONT_VECTOR       0x060u
#define AE_P2_PLAYER_UP_VECTOR          0x068u
#define AE_P2_PLAYER_SQUASH_FRAME       0x100u
#define AE_P2_PLAYER_ANIM_SET            0x018u
#define AE_P2_PLAYER_SAVED_GADGET         0x0F3u
#define AE_P2_MODEL_VERTEX_COUNT        0x006u
#define AE_P2_IDLE_MODEL_STATE          0x086u
#define AE_P2_IDLE_MODEL_POSITION       0x034u
#define AE_P2_IDLE_MODEL_ANIM_FRAME     0x06Cu
#define AE_P2_IDLE_MODEL_ACTOR          0x010u
#define AE_P2_IDLE_MODEL_BONES          0x01Cu
#define AE_P2_IDLE_MODEL_MATRICES       0x024u
#define AE_P2_IDLE_MODEL_ANIMATION      0x028u
#define AE_P2_IDLE_MODEL_ANIM_SET       0x000u
#define AE_P2_IDLE_MODEL_STATE_TABLE    0x004u
#define AE_P2_IDLE_MODEL_TRANSITIONS    0x008u
#define AE_P2_IDLE_MODEL_EVENTS         0x00Cu
#define AE_P2_IDLE_MODEL_MATRIX         0x024u
#define AE_P2_IDLE_MODEL_BONE_DATA      0x01Cu
#define AE_P2_IDLE_MODEL_MATRIX_PTR     0x024u
#define AE_P2_PLAYER_ANIM_FRAME         0x05Cu
#define AE_P2_PLAYER_CONTACT            0x050u
#define AE_P2_PLAYER_FACING_AND_UP_SIZE 0x010u
#define AE_P2_RC_CAR_ACTIVE             0x070u
#define AE_P2_RC_CAR_COOLDOWN           0x078u
#define AE_P2_RC_CAR_BODY               0x024u
#define AE_P2_RC_CAR_VELOCITY           0x030u
#define AE_P2_RC_CAR_FACING_AND_UP      0x060u
#define AE_P2_RC_CAR_RADIUS             0x044u
#define AE_P2_RC_CAR_HEIGHT             0x046u
#define AE_P2_RC_CAR_FLAGS              0x050u
#define AE_P2_STATE_SET_STRIDE          0x010u
#define AE_P2_STATE_SET_STATES          0x004u
#define AE_P2_STATE_SET_TRANSITIONS     0x008u
#define AE_P2_STATE_SET_EVENTS          0x00Cu
#define AE_P2_STATE_ENTRY_SIZE          0x008u
#define AE_P2_STATE_START_FRAME         0x000u
#define AE_P2_STATE_END_FRAME           0x002u
#define AE_P2_COLLISION_SLOT_BASE       0x800BBDA0u
#define AE_P2_COLLISION_SLOT_STRIDE     0x014u
#define AE_P2_COLLISION_SLOT_BODY       0x004u
#define AE_P2_BODY_TYPE                 0x024u
#define AE_P2_BODY_FLAGS                0x026u
#define AE_P2_BODY_POSITION_X           0x000u
#define AE_P2_BODY_POSITION_Y           0x004u
#define AE_P2_BODY_POSITION_Z           0x008u
#define AE_P2_BODY_FACING               0x018u
#define AE_P2_RANDOM_PARTICLE_STRIDE    0x01Cu
#define AE_P2_RANDOM_PARTICLE_POINTER   0x000u
#define AE_P2_RANDOM_PARTICLE_POSITION  0x004u
#define AE_P2_RANDOM_PARTICLE_VELOCITY  0x008u
#define AE_P2_RANDOM_PARTICLE_SPREAD    0x010u
#define AE_P2_RANDOM_PARTICLE_TYPE      0x018u
#define AE_P2_RANDOM_PARTICLE_VARIANT   0x019u
#define AE_P2_RANDOM_PARTICLE_SIZE      0x01Au
#define AE_P2_PLAYER_GROUND_Y          0x12Cu
#define AE_P2_PLAYER_FACING            0x060u
#define AE_P2_PLAYER_HAS_GROUND        0x136u
#define AE_P2_PLAYER_GROUND_TYPE       0x138u
#define AE_P2_PLAYER_STATE             0x05Eu
#define AE_P2_TRANSITION_STATE          0x00u
#define AE_P2_TRANSITION_CONDITION      0x01u
#define AE_P2_TRANSITION_NEXT_STATE     0x02u
#define AE_P2_TRANSITION_ACTION         0x03u
#define AE_P2_TRANSITION_STRIDE         0x04u
#define AE_P2_PLAYER_CURRENT_GADGET    0x0F2u
#define AE_P2_PLAYER_TARGET_MOVE_VEL_X 0x114u
#define AE_P2_PLAYER_TARGET_MOVE_VEL_Y 0x118u
#define AE_P2_PLAYER_TARGET_MOVE_VEL_Z 0x11Cu
#define AE_P2_PLAYER_MOVE_VEL_X        0x120u
#define AE_P2_PLAYER_MOVE_VEL_Y        0x124u
#define AE_P2_PLAYER_MOVE_VEL_Z        0x128u
#define AE_P2_PLAYER_HEALTH            0x0E8u
#define AE_P2_PLAYER_WALL_BUMP_MARKER   0x0EEu
#define AE_P2_PLAYER_STATE_FLAGS        0x070u
#define AE_P2_PLAYER_PITCH              0x07Cu
#define AE_P2_PLAYER_SAVED_FACING       0x080u

#define AE_P2_BLINK_FX_TIMER           0x008u
#define AE_P2_BLINK_FX_VALUE           0x009u
#define AE_P2_AIM_TABLE_STRIDE         0x008u
#define AE_P2_VECTOR_WORD_SIZE         0x004u

#define AE_P2_GADGET_FX_CLEAR_FLAGS    0x024u
#define AE_P2_GADGET_FX_CLEAR_TIMER    0x028u
#define AE_P2_GADGET_FX_CLEAR_MODE     0x02Au
#define AE_P2_GADGET_FX_CLEAR_FRAME    0x02Cu
#define AE_P2_GADGET_FX_CLEAR_COUNT    0x048u
#define AE_P2_GADGET_FX_CLEAR_VALUE    0x04Au
#define AE_P2_GADGET_FX_CLEAR_FLAGS_2  0x04Cu
#define AE_P2_GADGET_FX_CLEAR_FLAGS_3  0x050u

#define AE_P2_RADAR_POSITION           0x004u
#define AE_P2_RADAR_DIRECTION          0x008u
#define AE_P2_RADAR_TARGET              0x00Cu
#define AE_P2_RADAR_TIMER               0x010u
#define AE_P2_RADAR_MARKER_A            0x014u
#define AE_P2_RADAR_MARKER_B            0x016u
#define AE_P2_RADAR_FLAGS               0x018u
#define AE_P2_RADAR_FLAGS_2             0x01Cu

#define AE_P2_SPU_MAIN_LEFT             0x1B0u
#define AE_P2_SPU_MAIN_RIGHT            0x1B2u
#define AE_P2_LOCAL_DUST_BEHIND         0x801FF100u
#define AE_P2_LOCAL_SPARKLE             0x801FF110u
#define AE_P2_LOCAL_DUST                0x801FF120u
#define AE_P2_LOCAL_GROUND_IMPACT       0x801FF130u
#define AE_P2_LOCAL_AREA2_DUST           0x801FF160u
#define AE_P2_G_PLAYER_RANDOM_SEED       0x800AE8F0u
#define AE_P2_G_PLAYER_RANDOM_SEED_2     0x800AE8F2u
#define AE_P2_AREA2_DUST_PARTICLE_TABLE  0x8013F248u
#define AE_P2_RAND_SEED                 0x800AE8F0u
#define AE_P2_G_PLAYER_FX_POSITION      0x800B4D88u
#define AE_P2_G_CURRENT_AREA            0x800F4476u
#define AE_P2_G_FRAME_COUNTER           0x800EC350u
#define AE_P2_G_PLAYER_MODE             0x800F44A0u
#define AE_P2_G_PLAYER_STATE_CHANGE_MODE 0x800EC23Eu
#define AE_P2_G_PLAYER_SCRATCH          0x1F800000u
#define AE_P2_SCRATCH_CONTACT_NORMAL    0x008u
#define AE_P2_SCRATCH_CONTACT_NORMAL_Z  0x00Cu
#define AE_P2_SCRATCH_CONTACT_DIRECTION 0x010u
#define AE_P2_SCRATCH_CONTACT_DELTA_X   0x018u
#define AE_P2_SCRATCH_CONTACT_DELTA_Y   0x01Cu
#define AE_P2_SCRATCH_CONTACT_DELTA_Z   0x020u
#define AE_P2_G_INPUT_BLOCK             0x800E5320u
#define AE_P2_G_CLIMB_STICK_Y           0x800E53C3u
#define AE_P2_G_CLIMB_STICK_X           0x800E53C2u
#define AE_P2_G_ROPE_MOVE_STICK         0x800E54A0u
#define AE_P2_G_ROPE_TARGET_DIRECTION   0x800E54B4u
#define AE_P2_G_ROPE_JUMP_ALLOWED       0x800E5498u
#define AE_P2_G_DAMAGE_KNOCKBACK_TABLE  0x800B0264u
#define AE_P2_G_DAMAGE_ANGLE_TABLE      0x800B0250u
#define AE_P2_G_CLIMB_MATRIX_RIGHT      0x800AE7A0u
#define AE_P2_G_CLIMB_MATRIX_LEFT       0x800AE780u
#define AE_P2_G_CURRENT_LEVEL           0x800F4474u
#define AE_P2_G_CAPTURE_ACTOR           0x800F50B0u
#define AE_P2_G_CAPTURE_ANIM_DATA       0x800B4DA0u
#define AE_P2_G_CAPTURE_ANIM_FRAME      0x800F510Cu
#define AE_P2_G_CAPTURE_ANIM_INDEX      0x800F510Eu
#define AE_P2_G_CAPTURE_CLOTH_MATRIX    0x800F50C4u
#define AE_P2_G_CAPTURE_FLASH_FRAME     0x800F5174u
#define AE_P2_G_CAPTURE_PIVOT           0x800F50D4u
#define AE_P2_G_CAPTURE_NORMAL_A        0x800F5110u
#define AE_P2_G_CAPTURE_NORMAL_B        0x800F5118u
#define AE_P2_G_STATIC_MATRIX           0x800F4910u
#define AE_P2_G_PLAYER_STATIC_MODELS    0x800F4630u
#define AE_P2_G_NET_MODEL_NORMAL        0x800EC070u
#define AE_P2_G_NET_MODEL_SWIMMING      0x800EC140u
#define AE_P2_G_NET_MODEL_RIGID         0x800EC0E0u
#define AE_P2_G_NET_NORMAL_VERTICES     0x800EC07Cu
#define AE_P2_G_NET_SWIM_VERTICES       0x800EC14Cu
#define AE_P2_G_NET_MATRIX_SOURCE       0x800E9E40u
#define AE_P2_G_NET_POINT_TABLE_NORMAL  0x800B2140u
#define AE_P2_G_NET_POINT_TABLE_SWIMMING 0x800B2188u
#define AE_P2_G_CAUGHT_GROUP_LOOKUP     0x800AEBF8u
#define AE_P2_G_STATIC_MODEL_MATRICES   0x800E9C60u
#define AE_P2_G_CAPTURE_DRAW_BASE       0x800EC3D0u
#define AE_P2_G_VIEW_MATRIX             0x800E78C0u
#define AE_P2_G_NET_LOOK_DIRECTION      0x800F51D8u
#define AE_P2_G_NET_ORIGIN_X            0x800F51D0u
#define AE_P2_G_NET_ORIGIN_Y            0x800F51D2u
#define AE_P2_G_NET_ORIGIN_Z            0x800F51D4u
#define AE_P2_CAPTURE_ANIM_FIRST        0x000u
#define AE_P2_CAPTURE_ANIM_LAST         0x002u
#define AE_P2_G_NET_FRAME_LOCAL_MATRIX  0x801FFE70u
#define AE_P2_G_NET_NESTED_LOCAL_MATRIX 0x801FFE58u
#define AE_P2_G_NET_DRAW_CONTEXT_STRIDE 0x4078u
#define AE_P2_G_PLAYER_DRAW_STRIDE      0x4078u
#define AE_P2_G_NET_RIBBON_FLAGS        0x800F4450u
#define AE_P2_NET_STATIC_MODEL_POINTER  0x000u
#define AE_P2_NET_STATIC_MODEL_MESH_INDEX 0x004u
#define AE_P2_NET_STATIC_MODEL_STRIDE   0x010u
#define AE_P2_NET_MATRIX_STRIDE         0x020u
#define AE_P2_NET_MATRIX_TRANSLATION_X  0x014u
#define AE_P2_NET_MATRIX_TRANSLATION_Y  0x018u
#define AE_P2_NET_MATRIX_TRANSLATION_Z  0x01Cu
#define AE_P2_NET_MATRIX_WORD_SIZE      0x004u
#define AE_P2_NET_SVECTOR_SIZE          0x008u
#define AE_P2_NET_SVECTOR_Y_OFFSET      0x002u
#define AE_P2_NET_SVECTOR_Z_OFFSET      0x004u
#define AE_P2_NET_VECTOR_Y_OFFSET       0x004u
#define AE_P2_NET_VECTOR_Z_OFFSET       0x008u
#define AE_P2_NET_VECTOR_AXIS_SIZE      0x004u
#define AE_P2_NET_POINT_RECORD_STRIDE   0x00Cu
#define AE_P2_NET_POINT_MATRIX_INDEX    0x000u
#define AE_P2_NET_POINT_VERTEX_INDEX    0x002u
#define AE_P2_NET_POINT_POSITION        0x004u
#define AE_P2_NET_FLASH_SCALE_FRAMES    9u
#define AE_P2_NET_NORMAL_ENDPOINT_A     0u
#define AE_P2_NET_NORMAL_ENDPOINT_B     0x51u
#define AE_P2_NET_NORMAL_ENDPOINT_C     8u
#define AE_P2_NET_SWIM_ENDPOINT_A       2u
#define AE_P2_NET_SWIM_ENDPOINT_B       0x0Cu
#define AE_P2_NET_SWIM_ENDPOINT_C       0x0Bu
#define AE_P2_NET_SCREEN_VERTEX_STRIDE  8u
#define AE_P2_NET_LOCAL_TRANSFORM_A_OFFSET 0x020u
#define AE_P2_NET_LOCAL_TRANSFORM_B_OFFSET 0x028u
#define AE_P2_NET_LOCAL_TRANSFORM_C_OFFSET 0x030u
#define AE_P2_NET_LOCAL_EDGE_A_OFFSET   0x040u
#define AE_P2_NET_LOCAL_EDGE_B_OFFSET   0x050u
#define AE_P2_NET_LOCAL_SCALE_OFFSET    0x058u
#define AE_P2_NET_LOCAL_GTE_SCRATCH_OFFSET 0x068u
#define AE_P2_NET_PROJECTED_DEPTH       0x801FFED8u
#define AE_P2_NET_PROJECTED_FLAGS       0x801FFED8u
#define AE_P2_G_AIM_MODE                0x800E6916u
#define AE_P2_G_AIM_REQUEST             0x800E6902u
#define AE_P2_G_CAMERA_FIXED_MODE       0x800E6908u
#define AE_P2_G_WATER_ENTRY_STATE       0x800F51E0u
#define AE_P2_G_WATER_ENTRY_MARKER_A    0x800F51E8u
#define AE_P2_G_WATER_ENTRY_MARKER_B    0x800F51EAu
#define AE_P2_ACCEL_SHIFT                6u
#define AE_P2_ACCEL_ROUNDING             0x3Fu

static int32_t ae_p2_s16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return low < 0x8000u ? (int32_t)low : (int32_t)low - 0x10000;
}

static int32_t ae_p2_s8(uint32_t value)
{
    uint32_t low = value & 0xFFu;
    return low < 0x80u ? (int32_t)low : (int32_t)low - 0x100;
}

static int32_t ae_p2_s32_from_bits(uint32_t bits)
{
    return bits <= INT32_MAX ? (int32_t)bits : (int32_t)((int64_t)bits - 0x100000000LL);
}

static int32_t ae_p2_sub_wrap(int32_t left, int32_t right)
{
    return ae_p2_s32_from_bits((uint32_t)left - (uint32_t)right);
}

static int32_t ae_p2_add_wrap(int32_t left, int32_t right)
{
    return ae_p2_s32_from_bits((uint32_t)left + (uint32_t)right);
}

static int32_t ae_p2_mul_wrap(int32_t left, int32_t right)
{
    return ae_p2_s32_from_bits((uint32_t)left * (uint32_t)right);
}

static uint32_t ae_p2_advance_random_seed(AeRam *r)
{
    uint32_t seed = (ae_rd16(r, AE_P2_RAND_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_P2_RAND_SEED, seed);
    return seed;
}

static uint32_t ae_p2_random_byte(AeRam *r)
{
    return ae_p2_advance_random_seed(r) >> 8;
}

static int ae_p2_set_position_padding(AeRam *r, const AePlayer2Hooks *hooks,
                                      uint32_t position, uint32_t original_function)
{
    if (!hooks || !hooks->position_padding) {
        ae_ram_fail(r, "AePlayer2Hooks.position_padding not provided for %08x",
                    original_function);
        return 0;
    }
    ae_wr16(r, position + 6u,
            hooks->position_padding(hooks->user, r, original_function));
    return !ae_ram_failed(r);
}

static int ae_p2_play_sfx(AeRam *r, const AePlayer2Hooks *hooks, uint32_t sound_id)
{
    if (!hooks || !hooks->PlaySfx) {
        ae_ram_fail(r, "AePlayer2Hooks.PlaySfx not provided");
        return 0;
    }
    hooks->PlaySfx(hooks->user, r, sound_id);
    return !ae_ram_failed(r);
}

static int ae_p2_call_simple_hook(AeRam *r, const AePlayer2Hooks *hooks,
                                  AePlayer2SimpleHookFn function, const char *name)
{
    if (!function) {
        ae_ram_fail(r, "AePlayer2Hooks.%s not provided", name);
        return 0;
    }
    function(hooks->user, r);
    return !ae_ram_failed(r);
}

static int32_t ae_p2_shift_right_6(int32_t value)
{
    uint32_t shifted = (uint32_t)value >> AE_P2_ACCEL_SHIFT;
    if (value < 0) shifted |= 0xFC000000u;
    return ae_p2_s32_from_bits(shifted);
}

static int32_t ae_p2_shift_right(int32_t value, uint32_t amount)
{
    uint32_t shifted = (uint32_t)value >> amount;
    if (value < 0) shifted |= UINT32_MAX << (32u - amount);
    return ae_p2_s32_from_bits(shifted);
}

static int32_t ae_p2_accel_divide(AeRam *r, int32_t numerator, int32_t denominator,
                                 const char *function_name)
{
    if (denominator == 0) {
        ae_ram_fail(r, "%s reached a signed divide by zero", function_name);
        return 0;
    }
    if (numerator == INT32_MIN && denominator == -1) {
        ae_ram_fail(r, "%s reached signed division overflow", function_name);
        return 0;
    }
    return numerator / denominator;
}

static int32_t ae_p2_accel_denominator(AeRam *r, int32_t distance,
                                       const char *function_name)
{
    int32_t rounded = distance < 0 ? ae_p2_add_wrap(distance, AE_P2_ACCEL_ROUNDING) : distance;
    int32_t denominator = ae_p2_shift_right_6(rounded);

    if (denominator == 0) {
        ae_ram_fail(r, "%s reached a zero acceleration divisor", function_name);
    }
    return denominator;
}

int32_t ae_PlayerIsOnGround(AeRam *r)
{
    int32_t ground_y = ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_GROUND_Y);
    int32_t position_y = ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Y);
    int32_t has_ground = ae_rds16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_HAS_GROUND);

    return ground_y <= position_y && has_ground != 0;
}

void ae_PlayerRunTransitions(AeRam *r, const AePlayer2Hooks *hooks,
                             const AePlayerActHooks *act, uint32_t transitions)
{
    uint32_t index = 0;
    uint32_t player_state = ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_STATE);

    for (;;) {
        uint32_t entry = transitions + index * AE_P2_TRANSITION_STRIDE;
        uint32_t entry_state = ae_rd8(r, entry + AE_P2_TRANSITION_STATE);
        int32_t condition_index;
        uint32_t condition_function;
        int32_t condition_result;
        int take_transition;
        uint32_t action_index;

        if (player_state < entry_state) return;
        if (player_state != entry_state) {
            index++;
            continue;
        }

        condition_index = ae_p2_s8(ae_rd8(r, entry + AE_P2_TRANSITION_CONDITION));
        condition_function = ae_rd32(r, AE_P2_G_PLAYER_CONDITIONS +
                                     (uint32_t)(condition_index < 0
                                             ? -condition_index : condition_index) * 4u);
        if (!hooks || !hooks->PlayerCondition) {
            ae_ram_fail(r, "AePlayer2Hooks.PlayerCondition not provided");
            return;
        }
        condition_result = hooks->PlayerCondition(hooks->user, r, condition_function);
        if (ae_ram_failed(r)) return;
        take_transition = condition_index > 0
                ? condition_result != 0
                : condition_result == 0;
        index++;
        if (!take_transition) continue;

        ae_PlayerSetState(r, act, ae_rd8(r, entry + AE_P2_TRANSITION_NEXT_STATE));
        if (ae_ram_failed(r)) return;
        action_index = ae_rd8(r, entry + AE_P2_TRANSITION_ACTION);
        if (action_index != 0) {
            uint32_t action_function = ae_rd32(r, AE_P2_G_PLAYER_ACTIONS + action_index * 4u);
            if (!hooks->PlayerAction) {
                ae_ram_fail(r, "AePlayer2Hooks.PlayerAction not provided");
                return;
            }
            hooks->PlayerAction(hooks->user, r, action_function);
        }
        return;
    }
}

void ae_PlayerUpdateHeldByObject(AeRam *r, const AePlayer2Hooks *hooks,
                                 const AePlayerActHooks *act)
{
    uint32_t state = ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_STATE);
    int32_t gadget = ae_p2_s8(ae_rd8(r, AE_P2_G_PLAYER + AE_P2_PLAYER_CURRENT_GADGET));
    uint32_t gadget_set = AE_P2_G_PLAYER_STATE_SETS +
                          (uint32_t)(gadget + 1) * AE_P2_STATE_SET_STRIDE;
    uint32_t state_table = AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_STATES;
    uint32_t anim_frame = ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_ANIM_FRAME);
    int32_t stick_travel = ae_rds16(r, AE_P2_G_STICK_TRAVEL) +
                           ae_rds16(r, AE_P2_G_STICK_TRAVEL + 2u);

    if (state >= 0x80u) state_table = gadget_set + AE_P2_STATE_SET_STATES;
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_STATE_TABLE, ae_rd32(r, state_table));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS,
            ae_rd32(r, gadget_set + AE_P2_STATE_SET_TRANSITIONS));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_EVENTS,
            ae_rd32(r, gadget_set + AE_P2_STATE_SET_EVENTS));

    ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X, 0);
    ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Y, 0);
    ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z, 0);
    anim_frame += stick_travel > 100 ? 2u : 1u;
    ae_wr16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_ANIM_FRAME, anim_frame);

    {
        uint32_t contact = ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_CONTACT);
        if ((contact >> 28) == 0xBu) {
            uint32_t body = ae_GetObjectByIndex(r, contact & 0xFFFu);
            uint32_t object_type = ae_rd16(r, body + AE_P2_BODY_TYPE) & 0xFF00u;

            if (object_type == 0x0B00u) {
                uint32_t player = AE_P2_G_PLAYER;
                ae_wr32(r, player + AE_P2_PLAYER_POSITION_X,
                        ae_rd32(r, body + AE_P2_BODY_POSITION_X));
                ae_wr32(r, player + AE_P2_PLAYER_POSITION_Y,
                        ae_rd32(r, body + AE_P2_BODY_POSITION_Y));
                ae_wr32(r, player + AE_P2_PLAYER_POSITION_Z,
                        ae_rd32(r, body + AE_P2_BODY_POSITION_Z));
                ae_wr32(r, player + AE_P2_PLAYER_FACING,
                        ae_rd32(r, body + AE_P2_BODY_FACING));
                ae_wr32(r, player + AE_P2_PLAYER_FACING + 4u,
                        ae_rd32(r, body + AE_P2_BODY_FACING + 4u));
                ae_PlayerRunTransitions(r, hooks, act,
                        ae_rd32(r, AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_TRANSITIONS));
                if (ae_ram_failed(r)) return;
                ae_PlayerRunTransitions(r, hooks, act,
                        ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS));
                return;
            }
        }
    }

    ae_PlayerSetMode(r, act, 0);
    if (ae_ram_failed(r)) return;
    ae_PlayerSetState(r, act, 0);
}

int32_t ae_PlayerCheckWaterEntry(AeRam *r, const AePlayer2Hooks *hooks,
                                 const AeSpawnHooks *spawn)
{
    uint32_t state_flags = ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_STATE_FLAGS) & 0xF0u;
    uint32_t ground_type;
    int32_t gadget;

    if (state_flags == 0xC0u || state_flags == 0xE0u) return 0;
    ground_type = ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_GROUND_TYPE);
    if ((ground_type >> 12) != 1u || (ground_type & 0xFu) != 0) return 0;

    gadget = ae_p2_s8(ae_rd8(r, AE_P2_G_PLAYER + AE_P2_PLAYER_CURRENT_GADGET));
    if (!hooks || !hooks->PlayerStopGadgetSounds) {
        ae_ram_fail(r, "AePlayer2Hooks.PlayerStopGadgetSounds not provided");
        return 0;
    }
    hooks->PlayerStopGadgetSounds(hooks->user, r, gadget);
    if (ae_ram_failed(r)) return 0;

    ae_wr32(r, AE_P2_G_WATER_ENTRY_STATE, 0);
    ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_PITCH, 0);
    ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_SAVED_FACING,
            ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING));
    ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_SAVED_FACING + 4u,
            ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING + 4u));
    ae_PlayerSplashStart(r, spawn, hooks);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, AE_P2_G_WATER_ENTRY_MARKER_A, 0xFFFFu);
    ae_wr16(r, AE_P2_G_WATER_ENTRY_MARKER_B, 0xFFFFu);
    if (ae_rd8(r, AE_P2_G_AIM_MODE) == 1u &&
        !ae_p2_call_simple_hook(r, hooks, hooks->AimModeLeave, "AimModeLeave")) return 0;
    if (gadget == 2) ae_RadarResetState(r);
    return 1;
}

void ae_PlayerSplashStart(AeRam *r, const AeSpawnHooks *spawn,
                          const AePlayer2Hooks *hooks)
{
    int32_t splash_velocity;
    uint32_t position = AE_P2_LOCAL_SPLASH;

    if (ae_rd8(r, AE_P2_G_SPLASH_FADE) != 0) return;

    (void)ae_GetSurfaceType(r,
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Y),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Z));
    ae_wr16(r, AE_P2_G_SPLASH_SURFACE,
            (uint32_t)(ae_rds16(r, AE_P2_SCRATCH_SURFACE_TOP) -
                       ae_rds16(r, AE_P2_SCRATCH_SURFACE_BOTTOM)));
    /* 8006B8B8 publishes the same height for water particles. */
    ae_wr16(r, AE_P2_G_WATER_PARTICLE_HEIGHT, ae_rd16(r, AE_P2_G_SPLASH_SURFACE));
    ae_wr16(r, AE_P2_LOCAL_SPLASH,
            ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X));
    ae_wr16(r, AE_P2_LOCAL_SPLASH + 2u, ae_rd16(r, AE_P2_G_SPLASH_SURFACE));
    ae_wr16(r, AE_P2_LOCAL_SPLASH + 4u,
            ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Z));
    ae_SpawnParticle(r, spawn, 0x100, position);
    if (ae_ram_failed(r)) return;
    if (!ae_p2_play_sfx(r, hooks, 0x72u)) return;

    ae_wr8(r, AE_P2_G_SPLASH_STAGE, 0);
    splash_velocity = ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_VELOCITY_Y) / 2;
    ae_wr8(r, AE_P2_G_SPLASH_FADE,
            (uint32_t)((uint8_t)splash_velocity > 0x14u ? 0x14 : splash_velocity));
}

int32_t ae_PlayerAccelXZ(AeRam *r, int32_t max_step)
{
    int32_t delta_x = ae_p2_sub_wrap(
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_X));
    int32_t delta_z = ae_p2_sub_wrap(
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_Z));
    int32_t scaled_x;
    int32_t scaled_z;
    int32_t distance;
    int32_t divisor;

    if (delta_x < 0) delta_x = ae_p2_add_wrap(delta_x, AE_P2_ACCEL_ROUNDING);
    if (delta_z < 0) delta_z = ae_p2_add_wrap(delta_z, AE_P2_ACCEL_ROUNDING);
    scaled_x = ae_p2_shift_right_6(delta_x);
    scaled_z = ae_p2_shift_right_6(delta_z);
    distance = ae_p2_s32_from_bits((uint32_t)ae_player_vec_length3(scaled_x, 0, scaled_z)
                                   << AE_P2_ACCEL_SHIFT);
    if (max_step < distance) {
        int32_t step_x = ae_p2_mul_wrap(scaled_x, max_step);
        int32_t step_z = ae_p2_mul_wrap(scaled_z, max_step);
        int32_t quotient_x;
        int32_t quotient_z;
        divisor = ae_p2_accel_denominator(r, distance, "PlayerAccelXZ");
        if (ae_ram_failed(r)) return 0;
        quotient_x = ae_p2_accel_divide(r, step_x, divisor, "PlayerAccelXZ");
        if (ae_ram_failed(r)) return 0;
        quotient_z = ae_p2_accel_divide(r, step_z, divisor, "PlayerAccelXZ");
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X,
                ae_p2_add_wrap(ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_X),
                               quotient_x));
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z,
                ae_p2_add_wrap(ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_Z),
                               quotient_z));
        return distance;
    }
    return 0;
}

int32_t ae_PlayerAccelXYZ(AeRam *r, int32_t max_step)
{
    int32_t delta_x = ae_p2_sub_wrap(
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_X));
    int32_t delta_y = ae_p2_sub_wrap(
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Y),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_Y));
    int32_t delta_z = ae_p2_sub_wrap(
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z),
            ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_Z));
    int32_t scaled_x;
    int32_t scaled_y;
    int32_t scaled_z;
    int32_t distance;
    int32_t divisor;

    if (delta_x < 0) delta_x = ae_p2_add_wrap(delta_x, AE_P2_ACCEL_ROUNDING);
    if (delta_y < 0) delta_y = ae_p2_add_wrap(delta_y, AE_P2_ACCEL_ROUNDING);
    if (delta_z < 0) delta_z = ae_p2_add_wrap(delta_z, AE_P2_ACCEL_ROUNDING);
    scaled_x = ae_p2_shift_right_6(delta_x);
    scaled_y = ae_p2_shift_right_6(delta_y);
    scaled_z = ae_p2_shift_right_6(delta_z);
    distance = ae_p2_s32_from_bits((uint32_t)ae_player_vec_length3(scaled_x, scaled_y, scaled_z)
                                   << AE_P2_ACCEL_SHIFT);
    if (max_step < distance) {
        int32_t step_x = ae_p2_mul_wrap(scaled_x, max_step);
        int32_t step_y = ae_p2_mul_wrap(scaled_y, max_step);
        int32_t step_z = ae_p2_mul_wrap(scaled_z, max_step);
        int32_t quotient_x;
        int32_t quotient_y;
        int32_t quotient_z;
        divisor = ae_p2_accel_denominator(r, distance, "PlayerAccelXYZ");
        if (ae_ram_failed(r)) return 0;
        quotient_x = ae_p2_accel_divide(r, step_x, divisor, "PlayerAccelXYZ");
        if (ae_ram_failed(r)) return 0;
        quotient_y = ae_p2_accel_divide(r, step_y, divisor, "PlayerAccelXYZ");
        if (ae_ram_failed(r)) return 0;
        quotient_z = ae_p2_accel_divide(r, step_z, divisor, "PlayerAccelXYZ");
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X,
                ae_p2_add_wrap(ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_X),
                               quotient_x));
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Y,
                ae_p2_add_wrap(ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_Y),
                               quotient_y));
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z,
                ae_p2_add_wrap(ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_TARGET_MOVE_VEL_Z),
                               quotient_z));
        return distance;
    }
    return 0;
}

void ae_PlayerStartHurtBlink(AeRam *r)
{
    ae_wr16(r, AE_P2_G_PLAYER_HURT_BLINK, 0x0Fu);
}

void ae_PlayerEnterSwimIfInWater(AeRam *r)
{
    uint32_t ground_type = ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_GROUND_TYPE);

    if ((ground_type >> 12) == 1u) {
        /* State 0x49 cannot take PlayerSetState's animation-frame hook path. */
        ae_PlayerSetState(r, NULL, 0x49u);
    }
}

void ae_PlayerApplyGadgetStance(AeRam *r, const AePlayerActHooks *act)
{
    uint32_t state = ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_STATE);
    uint32_t new_state;
    int32_t has_alt_stance = ae_GadgetHasAltStance(r);

    if (has_alt_stance == 0) {
        if (state == 4u) {
            new_state = 3u;
        } else if (state < 5u) {
            if (state != 1u) return;
            new_state = 0u;
        } else if (state == 7u) {
            new_state = 5u;
        } else {
            if (state != 0x14u) return;
            new_state = 0x13u;
        }
    } else if (state == 3u) {
        new_state = 4u;
    } else if (state < 4u) {
        if (state != 0u) return;
        new_state = 1u;
    } else if (state == 5u) {
        new_state = 7u;
    } else {
        if (state != 0x13u) return;
        new_state = 0x14u;
    }

    ae_PlayerSetState(r, act, new_state);
}

void ae_PlayerResetAir(AeRam *r)
{
    ae_wr16(r, AE_P2_G_PLAYER_AIR, 600u);
    ae_wr16(r, AE_P2_G_AIR_WARNING_PENDING, 0);
}

void ae_PlayerFxResetBlink(AeRam *r)
{
    ae_wr8(r, AE_P2_G_PLAYER_BLINK_FX + AE_P2_BLINK_FX_TIMER, 0);
    ae_wr8(r, AE_P2_G_PLAYER_BLINK_FX + AE_P2_BLINK_FX_VALUE, 0x10u);
}

void ae_PlayerSpawnDustBehind(AeRam *r, const AePlayer2Hooks *hooks)
{
    int32_t facing_x = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING));
    int32_t facing_z = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING + 4u));
    uint32_t position = AE_P2_LOCAL_DUST_BEHIND;

    ae_wr16(r, position,
            (uint32_t)ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X)) -
            (uint32_t)(facing_x >> 6));
    ae_wr16(r, position + 2u,
            (uint32_t)ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Y)) - 0x30u);
    ae_wr16(r, position + 4u,
            (uint32_t)ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Z)) -
            (uint32_t)(facing_z >> 6));
    if (!ae_p2_set_position_padding(r, hooks, position, 0x8006BF10u)) return;

    ae_SpawnParticle(r, NULL, 0x14, position);
    if (!ae_ram_failed(r)) ae_SpawnParticle(r, NULL, 0x15, position);
}

void ae_PlayerSpawnDust(AeRam *r, const AePlayer2Hooks *hooks)
{
    uint32_t position = AE_P2_LOCAL_DUST;
    uint32_t player_position = AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X;
    uint32_t player_velocity = AE_P2_G_PLAYER + AE_P2_PLAYER_VELOCITY_X;

    if (ae_rd16(r, AE_P2_G_CURRENT_AREA) == 2u) {
        ae_FUN_8013f0c0(r, hooks);
        return;
    }
    if ((ae_rd32(r, AE_P2_G_FRAME_COUNTER) & 1u) == 0) return;

    ae_wr32(r, position, 0);
    ae_wr32(r, position + 4u, 0);
    ae_wr16(r, position,
            ae_rd16(r, player_position) - ((uint32_t)ae_rd32(r, player_velocity) << 1));
    ae_wr16(r, position + 2u, ae_rd16(r, player_position + 4u) - 0x20u);
    ae_wr16(r, position + 4u,
            ae_rd16(r, player_position + 8u) - ((uint32_t)ae_rd32(r, player_velocity + 8u) << 1));
    ae_SpawnParticle(r, NULL, 4, position);
}

/* Area overlay member 18C, function at 8013F0C0. It samples the player-relative
 * position repeatedly, with a seed-derived stopping limit, then starts particle
 * script 200 from the final sample. */
void ae_FUN_8013f0c0(AeRam *r, const AePlayer2Hooks *hooks)
{
    uint32_t position = AE_P2_LOCAL_AREA2_DUST;
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t player_x = ae_rd16(r, player + AE_P2_PLAYER_POSITION_X);
    uint32_t player_y = ae_rd16(r, player + AE_P2_PLAYER_POSITION_Y);
    uint32_t player_z = ae_rd16(r, player + AE_P2_PLAYER_POSITION_Z);
    uint16_t primary_seed = (uint16_t)(ae_rd16(r, AE_P2_G_PLAYER_RANDOM_SEED) * 5u + 1u);
    uint32_t initial_limit;

    ae_wr16(r, AE_P2_G_PLAYER_RANDOM_SEED, primary_seed);
    initial_limit = ((primary_seed >> 8) & 7u) + 3u;

    /* The original count is always in 3..10; its zero branch is therefore dead. */
    if (initial_limit != 0u) {
        uint32_t iteration = 0;

        for (;;) {
            uint16_t secondary_seed;
            uint32_t next_limit;
            int keep_sampling;

            iteration++;
            ae_wr16(r, position + 2u, player_y);

            secondary_seed = (uint16_t)(ae_rd16(r, AE_P2_G_PLAYER_RANDOM_SEED_2) * 5u + 1u);
            ae_wr16(r, AE_P2_G_PLAYER_RANDOM_SEED_2, secondary_seed);
            ae_wr16(r, position,
                    player_x + ((secondary_seed >> 8) & 0x7Fu) - 0x40u);

            primary_seed = (uint16_t)(ae_rd16(r, AE_P2_G_PLAYER_RANDOM_SEED) * 5u + 1u);
            ae_wr16(r, AE_P2_G_PLAYER_RANDOM_SEED, primary_seed);
            next_limit = ((primary_seed >> 8) & 7u) + 3u;
            keep_sampling = iteration < next_limit;

            secondary_seed = (uint16_t)(ae_rd16(r, AE_P2_G_PLAYER_RANDOM_SEED_2) * 5u + 1u);
            ae_wr16(r, AE_P2_G_PLAYER_RANDOM_SEED_2, secondary_seed);
            ae_wr16(r, position + 4u,
                    player_z + ((secondary_seed >> 8) & 0x7Fu) - 0x40u);

            if (!keep_sampling) break;
        }
    }

    if (!ae_p2_set_position_padding(r, hooks, position, 0x8013F0C0u)) return;
    ae_SpawnOneParticle(r, 200, position, AE_P2_AREA2_DUST_PARTICLE_TABLE);
}

void ae_PlayerFxSpawn10C(AeRam *r, const AeSpawnHooks *hooks)
{
    ae_SpawnParticle(r, hooks, 0x10C, AE_P2_G_PLAYER_FX_POSITION);
}

static void ae_p2_effect_ground_impact(AeRam *r, const AeWorldHooks *world,
                                       const AeSpawnHooks *spawn, const AePlayer2Hooks *hooks,
                                       int32_t x, int32_t y, int32_t z,
                                       uint32_t original_function)
{
    uint32_t position = AE_P2_LOCAL_GROUND_IMPACT;
    int32_t ground_y;

    x = ae_p2_s16((uint32_t)x);
    y = ae_p2_s16((uint32_t)y);
    z = ae_p2_s16((uint32_t)z);
    ground_y = ae_GetGroundHeight(r, world, x, y, z);
    if (ae_ram_failed(r)) return;

    ae_wr16(r, position, (uint32_t)x);
    ae_wr16(r, position + 2u, (uint32_t)(ground_y - 0x10));
    ae_wr16(r, position + 4u, (uint32_t)z);
    if (!ae_p2_set_position_padding(r, hooks, position, original_function)) return;
    ae_EffectSpawnFromDef(r, spawn, position, 0x800B1BD4u, 1, 4, 0);
    if (ae_ram_failed(r)) return;

    ae_CameraShake(r, 1);
    for (uint32_t index = 0; index < 25u && !ae_ram_failed(r); ++index) {
        uint32_t random_x = ae_p2_random_byte(r);
        uint32_t random_y = ae_p2_random_byte(r) >> 2;
        uint32_t random_z = ae_p2_random_byte(r);
        ae_wr16(r, position,
                (uint32_t)(x + (int32_t)(random_x & 0xFFu) - 0x80));
        ae_wr16(r, position + 2u,
                (uint32_t)(ground_y - 0x10 + (int32_t)random_y));
        ae_wr16(r, position + 4u,
                (uint32_t)(z + (int32_t)(random_z & 0xFFu) - 0x80));
        ae_SpawnParticle(r, spawn, 0x16, position);
    }
}

void ae_EffectGroundImpact(AeRam *r, const AeWorldHooks *world, const AeSpawnHooks *spawn,
                           const AePlayer2Hooks *hooks, int32_t x, int32_t y, int32_t z)
{
    ae_p2_effect_ground_impact(r, world, spawn, hooks, x, y, z, 0x8006B570u);
}

void ae_EffectGroundImpactAtPlayer(AeRam *r, const AeWorldHooks *world, const AeSpawnHooks *spawn,
                                   const AePlayer2Hooks *hooks)
{
    int32_t x = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X));
    int32_t y = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Y));
    int32_t z = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Z));

    ae_p2_effect_ground_impact(r, world, spawn, hooks, x, y, z, 0x8006BF84u);
}

void ae_PlayerSparkleBurst(AeRam *r, const AePlayer2Hooks *hooks)
{
    uint32_t position = AE_P2_LOCAL_SPARKLE;

    if (!ae_p2_set_position_padding(r, hooks, position, 0x8006C204u)) return;
    for (uint32_t index = 0; index < 10u && !ae_ram_failed(r); ++index) {
        uint32_t random_x = ae_p2_random_byte(r);
        int32_t x = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X)) +
                    (int32_t)(random_x & 0x7Fu) - 0x40;
        int32_t y = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Y)) - 0x80;
        uint32_t random_z;
        int32_t z;

        ae_wr16(r, position, (uint32_t)x);
        ae_wr16(r, position + 2u, (uint32_t)y);
        random_z = ae_p2_random_byte(r);
        z = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Z)) +
            (int32_t)(random_z & 0x7Fu) - 0x40;
        ae_wr16(r, position + 4u, (uint32_t)z);
        ae_SpawnParticle(r, NULL, 0x12, position);
    }
}

void ae_PlayPlayerVoice(AeRam *r, const AePlayer2Hooks *hooks,
                        uint32_t voice_id, uint32_t fallback_sound)
{
    uint32_t sound_id = fallback_sound;

    switch (voice_id) {
    case 0u:
    case 4u:
        sound_id = 0x86u;
        break;
    case 2u:
    case 5u:
        sound_id = 0x84u;
        break;
    case 3u:
        sound_id = 0x85u;
        break;
    case 6u:
        return;
    case 7u:
        sound_id = 0x87u;
        break;
    case 8u:
        sound_id = 0x88u;
        break;
    case 9u:
        sound_id = 0x89u;
        break;
    case 10u:
        sound_id = 0x8Au;
        break;
    case 11u:
        sound_id = 0x8Bu;
        break;
    default:
        break;
    }

    (void)ae_p2_play_sfx(r, hooks, sound_id);
}

void ae_PlayerOnWallBump(AeRam *r, const AePlayer2Hooks *hooks, uint32_t normal)
{
    int32_t facing_x;
    int32_t facing_z;
    int32_t normal_x;
    int32_t normal_z;
    int32_t dot;
    int32_t move_x;
    int32_t move_z;

    if (ae_rds16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_HEALTH) <= 0) return;

    facing_x = ae_rds16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING);
    facing_z = ae_rds16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING + 4u);
    normal_x = ae_rds16(r, normal);
    normal_z = ae_rds16(r, normal + 4u);
    dot = ae_p2_add_wrap(ae_p2_mul_wrap(facing_x, normal_x),
                         ae_p2_mul_wrap(facing_z, normal_z));

    if (ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Y) < 0) {
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Y, 0);
    }
    if (!ae_p2_play_sfx(r, hooks, 0x2Bu)) return;

    move_x = ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X);
    move_z = ae_rds32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z);
    if (dot <= 0) {
        ae_PlayerSetState(r, NULL, 0x42u);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X, move_x / 2);
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z, move_z / 2);
    } else {
        ae_PlayerSetState(r, NULL, 0x2Fu);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_X, move_x / 4);
        ae_wr32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_MOVE_VEL_Z, move_z / 4);
    }
    ae_wr8(r, AE_P2_G_PLAYER + AE_P2_PLAYER_WALL_BUMP_MARKER, 0);
}

void ae_AimTableClear3(AeRam *r)
{
    uint32_t index;

    for (index = 0; index < 3u; ++index) {
        ae_wr32(r, AE_P2_G_AIM_TABLE_LAST - index * AE_P2_AIM_TABLE_STRIDE, 0);
    }
}

void ae_PlayerFxClearDAT_800F5144(AeRam *r)
{
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_COUNT, 0xFFFFu);
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_FLAGS, 0);
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_TIMER, 0);
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_MODE, 0);
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_FRAME, 0);
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_VALUE, 0x1000u);
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_FLAGS_2, 0);
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_GADGET_FX_CLEAR_FLAGS_3, 0);
}

int32_t ae_IsPauseMenuOpen(AeRam *r)
{
    return ae_rds8(r, AE_P2_G_PAUSE_MENU_MODE);
}

void ae_RadarResetState(AeRam *r)
{
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_POSITION,
            ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING));
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_DIRECTION,
            ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING + AE_P2_VECTOR_WORD_SIZE));
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_TARGET, 0);
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_TIMER, 0);
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_MARKER_A, 0xFFFFu);
    ae_wr16(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_MARKER_B, 0xFFFFu);
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_FLAGS, 0);
    ae_wr32(r, AE_P2_G_GADGET4_STATE + AE_P2_RADAR_FLAGS_2, 0);
}

int32_t ae_Max16(AeRam *r, int32_t left, int32_t right)
{
    int32_t left_s16 = ae_p2_s16((uint32_t)left);
    int32_t right_s16 = ae_p2_s16((uint32_t)right);

    (void)r;
    return left_s16 <= right_s16 ? right_s16 : left_s16;
}

void ae_SpuSetMainVolume(AeRam *r, int32_t left, int32_t right)
{
    uint32_t shadow = ae_rd32(r, AE_P2_G_SPU_SHADOW);
    uint32_t left_volume = (uint32_t)left << 8;
    uint32_t right_volume = (uint32_t)right << 8;

    if (ae_rd16(r, AE_P2_G_SOUND_MONO) != 0) {
        int32_t left_byte = ae_p2_s8((uint32_t)left) * 0x100;
        int32_t right_byte = ae_p2_s8((uint32_t)right) * 0x100;
        uint32_t mono_volume = (uint32_t)ae_Max16(r, left_byte, right_byte);

        left_volume = mono_volume;
        right_volume = mono_volume;
    }

    ae_wr16(r, shadow + AE_P2_SPU_MAIN_LEFT, left_volume);
    ae_wr16(r, shadow + AE_P2_SPU_MAIN_RIGHT, right_volume);
}

int32_t ae_SoundSetVolumeMode(AeRam *r, int32_t mode)
{
    int32_t volume;

    if (mode == 1) {
        volume = ae_rds16(r, AE_P2_G_MAIN_VOLUME);
    } else if (mode < 2) {
        if (mode == 0) {
            int32_t reverb = ae_rds16(r, AE_P2_G_REVERB_VOLUME);
            volume = ae_p2_s16((uint32_t)((reverb * 0x7F) / 0x4B));
        } else {
            volume = 0x7F;
        }
    } else if (mode == 2) {
        volume = 0;
    } else {
        volume = 0x7F;
    }

    ae_SpuSetMainVolume(r, volume, volume);
    return 0;
}

void ae_RcCarSpawn(AeRam *r, const AePlayer2Hooks *player_hooks, const AeBodyHooks *body_hooks,
                   uint32_t x, uint32_t y, uint32_t z)
{
    if (!player_hooks || !player_hooks->PlayerCanActivateGadget) {
        ae_ram_fail(r, "AePlayer2Hooks.PlayerCanActivateGadget not provided");
        return;
    }
    if (player_hooks->PlayerCanActivateGadget(player_hooks->user, r, 7) == 0) return;

    if (ae_rd32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_ACTIVE) != 0) {
        if (ae_rds16(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_COOLDOWN) > 0) return;
        AeAreaSetHooks area_hooks = {0};
        area_hooks.body_hooks = body_hooks;
        ae_RcCarDespawn(r, &area_hooks);
        if (ae_ram_failed(r)) return;
    }

    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_ACTIVE, 1);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_BODY, x);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_BODY + 4u, y);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_BODY + 8u, z);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_VELOCITY, 0);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_VELOCITY + 4u, 0);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_VELOCITY + 8u, 0);
    for (uint32_t offset = 0; offset < AE_P2_PLAYER_FACING_AND_UP_SIZE; offset += 4u) {
        ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_FACING_AND_UP + offset,
                ae_rd32(r, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING + offset));
    }
    ae_wr16(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_RADIUS, 0x40u);
    ae_wr16(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_HEIGHT, 0x40u);
    ae_wr32(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_FLAGS, 0);
    (void)ae_CollisionBodyRegister(r, body_hooks, 2, AE_P2_G_RC_CAR + AE_P2_RC_CAR_BODY,
                                   AE_P2_G_RC_CAR);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, AE_P2_G_RC_CAR + AE_P2_RC_CAR_COOLDOWN, 10u);
}

void ae_SpawnRandomParticle(AeRam *r, uint32_t position, uint32_t variant,
                            uint32_t kind, int32_t speed)
{
    uint32_t cursor = ae_rd8(r, AE_P2_G_RANDOM_PARTICLE_CURSOR);
    uint32_t particle = AE_P2_G_RANDOM_PARTICLES + cursor * AE_P2_RANDOM_PARTICLE_STRIDE;
    uint32_t seed_for_size;
    uint32_t seed_x;
    uint32_t seed_y;
    uint32_t seed_z;
    int32_t velocity_x;
    int32_t velocity_y;
    int32_t velocity_z;

    ae_wr8(r, particle + AE_P2_RANDOM_PARTICLE_TYPE, 7u);
    seed_for_size = ae_p2_advance_random_seed(r);
    ae_wr8(r, particle + AE_P2_RANDOM_PARTICLE_VARIANT, variant);
    seed_x = ae_p2_advance_random_seed(r);
    seed_y = ae_p2_advance_random_seed(r);
    seed_z = ae_p2_advance_random_seed(r);

    ae_wr32(r, particle + AE_P2_RANDOM_PARTICLE_POINTER,
            AE_P2_G_RANDOM_PARTICLE_TABLE + (kind & 0xFFu) * 4u);
    ae_wr32(r, particle + AE_P2_RANDOM_PARTICLE_POSITION, position);
    ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SIZE, (seed_for_size >> 4) & 0xFF0u);

    velocity_x = ae_p2_shift_right(
            ae_p2_mul_wrap((int32_t)(seed_x >> 8) - 0x80, speed), 8);
    velocity_y = ae_p2_shift_right(
            ae_p2_mul_wrap(-(int32_t)(seed_y >> 8), speed), 8);
    velocity_z = ae_p2_shift_right(
            ae_p2_mul_wrap((int32_t)(seed_z >> 8) - 0x80, speed), 8);
    ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_VELOCITY, (uint32_t)velocity_x);
    ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_VELOCITY + 2u, (uint32_t)velocity_y);
    ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_VELOCITY + 4u, (uint32_t)velocity_z);

    if ((kind & 0x100u) != 0) {
        uint32_t seed_for_secondary_y = ae_p2_advance_random_seed(r);
        velocity_y = ae_p2_shift_right(
                ae_p2_mul_wrap((int32_t)(seed_for_secondary_y >> 8) - 0x80, speed), 8);
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_VELOCITY + 2u,
                (uint32_t)velocity_y);
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SPREAD,
                (uint32_t)ae_p2_sub_wrap(0, velocity_x));
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SPREAD + 2u,
                (uint32_t)ae_p2_sub_wrap(0, velocity_y));
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SPREAD + 4u,
                (uint32_t)ae_p2_sub_wrap(0, velocity_z));
    } else {
        uint32_t seed_spread_x = ae_p2_advance_random_seed(r);
        uint32_t seed_spread_y = ae_p2_advance_random_seed(r);
        uint32_t seed_spread_z = ae_p2_advance_random_seed(r);
        int32_t spread_x = ae_p2_shift_right(
                ae_p2_mul_wrap((int32_t)(seed_spread_x >> 8) - 0x80, speed), 7);
        int32_t spread_y = ae_p2_shift_right(
                ae_p2_mul_wrap((int32_t)(seed_spread_y >> 8) - 0x80, speed), 7);
        int32_t spread_z = ae_p2_shift_right(
                ae_p2_mul_wrap((int32_t)(seed_spread_z >> 8) - 0x80, speed), 7);
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SPREAD, (uint32_t)spread_x);
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SPREAD + 2u, (uint32_t)spread_y);
        ae_wr16(r, particle + AE_P2_RANDOM_PARTICLE_SPREAD + 4u, (uint32_t)spread_z);
    }

    ae_wr8(r, AE_P2_G_RANDOM_PARTICLE_CURSOR, (cursor + 1u) & 7u);
}

static int32_t ae_p2_random_quarter_offset(uint32_t seed)
{
    int32_t offset = (int32_t)(seed >> 8) - 0x80;
    if (offset < 0) offset = ae_p2_add_wrap(offset, 3);
    return ae_p2_shift_right(offset, 2);
}

static void ae_p2_write_squash_direction(AeRam *r, uint32_t direction)
{
    for (uint32_t axis = 0; axis < 3u; axis++) {
        uint32_t seed = ae_p2_advance_random_seed(r);
        ae_wr16(r, direction + axis * 2u,
                (seed >> 4) & 0xFF0u);
    }
}

static void ae_p2_player_fx_squash_particles(AeRam *r, const AeSpawnHooks *spawn,
                                             const AePlayer2Hooks *hooks,
                                             uint32_t direction)
{
    uint32_t frame = ae_rd16(r, AE_P2_G_PLAYER_SQUASH_FRAME);
    uint32_t seed = ae_rd16(r, AE_P2_RAND_SEED);
    uint32_t random_x = (seed * 5u + 1u) & 0xFFFFu;
    uint32_t random_y = (random_x * 5u + 1u) & 0xFFFFu;
    uint32_t random_z = (random_y * 5u + 1u) & 0xFFFFu;
    int32_t player_x = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_X));
    int32_t player_y = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Y));
    int32_t player_z = ae_p2_s16(ae_rd16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_POSITION_Z));

    ae_wr16(r, AE_P2_G_SQUASH_PARTICLE_POSITION,
            (uint32_t)(player_x + ae_p2_random_quarter_offset(random_x)));
    ae_wr16(r, AE_P2_G_SQUASH_PARTICLE_POSITION + 2u,
            (uint32_t)(player_y - ((random_y >> 11) + 0x60u)));
    ae_wr16(r, AE_P2_RAND_SEED, random_z);
    ae_wr16(r, AE_P2_G_SQUASH_PARTICLE_POSITION + 4u,
            (uint32_t)(player_z + ae_p2_random_quarter_offset(random_z)));

    if (frame == 0x1Du) {
        if (!ae_p2_play_sfx(r, hooks, 8u) || !ae_p2_play_sfx(r, hooks, 9u)) return;
    }
    if (frame >= 0x11u && frame <= 0x1Du) {
        for (uint32_t index = 0; index < 2u; index++) {
            uint32_t size_seed = ae_p2_advance_random_seed(r);
            uint32_t speed = ((size_seed >> 4) & 0xFF0u) + 0x400u;
            ae_SpawnRandomParticle(r, AE_P2_G_SQUASH_PARTICLE_POSITION, 0,
                                   0x101u, (int32_t)speed);
        }
        if ((frame & 3u) == 0) {
            ae_SpawnParticle(r, spawn, 0x112, AE_P2_G_PLAYER_FX_POSITION);
            if (ae_ram_failed(r)) return;
        }
    }
    if (frame == 0x14u) {
        for (uint32_t index = 0; index < 3u; index++) {
            ae_p2_write_squash_direction(r, direction);
            ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                                  AE_P2_SQUASH_EFFECT_RING, 1, (int32_t)(index * 2u),
                                  direction);
            if (ae_ram_failed(r)) return;
        }
    }
    if (frame == 0x12u) {
        ae_TrailSpawn(r, AE_P2_G_PLAYER_FX_POSITION, 1, -230, 0);
        ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                              AE_P2_SQUASH_EFFECT_TRAIL, 1, 0, 0x800EC240u);
        if (ae_ram_failed(r)) return;
    }
    if (frame == 10u) {
        ae_TrailSpawn(r, AE_P2_G_PLAYER_FX_POSITION, 1, 0xF0, 0);
        ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                              AE_P2_SQUASH_EFFECT_BURST, 1, 4, 0);
        if (ae_ram_failed(r) || !ae_p2_play_sfx(r, hooks, 0x0Cu)) return;
    }
    if (frame == 6u) {
        for (uint32_t index = 0; index < 0x28u; index++) {
            ae_SpawnParticle(r, spawn, 0x29, AE_P2_G_PLAYER_FX_POSITION);
            if (ae_ram_failed(r)) return;
        }
        if (!ae_p2_play_sfx(r, hooks, 0x0Bu)) return;
    }
    if (frame == 1u) (void)ae_p2_play_sfx(r, hooks, 10u);
}

void ae_PlayerFxSquashParticles(AeRam *r, const AeSpawnHooks *spawn,
                                const AePlayer2Hooks *hooks)
{
    ae_p2_player_fx_squash_particles(r, spawn, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);
}

static void ae_p2_player_fx_flattened_particles(AeRam *r, const AeSpawnHooks *spawn,
                                                const AePlayer2Hooks *hooks,
                                                uint32_t direction)
{
    uint32_t frame = ae_rd16(r, AE_P2_G_PLAYER_SQUASH_FRAME);
    int32_t signed_frame = ae_p2_s16(frame);

    if (signed_frame < 8 && (frame & 3u) == 0) {
        ae_SpawnParticle(r, spawn, 0x112, AE_P2_G_PLAYER_FX_POSITION);
        if (ae_ram_failed(r)) return;
    }
    if (frame == 1u) {
        for (uint32_t index = 0; index < 4u; index++) {
            ae_p2_write_squash_direction(r, direction);
            ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                                  AE_P2_SQUASH_EFFECT_RING, 1, (int32_t)(index * 2u),
                                  direction);
            if (ae_ram_failed(r)) return;
        }
        ae_TrailSpawn(r, AE_P2_G_PLAYER_FX_POSITION, 1, 0xE0, 0);
        ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                              AE_P2_SQUASH_EFFECT_BURST, 2, 4, 0);
        if (ae_ram_failed(r)) return;
        ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                              AE_P2_SQUASH_EFFECT_TRAIL, 1, 0, 0x800EC240u);
        if (ae_ram_failed(r) || !ae_p2_play_sfx(r, hooks, 0x0Cu)) return;
    }
    if (frame == 7u) {
        for (uint32_t index = 0; index < 0x28u; index++) {
            ae_SpawnParticle(r, spawn, 0x29, AE_P2_G_PLAYER_FX_POSITION);
            if (ae_ram_failed(r)) return;
        }
        ae_EffectSpawnFromDef(r, spawn, AE_P2_G_PLAYER_FX_POSITION,
                              AE_P2_SQUASH_EFFECT_FINAL, 1, 0, 0);
        if (ae_ram_failed(r) || !ae_p2_play_sfx(r, hooks, 10u)) return;
    }
}

void ae_PlayerFxFlattenedParticles(AeRam *r, const AeSpawnHooks *spawn,
                                   const AePlayer2Hooks *hooks)
{
    ae_p2_player_fx_flattened_particles(r, spawn, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);
}

static int ae_p2_call_address_hook(AeRam *r, const AePlayer2Hooks *hooks,
                                   AePlayer2AddressHookFn hook, const char *name,
                                   uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (!hooks || !hook) {
        ae_ram_fail(r, "player2 hook %s not provided", name);
        return 0;
    }
    hook(hooks->user, r, a0, a1, a2, a3);
    return !ae_ram_failed(r);
}

static void ae_p2_draw_squashed(AeRam *r, const AePlayer2Hooks *hooks,
                                uint32_t local_scale)
{
    uint32_t player = AE_P2_G_PLAYER;
    int32_t squash_frame = ae_rds16(r, player + AE_P2_PLAYER_SQUASH_FRAME);
    if (ae_ram_failed(r) || squash_frame >= 8) return;

    uint32_t model = ae_rd32(r, player + AE_P2_PLAYER_MODEL);
    uint32_t matrices = ae_rd32(r, player + AE_P2_PLAYER_BONE_MATRICES);
    if (ae_ram_failed(r)) return;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->AnimDecodeFrame : NULL,
                                 "AnimDecodeFrame", player, 0, 0, 0)) return;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->MatrixFromFrontUp : NULL,
                                 "MatrixFromFrontUp",
                                 player + AE_P2_PLAYER_FRONT_VECTOR,
                                 player + AE_P2_PLAYER_UP_VECTOR, matrices, 0)) return;

    uint32_t scale_x = 0x1000u - ((uint32_t)squash_frame << 9);
    uint32_t scale_y = ((uint32_t)squash_frame << 11) + 0x1000u;
    ae_wr32(r, local_scale, scale_x);
    ae_wr32(r, local_scale + 4u, scale_y);
    ae_wr32(r, local_scale + 8u, scale_x);
    if (ae_ram_failed(r)) return;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ScaleMatrixL : NULL,
                                 "ScaleMatrixL", matrices,
                                 local_scale, 0, 0)) return;

    ae_wr32(r, matrices + 20u, 0);
    ae_wr32(r, matrices + 24u, (uint32_t)squash_frame << 5);
    ae_wr32(r, matrices + 28u, 0);
    if (ae_ram_failed(r)) return;
    ae_ModelBuildBoneMatrices(r, player, 7u);
    if (ae_ram_failed(r)) return;

    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ModelTransformSkinned : NULL,
                                 "ModelTransformSkinned", player, 0, 0, 0)) return;
    uint32_t vertex_count = ae_rd16(r, model + AE_P2_MODEL_VERTEX_COUNT);
    if (ae_ram_failed(r)) return;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ModelPrepCycleColors : NULL,
                                 "ModelPrepCycleColors", AE_P2_SCREEN_VERTICES,
                                 vertex_count, 0, 0)) return;
    uint32_t ordering_table = ae_rd32(r, AE_P2_G_ORDERING_TABLE);
    if (ae_ram_failed(r)) return;
    (void)ae_p2_call_address_hook(r, hooks,
                                  hooks ? hooks->ModelEmitPrimsCycleColors : NULL,
                                  "ModelEmitPrimsCycleColors", ordering_table,
                                  AE_P2_SCREEN_VERTICES, player, 0);
}

void ae_PlayerDrawSquashed(AeRam *r, const AePlayer2Hooks *hooks)
{
    ae_p2_draw_squashed(r, hooks, AE_P2_LOCAL_SQUASH_SCALE);
}

void ae_PlayerDrawSquashedWithFx(AeRam *r, const AePlayer2Hooks *hooks,
                                 const AeSpawnHooks *spawn)
{
    ae_p2_draw_squashed(r, hooks, AE_P2_NESTED_SQUASH_SCALE);
    if (ae_ram_failed(r)) return;
    if (ae_rds16(r, AE_P2_G_PLAYER + AE_P2_PLAYER_SQUASH_FRAME) < 0x1E) {
        ae_p2_player_fx_squash_particles(r, spawn, hooks, AE_P2_NESTED_SQUASH_DIRECTION);
    }
}

void ae_PlayerDrawSquashedFlattened(AeRam *r, const AePlayer2Hooks *hooks,
                                    const AeSpawnHooks *spawn)
{
    uint32_t player = AE_P2_G_PLAYER;
    ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_X, 0);
    ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_Y, 0);
    ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_Z, 0);
    if (ae_ram_failed(r)) return;

    ae_p2_draw_squashed(r, hooks, AE_P2_NESTED_SQUASH_SCALE);
    if (ae_ram_failed(r)) return;
    if (ae_rds16(r, player + AE_P2_PLAYER_SQUASH_FRAME) < 16) {
        ae_p2_player_fx_flattened_particles(r, spawn, hooks,
                                            AE_P2_NESTED_SQUASH_DIRECTION);
        if (ae_ram_failed(r)) return;
    }
    ae_wr16(r, player + AE_P2_PLAYER_SQUASH_FRAME,
            ae_rd16(r, player + AE_P2_PLAYER_SQUASH_FRAME) + 1u);
}

void ae_PlayerUpdateIdleAnim(AeRam *r, const AePlayer2Hooks *hooks,
                             const AePlayerActHooks *act)
{
    uint32_t idle = ae_rd32(r, AE_P2_G_IDLE_ANIMATION);
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t actor = idle + AE_P2_IDLE_MODEL_ACTOR;
    uint32_t draw_buffer = ae_rd32(r, AE_P2_G_DRAW_BUFFER_INDEX);
    uint32_t frame = ae_rd32(r, AE_P2_G_IDLE_FRAME_COUNTER) & 0x1Fu;
    if (ae_ram_failed(r)) return;

    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_STATE_TABLE,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_STATE_TABLE));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_TRANSITIONS));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_EVENTS,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_EVENTS));
    ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X, 0);
    ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, 0);
    ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z, 0);
    ae_wr16(r, player + AE_P2_PLAYER_ANIM_FRAME,
            ae_rd16(r, player + AE_P2_PLAYER_ANIM_FRAME) + 1u);
    ae_wr32(r, player + AE_P2_PLAYER_ANIM_SET,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_ANIM_SET));
    if (ae_ram_failed(r)) return;

    ae_PlayerRunTransitions(r, hooks, act,
                            ae_rd32(r, idle + AE_P2_IDLE_MODEL_TRANSITIONS));
    if (ae_ram_failed(r)) return;
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr32(r, idle + AE_P2_IDLE_MODEL_POSITION + axis * 4u,
                ae_rd32(r, player + AE_P2_PLAYER_POSITION_X + axis * 4u));
    }
    ae_wr16(r, idle + AE_P2_IDLE_MODEL_ANIM_FRAME,
            ae_rd16(r, player + AE_P2_PLAYER_ANIM_FRAME));
    if (ae_ram_failed(r)) return;

    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->MatrixFromFrontUp : NULL,
                                 "MatrixFromFrontUp",
                                 player + AE_P2_PLAYER_FRONT_VECTOR,
                                 player + AE_P2_PLAYER_UP_VECTOR,
                                 ae_rd32(r, idle + AE_P2_IDLE_MODEL_MATRIX_PTR), 1u)) return;
    ae_ModelPoseBones(r, actor, 1u);
    if (ae_ram_failed(r)) return;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ModelTransformSkinned : NULL,
                                 "ModelTransformSkinned", actor, 0, 0, 0)) return;

    uint32_t model_draw_buffer = AE_P2_G_IDLE_MODEL_DRAW_BASE + draw_buffer *
                                 AE_P2_G_IDLE_MODEL_DRAW_STRIDE;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ModelDraw : NULL,
                                 "ModelDraw", model_draw_buffer, AE_P2_SCREEN_VERTICES,
                                 actor, 0)) return;
    if (ae_rds16(r, idle + AE_P2_IDLE_MODEL_STATE) == 5) {
        if (frame > 16u) frame = 32u - frame;
        if (!ae_p2_call_address_hook(r, hooks,
                                     hooks ? hooks->DrawRibbonBetween : NULL,
                                     "DrawRibbonBetween", AE_P2_G_IDLE_RIBBON_START,
                                     AE_P2_G_IDLE_RIBBON_END, frame + 0x18u,
                                     0x00FF8080u)) return;
    }

    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_STATE_TABLE,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_STATE_TABLE));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_TRANSITIONS));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_EVENTS,
            ae_rd32(r, idle + AE_P2_IDLE_MODEL_EVENTS));
}

static int32_t ae_p2_gadget_spawn_particle(void *user, AeRam *r,
                                           uint32_t kind, uint32_t position,
                                           uint32_t unused_a2, uint32_t unused_a3,
                                           uint32_t unused_a4)
{
    const AeSpawnHooks *spawn = (const AeSpawnHooks *)user;
    (void)unused_a2;
    (void)unused_a3;
    (void)unused_a4;
    ae_SpawnParticle(r, spawn, kind, position);
    return ae_ram_failed(r) ? 0 : 1;
}

static int ae_p2_stop_gadget_sounds(AeRam *r, const AePlayer2Hooks *hooks, int32_t gadget)
{
    if (!hooks || !hooks->PlayerStopGadgetSounds) {
        ae_ram_fail(r, "player2 hook PlayerStopGadgetSounds not provided");
        return 0;
    }
    hooks->PlayerStopGadgetSounds(hooks->user, r, gadget);
    return !ae_ram_failed(r);
}

static int ae_p2_call_sfx(AeRam *r, const AeSoundHw *sound, int32_t sound_id)
{
    (void)ae_PlaySfx(r, sound, sound_id);
    return !ae_ram_failed(r);
}

static void ae_p2_call_aim_mode_leave(AeRam *r, const AeSoundHw *sound)
{
    AePlayerSpawnHooks spawn_hooks = {0};
    spawn_hooks.sound = sound;
    ae_AimModeLeave(r, &spawn_hooks);
}

static void ae_p2_call_gadget_select_fx(AeRam *r, const AeSpawnHooks *spawn)
{
    AeGadgetHooks gadget_hooks = {0};
    gadget_hooks.user = (void *)spawn;
    gadget_hooks.SpawnParticle = ae_p2_gadget_spawn_particle;
    ae_GadgetSelectFx(r, &gadget_hooks);
}

int32_t ae_PlayerSelectGadget(AeRam *r, const AePlayer2Hooks *hooks,
                              const AePlayerActHooks *act, const AeSpawnHooks *spawn,
                              const AeBodyHooks *bodies, const AeSoundHw *sound,
                              int32_t gadget, int32_t previous_gadget)
{
    uint32_t player = AE_P2_G_PLAYER;
    int32_t stop_gadget = -1;
    if (previous_gadget == 2) {
        if (gadget != 2) stop_gadget = 2;
    } else if (previous_gadget < 3) {
        if (previous_gadget == 0 && gadget != 0) stop_gadget = 0;
    } else if (previous_gadget == 4) {
        stop_gadget = 4;
    } else if (previous_gadget == 6 && gadget != 6) {
        stop_gadget = 6;
    }
    if (stop_gadget >= 0 && !ae_p2_stop_gadget_sounds(r, hooks, stop_gadget)) return 0;

    switch (gadget) {
    case 0:
        if (previous_gadget != 0) {
            ae_PlayerFxClearDAT_800F5144(r);
            if (!ae_p2_call_simple_hook(r, hooks,
                                        hooks ? hooks->TrailRingReset : NULL,
                                        "TrailRingReset")) return 0;
        }
        break;
    case 2:
        if (previous_gadget != 2) ae_RadarResetState(r);
        break;
    case 3:
        if (ae_rds8(r, player + AE_P2_PLAYER_CURRENT_GADGET) == gadget) {
            int32_t timer = ae_rds8(r, AE_P2_G_GADGET3_FRAME);
            int32_t incremented = (int8_t)(timer + 1);
            int32_t third = (int8_t)((timer + 1) / 3);
            ae_wr8(r, AE_P2_G_GADGET3_DELAY, 0x1Cu);
            ae_wr8(r, AE_P2_G_GADGET3_FRAME, (uint32_t)(incremented - third * 3));
            if (!ae_p2_call_sfx(r, sound, 3)) return 0;
            ae_AimTableClear3(r);
        } else {
            ae_wr8(r, AE_P2_G_GADGET3_REPEAT, 1);
            ae_wr16(r, AE_P2_G_GADGET3_WAIT, 400);
            ae_wr8(r, AE_P2_G_GADGET3_FRAME, 0);
            ae_wr16(r, AE_P2_G_GADGET3_FLAG, 0);
        }
        if (ae_rd8(r, AE_P2_G_AIM_REQUEST) == 1u) {
            if (!hooks || !hooks->AimModeEnter) {
                ae_ram_fail(r, "player2 hook AimModeEnter not provided");
                return 0;
            }
            hooks->AimModeEnter(hooks->user, r);
        }
        break;
    case 4:
        ae_wr16(r, AE_P2_G_GADGET4_COUNTER, 0);
        ae_wr32(r, AE_P2_G_GADGET4_PHASE, 0);
        ae_wr32(r, AE_P2_G_GADGET4_HEIGHT, 0);
        ae_wr32(r, AE_P2_G_GADGET4_DISTANCE, 0);
        ae_wr32(r, AE_P2_G_GADGET4_FLAGS, 0);
        ae_wr16(r, AE_P2_G_GADGET4_TRAIL_A, 0xFFFFu);
        ae_wr16(r, AE_P2_G_GADGET4_TRAIL_B, 0xFFFFu);
        ae_Gadget4TrailReset(r);
        break;
    case 5:
        ae_wr16(r, AE_P2_G_GADGET4_COUNTER, 0x400u);
        ae_wr32(r, AE_P2_G_GADGET4_PHASE,
                ae_rd32(r, AE_P2_G_GADGET4_PHASE) & 0xFFFF0000u);
        ae_wr32(r, AE_P2_G_GADGET4_FLAGS, 0);
        ae_wr32(r, AE_P2_G_GADGET4_FLAGS_2, 0);
        ae_wr16(r, AE_P2_G_GADGET4_RATE, 0x400u);
        break;
    case 6:
        if (previous_gadget != 6) ae_wr32(r, AE_P2_G_GADGET4_PHASE, 0);
        break;
    case 7:
        if ((ae_rd32(r, player + AE_P2_PLAYER_STATE_FLAGS) & 0xF0u) != 0xC0u &&
            previous_gadget == gadget) {
            ae_RcCarSpawn(r, hooks, bodies,
                          ae_rd32(r, player + AE_P2_PLAYER_POSITION_X),
                          ae_rd32(r, player + AE_P2_PLAYER_POSITION_Y),
                          ae_rd32(r, player + AE_P2_PLAYER_POSITION_Z));
            if (ae_ram_failed(r)) return 0;
            ae_CameraSlotFlagBit81(r, NULL);
            if (!ae_p2_call_sfx(r, sound, 0x2E)) return 0;
        }
        if (ae_rd8(r, AE_P2_G_AIM_MODE) == 1u) ae_p2_call_aim_mode_leave(r, sound);
        break;
    default:
        break;
    }
    if (ae_ram_failed(r)) return 0;

    ae_wr8(r, player + AE_P2_PLAYER_CURRENT_GADGET, (uint32_t)gadget);
    ae_wr8(r, player + AE_P2_PLAYER_SAVED_GADGET, (uint32_t)previous_gadget);
    if (gadget != 3 && ae_rd8(r, AE_P2_G_AIM_MODE) == 1u)
        ae_p2_call_aim_mode_leave(r, sound);
    if (ae_ram_failed(r)) return 0;
    if (previous_gadget == gadget) return 0;

    if (previous_gadget != -1 && gadget != 9 && !ae_p2_call_sfx(r, sound, 0x24)) return 0;
    if (ae_rd16(r, player + AE_P2_PLAYER_STATE) > 0x7Fu) {
        uint32_t new_state = ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y) <
                             ae_rds32(r, player + AE_P2_PLAYER_GROUND_Y) ? 0x24u : 0u;
        ae_PlayerSetState(r, act, new_state);
    }
    ae_PlayerApplyGadgetStance(r, act);
    if (ae_ram_failed(r)) return 0;
    if (ae_IsPauseMenuOpen(r) == 0) ae_p2_call_gadget_select_fx(r, spawn);
    if (ae_ram_failed(r)) return 0;
    if (gadget == 7) ae_PlayerFxResetBlink(r);
    return ae_ram_failed(r) ? 0 : 1;
}

static int32_t ae_p2_average_s32(int32_t left, int32_t right)
{
    int32_t sum = ae_p2_add_wrap(left, right);
    uint32_t sign = (uint32_t)sum >> 31;
    return ae_p2_shift_right(ae_p2_add_wrap(sum, (int32_t)sign), 1);
}

static int32_t ae_p2_climb_offset(int32_t angle)
{
    int32_t rounded = angle < 0 ? ae_p2_add_wrap(angle, 0x7F) : angle;
    return ae_p2_shift_right(rounded, 7);
}

static int32_t ae_p2_player_vector(int32_t value, uint32_t shift)
{
    int32_t rounded = value < 0
            ? ae_p2_add_wrap(value, (int32_t)((1u << shift) - 1u)) : value;
    return ae_p2_shift_right(rounded, shift);
}

static void ae_p2_set_state_table(AeRam *r)
{
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    int32_t gadget = ae_p2_s8(ae_rd8(r, player + AE_P2_PLAYER_CURRENT_GADGET));
    uint32_t gadget_set = AE_P2_G_PLAYER_STATE_SETS +
                          (uint32_t)(gadget + 1) * AE_P2_STATE_SET_STRIDE;
    uint32_t state_table = AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_STATES;
    if (state >= 0x80u) state_table = gadget_set + AE_P2_STATE_SET_STATES;
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_STATE_TABLE, ae_rd32(r, state_table));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS,
            ae_rd32(r, gadget_set + AE_P2_STATE_SET_TRANSITIONS));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH_EVENTS,
            ae_rd32(r, gadget_set + AE_P2_STATE_SET_EVENTS));
}

static void ae_p2_turn_towards_y(AeRam *r, const AePlayer2Hooks *hooks,
                                 uint32_t target, uint32_t direction)
{
    (void)ae_p2_call_address_hook(r, hooks, hooks ? hooks->TurnTowardsY : NULL,
                                  "TurnTowardsY", target, direction, 0x155u, 0);
}

void ae_PlayerUpdateClimb(AeRam *r, const AePlayer2Hooks *hooks,
                          const AePlayerActHooks *act)
{
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t state_table;
    uint32_t state;
    uint32_t state_entry;
    int32_t desired_x;
    int32_t desired_z;
    int32_t climb_speed;
    int32_t target_y;

    ae_p2_set_state_table(r);
    state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    desired_x = ae_rds16(r, player + AE_P2_PLAYER_DESIRED_DIRECTION);
    desired_z = ae_rds16(r, player + AE_P2_PLAYER_DESIRED_DIRECTION + 4u);
    {
        int32_t target_x = ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET) -
                           ae_p2_climb_offset(desired_x);
        int32_t target_z = ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET + 4u) -
                           ae_p2_climb_offset(desired_z);
        int32_t position_x = ae_rds32(r, player + AE_P2_PLAYER_POSITION_X);
        int32_t position_z = ae_rds32(r, player + AE_P2_PLAYER_POSITION_Z);
        if (position_x != target_x)
            ae_wr32(r, player + AE_P2_PLAYER_POSITION_X,
                    (uint32_t)ae_p2_average_s32(position_x, target_x));
        if (position_z != target_z)
            ae_wr32(r, player + AE_P2_PLAYER_POSITION_Z,
                    (uint32_t)ae_p2_average_s32(position_z, target_z));
    }
    ae_wr16(r, player + AE_P2_PLAYER_ANIM_FRAME,
            ae_rd16(r, player + AE_P2_PLAYER_ANIM_FRAME) + 1u);

    climb_speed = 0;
    if (ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_INPUT_BLOCK) == 0) {
        int32_t stick_y = ae_p2_s8(ae_rd8(r, AE_P2_G_CLIMB_STICK_Y));
        climb_speed = ae_p2_player_vector(-stick_y, 3);
    }
    if (climb_speed > 0) climb_speed *= 2;
    ae_wr32(r, player + AE_P2_PLAYER_POSITION_Y,
            (uint32_t)ae_p2_add_wrap(ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y),
                                    climb_speed));
    if (ae_ram_failed(r)) return;

    if (state < 0x38u || state > 0x3Cu) {
        ae_PlayerSetState(r, act, 0x38u);
        if (ae_ram_failed(r)) return;
    }
    if (state < 0x38u || state > 0x3Cu || state == 0x38u) {
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_TRANSITIONS));
        if (ae_ram_failed(r)) return;
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS));
        if (ae_ram_failed(r)) return;
    } else if (state == 0x39u) {
        if (climb_speed > 0) {
            ae_PlayerSetState(r, act, 0x3Bu);
        } else if (climb_speed < 0) {
            ae_PlayerSetState(r, act, 0x3Au);
        }
        if (ae_ram_failed(r)) return;
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_TRANSITIONS));
        if (ae_ram_failed(r)) return;
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS));
        if (ae_ram_failed(r)) return;
    } else if (state == 0x3Au) {
        if (climb_speed > 0) {
            ae_PlayerSetState(r, act, 0x3Bu);
        } else {
            int32_t top = ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET + 2u) -
                          ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET_Y_OFFSET);
            int32_t position_y = ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y);
            if (position_y < top &&
                ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET_HEIGHT) == 1) {
                ae_PlayerSetMode(r, act, 0);
                if (ae_ram_failed(r)) return;
                ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X, 0);
                ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, 0);
                ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z, 0);
                ae_wr32(r, player + AE_P2_PLAYER_POSITION_Y, (uint32_t)(top - 0x100));
                ae_PlayerSetState(r, act, 0x0Eu);
                return;
            }
            if (position_y < top) {
                ae_wr32(r, player + AE_P2_PLAYER_POSITION_Y, (uint32_t)top);
            }
            if (climb_speed == 0) {
                uint32_t current_frame = ae_rd16(r, player + AE_P2_PLAYER_ANIM_FRAME);
                uint32_t entry = ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_STATE_TABLE) +
                                 state * AE_P2_STATE_ENTRY_SIZE;
                uint32_t frame_delta = current_frame -
                                      ae_rd16(r, entry + AE_P2_STATE_START_FRAME);
                if (frame_delta == 5u || frame_delta == 15u)
                    ae_PlayerSetState(r, act, 0x39u);
            }
        }
        if (ae_ram_failed(r)) return;
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_TRANSITIONS));
        if (ae_ram_failed(r)) return;
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS));
        if (ae_ram_failed(r)) return;
    } else {
        target_y = ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET + 2u);
        if (target_y < ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y)) {
            ae_PlayerSetMode(r, act, 0);
            if (ae_ram_failed(r)) return;
            int32_t facing_x = ae_rds16(r, player + AE_P2_PLAYER_FACING);
            int32_t facing_z = ae_rds16(r, player + AE_P2_PLAYER_FACING + 4u);
            ae_wr32(r, player + AE_P2_PLAYER_POSITION_X,
                    (uint32_t)ae_p2_sub_wrap(ae_rds32(r, player + AE_P2_PLAYER_POSITION_X),
                                             ae_p2_player_vector(facing_x, 5)));
            ae_wr32(r, player + AE_P2_PLAYER_POSITION_Z,
                    (uint32_t)ae_p2_sub_wrap(ae_rds32(r, player + AE_P2_PLAYER_POSITION_Z),
                                             ae_p2_player_vector(facing_z, 5)));
            ae_PlayerSetState(r, act, 0);
            if (ae_ram_failed(r)) return;
        } else if (climb_speed <= 0) {
            ae_PlayerSetState(r, act, 0x39u);
            if (ae_ram_failed(r)) return;
        }
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_TRANSITIONS));
        if (ae_ram_failed(r)) return;
        ae_PlayerRunTransitions(r, hooks, act,
                ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS));
        if (ae_ram_failed(r)) return;
    }

    if (ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_INPUT_BLOCK) == 0) {
        int32_t stick_x = ae_p2_s8(ae_rd8(r, AE_P2_G_CLIMB_STICK_X));
        if (stick_x > 0x78)
            (void)ae_sdk_ApplyMatrixSV(r, AE_P2_G_CLIMB_MATRIX_RIGHT,
                                      player + AE_P2_PLAYER_DESIRED_DIRECTION,
                                      player + AE_P2_PLAYER_DESIRED_DIRECTION);
        if (stick_x < -0x78)
            (void)ae_sdk_ApplyMatrixSV(r, AE_P2_G_CLIMB_MATRIX_LEFT,
                                      player + AE_P2_PLAYER_DESIRED_DIRECTION,
                                      player + AE_P2_PLAYER_DESIRED_DIRECTION);
    }
    ae_p2_turn_towards_y(r, hooks, player + AE_P2_PLAYER_DESIRED_DIRECTION,
                         player + AE_P2_PLAYER_FACING);
    if (ae_ram_failed(r)) return;

    state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    state_table = ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_STATE_TABLE);
    state_entry = state_table + state * AE_P2_STATE_ENTRY_SIZE;
    {
        uint32_t frame = ae_rd16(r, player + AE_P2_PLAYER_ANIM_FRAME);
        uint32_t first = ae_rd16(r, state_entry + AE_P2_STATE_START_FRAME);
        uint32_t last = ae_rd16(r, state_entry + AE_P2_STATE_END_FRAME);
        if (frame < first) frame = last;
        if (last < frame) frame = first;
        ae_wr16(r, player + AE_P2_PLAYER_ANIM_FRAME, frame);
    }
    target_y = ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET + 2u);
    {
        int32_t top = target_y - ae_rds16(r, player + AE_P2_PLAYER_CLIMB_TARGET + 10u);
        int32_t y = ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y);
        if (y < top) y = top;
        if (target_y < y) y = target_y;
        ae_wr32(r, player + AE_P2_PLAYER_POSITION_Y, (uint32_t)y);
    }

    if (((ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_HELD_BUTTONS) &
          ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CHANGED_BUTTONS) & 0xAu) == 0) ||
        state == 0x38u) {
        ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_X, 0);
        ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_Y, 0);
        ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_Z, 0);
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X, 0);
        return;
    }

    ae_PlayerSetState(r, act, 8u);
    if (ae_ram_failed(r)) return;
    {
        int32_t facing_x = ae_rds16(r, player + AE_P2_PLAYER_FACING);
        int32_t facing_z = ae_rds16(r, player + AE_P2_PLAYER_FACING + 4u);
        ae_wr32(r, player + AE_P2_PLAYER_POSITION_X,
                (uint32_t)ae_p2_sub_wrap(ae_rds32(r, player + AE_P2_PLAYER_POSITION_X),
                                         ae_p2_player_vector(facing_x, 5)));
        ae_wr32(r, player + AE_P2_PLAYER_POSITION_Z,
                (uint32_t)ae_p2_sub_wrap(ae_rds32(r, player + AE_P2_PLAYER_POSITION_Z),
                                         ae_p2_player_vector(facing_z, 5)));
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X,
                (uint32_t)ae_p2_s32_from_bits((uint32_t)(-facing_x) << 4));
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z,
                (uint32_t)ae_p2_s32_from_bits((uint32_t)(-facing_z) << 4));
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, (uint32_t)-0x1E000);
    }
    ae_PlayerSetMode(r, act, 0);
}

void ae_PlayerUpdateRope(AeRam *r, const AePlayer2Hooks *hooks,
                         const AePlayerActHooks *act)
{
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t rope = player + AE_P2_PLAYER_ROPE;
    uint32_t state;
    uint32_t state_handler;
    int32_t fraction = ae_rds16(r, rope + AE_P2_ROPE_FRACTION);
    int32_t inverse_fraction = AE_P2_ROPE_FRACTION_SCALE - fraction;
    int32_t endpoint_a[3];
    int32_t endpoint_b[3];

    ae_p2_set_state_table(r);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        endpoint_a[axis] = ae_rds16(r, rope + axis * AE_P2_ROPE_AXIS_STRIDE);
        endpoint_b[axis] = ae_rds16(r, rope + AE_P2_ROPE_ENDPOINT_B +
                                      axis * AE_P2_ROPE_AXIS_STRIDE);
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X + axis * AE_P2_VECTOR_WORD_SIZE, 0);
    }
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        int32_t weighted = ae_p2_add_wrap(ae_p2_mul_wrap(endpoint_a[axis], fraction),
                                          ae_p2_mul_wrap(endpoint_b[axis], inverse_fraction));
        int32_t target = ae_p2_player_vector(weighted, 12);
        int32_t position = ae_rds32(r, player + AE_P2_PLAYER_POSITION_X +
                                       axis * AE_P2_VECTOR_WORD_SIZE);
        if (position != target)
            ae_wr32(r, player + AE_P2_PLAYER_POSITION_X + axis * AE_P2_VECTOR_WORD_SIZE,
                    (uint32_t)ae_p2_average_s32(position, target));
    }
    ae_wr16(r, player + AE_P2_PLAYER_ANIM_FRAME,
            ae_rd16(r, player + AE_P2_PLAYER_ANIM_FRAME) + 1u);
    if (ae_rd8(r, AE_P2_G_AIM_MODE) == 1u) {
        AePlayerSpawnHooks spawn_hooks = {0};
        ae_AimModeLeave(r, &spawn_hooks);
        if (ae_ram_failed(r)) return;
    }
    if (ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_INPUT_BLOCK) == 0 &&
        ae_rd16(r, AE_P2_G_ROPE_MOVE_STICK) != 0) {
        ae_p2_turn_towards_y(r, hooks, AE_P2_G_ROPE_TARGET_DIRECTION,
                             player + AE_P2_PLAYER_FACING);
        if (ae_ram_failed(r)) return;
    }

    state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    state_handler = state;
    if (state < 0x3Du || state > 0x41u) {
        ae_PlayerSetState(r, act, 0x3Du);
        if (ae_ram_failed(r)) return;
    } else if (state == 0x3Eu &&
               ae_rd16(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_ROPE_GESTURE) != 0) {
        ae_PlayerSetState(r, act, 0x40u);
        if (ae_ram_failed(r)) return;
    } else if (state == 0x3Fu &&
               ae_rd16(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_ROPE_GESTURE) != 0) {
        ae_PlayerSetState(r, act, 0x41u);
        if (ae_ram_failed(r)) return;
    }

    if (state_handler == 0x40u || state_handler == 0x41u) {
        int32_t facing_x = ae_rds16(r, player + AE_P2_PLAYER_FACING);
        int32_t facing_z = ae_rds16(r, player + AE_P2_PLAYER_FACING + 4u);
        int32_t rope_normal_x = ae_rds16(r, player + AE_P2_PLAYER_ROPE_NORMAL_X);
        int32_t rope_normal_z = ae_rds16(r, player + AE_P2_PLAYER_ROPE_NORMAL_Z);
        int32_t rope_length = ae_rds16(r, player + AE_P2_PLAYER_ROPE_LENGTH_SQRT);
        int32_t numerator = ae_p2_add_wrap(ae_p2_mul_wrap(facing_x, rope_normal_x),
                                           ae_p2_mul_wrap(facing_z, rope_normal_z));
        int32_t gesture = ae_rd16(r, AE_P2_G_PLAYER_SCRATCH +
                                     AE_P2_SCRATCH_ROPE_GESTURE);
        int32_t speed;
        int32_t fraction_change;
        uint32_t next_fraction;

        if (rope_length == 0 || (numerator == INT32_MIN && rope_length == -1)) {
            ae_ram_fail(r, "PlayerUpdateRope invalid rope length division");
            return;
        }
        speed = numerator / rope_length;
        fraction_change = ae_p2_shift_right(ae_p2_mul_wrap(speed, gesture),
                                            AE_P2_ROPE_GESTURE_SHIFT);
        next_fraction = ae_rd16(r, player + AE_P2_PLAYER_ROPE_FRACTION) -
                        (uint32_t)fraction_change;
        ae_wr16(r, player + AE_P2_PLAYER_ROPE_FRACTION, next_fraction);
    }

    {
        int32_t signed_fraction = ae_p2_s16(
                ae_rd16(r, player + AE_P2_PLAYER_ROPE_FRACTION));
        if (signed_fraction < 0) signed_fraction = 0;
        else if (signed_fraction > AE_P2_ROPE_FRACTION_SCALE)
            signed_fraction = AE_P2_ROPE_FRACTION_SCALE;
        ae_wr16(r, player + AE_P2_PLAYER_ROPE_FRACTION, (uint32_t)signed_fraction);
    }
    ae_PlayerRunTransitions(r, hooks, act,
            ae_rd32(r, AE_P2_G_PLAYER_STATE_SETS + AE_P2_STATE_SET_TRANSITIONS));
    if (ae_ram_failed(r)) return;
    ae_PlayerRunTransitions(r, hooks, act,
            ae_rd32(r, AE_P2_G_PLAYER_SCRATCH_TRANSITIONS));
    if (ae_ram_failed(r)) return;

    state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    if (((ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_HELD_BUTTONS) &
          ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CHANGED_BUTTONS) & 0xAu) != 0) &&
        state != 0x3Du) {
        ae_PlayerSetState(r, act, 0x35u);
        if (ae_ram_failed(r)) return;
        if (ae_rd16(r, AE_P2_G_ROPE_JUMP_ALLOWED) == 0) {
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X, 0);
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z, 0);
        } else {
            int32_t facing_x = ae_rds16(r, player + AE_P2_PLAYER_FACING);
            int32_t facing_z = ae_rds16(r, player + AE_P2_PLAYER_FACING + 4u);
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X, (uint32_t)(facing_x << 4));
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z, (uint32_t)(facing_z << 4));
        }
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, (uint32_t)-0x40000);
        ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Y, (uint32_t)-0x40000);
        ae_wr32(r, player + AE_P2_PLAYER_DESIRED_DIRECTION,
                ae_rd32(r, player + AE_P2_PLAYER_FACING));
        ae_wr32(r, player + AE_P2_PLAYER_DESIRED_DIRECTION + AE_P2_VECTOR_WORD_SIZE,
                ae_rd32(r, player + AE_P2_PLAYER_FACING + AE_P2_VECTOR_WORD_SIZE));
        ae_wr32(r, player + AE_P2_PLAYER_DESIRED_DIRECTION + 2u * AE_P2_VECTOR_WORD_SIZE,
                ae_rd32(r, player + AE_P2_PLAYER_FACING + 2u * AE_P2_VECTOR_WORD_SIZE));
        ae_wr32(r, player + AE_P2_PLAYER_DESIRED_DIRECTION + 3u * AE_P2_VECTOR_WORD_SIZE,
                ae_rd32(r, player + AE_P2_PLAYER_FACING + 3u * AE_P2_VECTOR_WORD_SIZE));
        ae_PlayerSetMode(r, act, 0);
        return;
    }

    if (((ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_HELD_BUTTONS) &
          ae_rd32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CHANGED_BUTTONS) & 0xAu) == 0) ||
        state == 0x3Du) {
        if ((ae_rd32(r, player + AE_P2_PLAYER_ATTACH_FLAGS) & 4u) == 0) {
            ae_PlayerSetMode(r, act, 0);
            if (ae_ram_failed(r)) return;
            ae_PlayerSetState(r, act, 0x24u);
            if (ae_ram_failed(r)) return;
        }
        uint32_t matrices = ae_rd32(r, player + AE_P2_PLAYER_BONE_MATRICES);
        ae_wr32(r, matrices + AE_P2_PLAYER_BONE_MATRIX_TRANSLATION_Y, (uint32_t)-0x100);
        ae_wr32(r, matrices + AE_P2_PLAYER_BONE_MATRIX_TRANSLATION_X, 0);
        ae_wr32(r, matrices + AE_P2_PLAYER_BONE_MATRIX_TRANSLATION_Z, 0);
    }
}

static void ae_p2_contact_rumble(AeRam *r, const AePlayerActHooks *act,
                                 int32_t motor, int32_t strength, int32_t frames)
{
    if (act == NULL || act->PadRumble == NULL) {
        ae_ram_fail(r, "PlayerOnContact PadRumble hook not provided");
        return;
    }
    act->PadRumble(act->user, r, motor, strength, frames);
}

static void ae_p2_contact_damage(AeRam *r, const AePlayerActHooks *act, int32_t amount)
{
    if (act == NULL || act->PlayerDamage == NULL) {
        ae_ram_fail(r, "PlayerOnContact PlayerDamage hook not provided");
        return;
    }
    act->PlayerDamage(act->user, r, amount);
}

static int ae_p2_contact_aim_clear(AeRam *r, const AePlayerActHooks *act)
{
    if (act == NULL || act->AimRequestClear == NULL) {
        ae_ram_fail(r, "PlayerOnContact AimRequestClear hook not provided");
        return 0;
    }
    act->AimRequestClear(act->user, r);
    return !ae_ram_failed(r);
}

static int ae_p2_contact_camera_fixed_off(AeRam *r, const AePlayer2Hooks *hooks)
{
    return ae_p2_call_simple_hook(r, hooks,
                                  hooks ? hooks->CameraFixedModeOff : NULL,
                                  "CameraFixedModeOff");
}

static int ae_p2_contact_sound(AeRam *r, const AeSoundHw *sound, int32_t sound_id)
{
    (void)ae_PlaySfx(r, sound, sound_id);
    return !ae_ram_failed(r);
}

static int ae_p2_contact_voice(AeRam *r, const AePlayer2Hooks *hooks, uint32_t voice)
{
    ae_PlayPlayerVoice(r, hooks, voice, 0);
    return !ae_ram_failed(r);
}

static void ae_p2_contact_aim_mode_leave(AeRam *r, const AeSoundHw *sound)
{
    AePlayerSpawnHooks spawn_hooks = {0};
    spawn_hooks.sound = sound;
    ae_AimModeLeave(r, &spawn_hooks);
}

static void ae_p2_contact_impact(AeRam *r, const AeWorldHooks *world,
                                 const AeSpawnHooks *spawn,
                                 const AePlayer2Hooks *hooks)
{
    ae_EffectGroundImpactAtPlayer(r, world, spawn, hooks);
}

static void ae_p2_contact_normalize(AeRam *r, uint32_t body, uint32_t out)
{
    uint32_t player = AE_P2_G_PLAYER;
    int32_t dx = ae_p2_sub_wrap(ae_rds32(r, body + AE_P2_BODY_POSITION_X),
                                ae_rds32(r, player + AE_P2_PLAYER_POSITION_X));
    int32_t dz = ae_p2_sub_wrap(ae_rds32(r, body + AE_P2_BODY_POSITION_Z),
                                ae_rds32(r, player + AE_P2_PLAYER_POSITION_Z));
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_DELTA_X, (uint32_t)dx);
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_DELTA_Y, 0);
    ae_wr32(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_DELTA_Z, (uint32_t)dz);
    if (dx != 0 || dz != 0)
        (void)ae_VectorNormalS(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_DELTA_X, out);
}

static void ae_p2_contact_ordinary_knockback(AeRam *r, uint32_t body_flags,
                                             uint32_t normalized,
                                             int negate_facing)
{
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t index = body_flags & 0xFFu;
    uint32_t table_offset = index * 4u;
    int32_t angle;
    int32_t speed;
    int32_t velocity_x;
    int32_t velocity_z;
    int32_t velocity_y;

    angle = ae_rd16(r, AE_P2_G_DAMAGE_ANGLE_TABLE + table_offset + 2u);
    speed = ae_rd16(r, AE_P2_G_DAMAGE_ANGLE_TABLE + table_offset);
    velocity_x = ae_p2_s16(ae_rd16(r, normalized));
    velocity_z = ae_p2_s16(ae_rd16(r, normalized + 4u));
    {
        int32_t product_x = ae_p2_mul_wrap(velocity_x, ae_sdk_rcos(r, angle));
        int32_t product_z = ae_p2_mul_wrap(velocity_z, ae_sdk_rcos(r, angle));
        if (product_x < 0) product_x = ae_p2_add_wrap(product_x, 0xFFF);
        if (product_z < 0) product_z = ae_p2_add_wrap(product_z, 0xFFF);
        velocity_x = ae_p2_mul_wrap(-ae_p2_shift_right(product_x, 12), speed);
        velocity_z = ae_p2_mul_wrap(-ae_p2_shift_right(product_z, 12), speed);
    }
    velocity_y = ae_p2_mul_wrap(-speed, ae_sdk_rsin(r, angle));
    ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X, (uint32_t)velocity_x);
    ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Y, (uint32_t)velocity_y);
    ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z, (uint32_t)velocity_z);
    ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X, (uint32_t)velocity_x);
    ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, (uint32_t)velocity_y);
    ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z, (uint32_t)velocity_z);
    ae_wr16(r, player + AE_P2_PLAYER_FACING,
            (uint32_t)(negate_facing ? -ae_p2_s16(ae_rd16(r, normalized))
                                    : ae_p2_s16(ae_rd16(r, normalized))));
    ae_wr16(r, player + AE_P2_PLAYER_FACING + 4u,
            (uint32_t)(negate_facing ? -ae_p2_s16(ae_rd16(r, normalized + 4u))
                                    : ae_p2_s16(ae_rd16(r, normalized + 4u))));
}

int32_t ae_PlayerOnContact(AeRam *r, const AePlayer2Hooks *hooks,
                           const AePlayerActHooks *act, const AeWorldHooks *world,
                           const AeSpawnHooks *spawn, const AeSoundHw *sound)
{
    uint32_t player = AE_P2_G_PLAYER;
    int32_t invuln = ae_rds16(r, player + AE_P2_PLAYER_INVULN_TIMER) - 1;
    uint32_t state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    uint32_t state_flags = ae_rd32(r, player + AE_P2_PLAYER_STATE_FLAGS) & 0xF0u;
    uint32_t contact = ae_rd32(r, player + AE_P2_PLAYER_CONTACT);
    uint32_t contact_type = contact >> 28;
    int32_t mode = ae_rds16(r, AE_P2_G_PLAYER_MODE);
    uint32_t body;
    uint32_t object_type;
    uint32_t body_flags;
    uint32_t normalized = AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_DIRECTION;

    if (invuln < 0) invuln = 0;
    else if (invuln > 0x96) invuln = 0x96;
    ae_wr16(r, player + AE_P2_PLAYER_INVULN_TIMER, (uint32_t)invuln);

    if (state == 0x4Du) {
        uint32_t cooldown = (ae_rd8(r, player + AE_P2_PLAYER_CONTACT_COOLDOWN) - 1u) & 0xFFu;
        ae_wr8(r, player + AE_P2_PLAYER_CONTACT_COOLDOWN, cooldown);
        if (cooldown != 0) return 0;
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, (uint32_t)-0x80000);
        ae_PlayerSetState(r, act, 0x2Bu);
        if (ae_ram_failed(r) || !ae_p2_contact_voice(r, hooks, 11u)) return 0;
        ae_p2_contact_impact(r, world, spawn, hooks);
        return 0;
    }
    if (state_flags == 0xD0u || state == 0x4Cu || state == 0x22u ||
        ae_rds16(r, AE_P2_G_PLAYER_HURT_BLINK) > 0) {
        ae_wr8(r, player + AE_P2_PLAYER_HURT, 0);
        return 0;
    }

    if (mode == 10 && contact_type > 3u &&
        (contact_type < 6u || contact_type == 0xBu)) {
        ae_PlayerSetMode(r, act, 0);
        if (ae_ram_failed(r)) return 0;
    }
    if (ae_rds16(r, player + AE_P2_PLAYER_HEALTH) < 1) return 0;
    if (contact_type < 4u) {
        ae_wr8(r, player + AE_P2_PLAYER_HURT, 0);
        return 0;
    }

    if (contact_type < 6u) {
        if ((ae_rd16(r, player + AE_P2_PLAYER_GROUND_TYPE) >> 12) == 1u) return 0;
        if (ae_rds32(r, player + AE_P2_PLAYER_GROUND_Y) <
            ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y) + 0x20) return 0;
        body = ae_GetObjectByIndex(r, contact & 0xFFFu);
        ae_p2_contact_normalize(r, body,
                                AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_NORMAL);
        ae_wr32(r, player + AE_P2_PLAYER_VEL_FRACTION_Y, 0);
        ae_wr32(r, player + AE_P2_PLAYER_VEL_FRACTION_Z,
                (uint32_t)-((uint32_t)ae_p2_s16(
                        ae_rd16(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_NORMAL_Z)) << 3));
        ae_wr32(r, player + AE_P2_PLAYER_VEL_FRACTION,
                (uint32_t)-((uint32_t)ae_p2_s16(
                        ae_rd16(r, AE_P2_G_PLAYER_SCRATCH + AE_P2_SCRATCH_CONTACT_NORMAL)) << 3));
        if (state != 0x24u) {
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Y, 0);
            ae_PlayerSetState(r, act, 0x24u);
        }
        return 0;
    }
    if (contact_type != 0xBu) {
        ae_wr8(r, player + AE_P2_PLAYER_HURT, 0);
        return 0;
    }

    body = ae_GetObjectByIndex(r, contact & 0xFFFu);
    body_flags = ae_rd16(r, body + AE_P2_BODY_FLAGS);
    object_type = ae_rd16(r, body + AE_P2_BODY_TYPE);
    if (ae_rd8(r, AE_P2_G_CAMERA_FIXED_MODE) == 1u &&
        !ae_p2_contact_camera_fixed_off(r, hooks)) return 0;
    if (ae_rd8(r, AE_P2_G_AIM_MODE) == 1u) {
        ae_p2_contact_aim_mode_leave(r, sound);
        if (ae_ram_failed(r)) return 0;
    }
    if (ae_rd8(r, AE_P2_G_AIM_REQUEST) == 1u && !ae_p2_contact_aim_clear(r, act)) return 0;

    if ((object_type & 0xFF00u) == 0x0A00u) {
        if (state_flags == 0xD0u) return 1;
        if (state != 0x4Cu) {
            ae_wr16(r, player + AE_P2_PLAYER_SQUASH_TIMER, 0x3Cu);
            ae_p2_contact_damage(r, act, 1);
            if (ae_ram_failed(r)) return 0;
            ae_p2_contact_rumble(r, act, 1, 0xF0, 5);
            if (ae_ram_failed(r)) return 0;
            if (mode == 5) {
                if (ae_rds16(r, player + AE_P2_PLAYER_HEALTH) < 1)
                    ae_PlayerAct_BounceBack(r, act);
            } else if (mode != 6) {
                ae_p2_contact_impact(r, world, spawn, hooks);
                if (ae_ram_failed(r)) return 0;
                ae_PlayerSetState(r, act, 0x4Bu);
                if (ae_ram_failed(r) || !ae_p2_contact_sound(r, sound, 0x42) ||
                    !ae_p2_contact_voice(r, hooks, 9u)) return 0;
                return 1;
            } else if (ae_rds16(r, player + AE_P2_PLAYER_HEALTH) < 1) {
                ae_PlayerAct_ExitToMode0Forward(r, act);
            }
            ae_PlayerStartHurtBlink(r);
            (void)ae_p2_contact_voice(r, hooks, 8u);
            return 1;
        }
        return 1;
    }
    if ((object_type & 0xFF00u) == 0x0600u) {
        ae_PlayerSetState(r, act, 0x2Fu);
        if (ae_ram_failed(r) || !ae_p2_contact_sound(r, sound, 0x4A)) return 0;
        ae_p2_contact_rumble(r, act, 0, 1, 5);
        return ae_ram_failed(r) ? 0 : 1;
    }
    if ((object_type & 0xFF00u) == 0x0B00u) {
        ae_PlayerSetMode(r, act, 7);
        return ae_ram_failed(r) ? 0 : 1;
    }

    if ((body_flags & 0x3FFFu) != 0x100u) {
        ae_PlayerFxSpawn10C(r, spawn);
        if (ae_ram_failed(r)) return 0;
    }
    if (ae_rd8(r, player + AE_P2_PLAYER_HURT) == 0) {
        ae_p2_contact_damage(r, act, object_type & 0xFu);
        if (ae_ram_failed(r)) return 0;
    }

    state = ae_rd16(r, player + AE_P2_PLAYER_STATE);
    if (state > 0x1Au && state < 0x1Du) {
        uint32_t frames = ae_rd8(r, player + AE_P2_PLAYER_CONTACT_FRAMES);
        ae_wr8(r, player + AE_P2_PLAYER_CONTACT_FRAMES, frames + 1u);
        if (frames > 5u) body_flags = 0x202u;
    } else {
        ae_wr8(r, player + AE_P2_PLAYER_CONTACT_FRAMES, 0);
    }
    if ((object_type & 0xFF00u) == 0x0300u) {
        ae_PlayerSetTintEffect(r, 1);
        if (!ae_p2_contact_voice(r, hooks, 11u) || !ae_p2_contact_sound(r, sound, 0x34)) return 0;
    } else if ((object_type & 0xFF00u) == 0x0200u) {
        ae_PlayerSetTintEffect(r, 2);
        if (!ae_p2_contact_voice(r, hooks, 11u) || !ae_p2_contact_sound(r, sound, 0x33)) return 0;
    } else if ((object_type & 0xFF00u) == 0x0400u) {
        ae_PlayerSetTintEffect(r, 3);
        if (!ae_p2_contact_sound(r, sound, 0x35)) return 0;
    }

    ae_p2_contact_normalize(r, body, normalized);
    if (mode == 5) {
        if (ae_rds16(r, player + AE_P2_PLAYER_HEALTH) < 1) ae_PlayerAct_BounceBack(r, act);
        ae_PlayerStartHurtBlink(r);
        if (!ae_p2_contact_voice(r, hooks, 8u)) return 0;
    } else if (mode == 6) {
        if (ae_rds16(r, player + AE_P2_PLAYER_HEALTH) < 1)
            ae_PlayerAct_ExitToMode0Forward(r, act);
        ae_PlayerStartHurtBlink(r);
        if (!ae_p2_contact_voice(r, hooks, 8u)) return 0;
    } else if ((body_flags & 0x3F00u) == 0x0100u) {
        uint32_t index = body_flags & 0xFFu;
        int32_t knockback = ae_rds32(r, AE_P2_G_DAMAGE_KNOCKBACK_TABLE + index * 4u);
        if (state_flags == 0xC0u) {
            ae_PlayerSetState(r, act, 0x4Cu);
            if (ae_ram_failed(r)) return 0;
            ae_PlayerSparkleBurst(r, hooks);
            ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X,
                    0u - ((uint32_t)ae_p2_s16(ae_rd16(r, normalized)) << 4));
            ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z,
                    0u - ((uint32_t)ae_p2_s16(ae_rd16(r, normalized + 4u)) << 4));
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X,
                    ae_rd32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X));
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z,
                    ae_rd32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z));
            ae_PlayerStartHurtBlink(r);
            if ((object_type & 0xFu) != 0 && !ae_p2_contact_voice(r, hooks, 8u)) return 0;
        } else {
            if ((body_flags & 0xFFu) < 2u) ae_PlayerSetState(r, act, 0x1Cu);
            else {
                ae_PlayerSetState(r, act, 0x1Bu);
                if (!ae_p2_contact_voice(r, hooks, 8u)) return 0;
            }
            int32_t nx = ae_p2_s16(ae_rd16(r, normalized));
            int32_t nz = ae_p2_s16(ae_rd16(r, normalized + 4u));
            ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X,
                    (uint32_t)ae_p2_mul_wrap(-nx, knockback));
            ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z,
                    (uint32_t)ae_p2_mul_wrap(-nz, knockback));
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X,
                    ae_rd32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X));
            ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z,
                    ae_rd32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z));
        }
        ae_wr16(r, player + AE_P2_PLAYER_FACING,
                ae_rd16(r, normalized));
        ae_wr16(r, player + AE_P2_PLAYER_FACING + 4u,
                ae_rd16(r, normalized + 4u));
        if (knockback == 0 && !ae_p2_contact_sound(r, sound, 0x5B)) return 0;
    } else if ((body_flags & 0x3F00u) == 0x0400u) {
        ae_p2_contact_ordinary_knockback(r, body_flags, normalized, 1);
        ae_PlayerSetState(r, act, 0x2Bu);
        if (!ae_p2_contact_sound(r, sound, 0x5F)) return 0;
    } else if (state_flags == 0xC0u) {
        ae_PlayerSetState(r, act, 0x4Cu);
        if (ae_ram_failed(r)) return 0;
        ae_PlayerSparkleBurst(r, hooks);
        ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X,
                0u - ((uint32_t)ae_p2_s16(ae_rd16(r, normalized)) << 4));
        ae_wr32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z,
                0u - ((uint32_t)ae_p2_s16(ae_rd16(r, normalized + 4u)) << 4));
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_X,
                ae_rd32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_X));
        ae_wr32(r, player + AE_P2_PLAYER_MOVE_VEL_Z,
                ae_rd32(r, player + AE_P2_PLAYER_TARGET_MOVE_VEL_Z));
        ae_PlayerStartHurtBlink(r);
        if (!ae_p2_contact_voice(r, hooks, 9u)) return 0;
    } else {
        ae_p2_contact_ordinary_knockback(r, body_flags, normalized, 0);
        ae_PlayerSetState(r, act, 0x1Du);
        if (!ae_p2_contact_sound(r, sound, 0x5F)) return 0;
        if (!ae_p2_contact_voice(r, hooks, 9u)) return 0;
    }
    if (ae_ram_failed(r)) return 0;
    ae_p2_contact_rumble(r, act, 1, 0xF0, 5);
    if (ae_ram_failed(r)) return 0;
    ae_CameraShake(r, 0);
    return 1;
}

static int ae_p2_monkey_net_rumble(AeRam *r, const AePlayer2Hooks *hooks,
                                   int32_t motor, int32_t strength, int32_t frames)
{
    if (hooks == NULL || hooks->PadRumble == NULL) {
        ae_ram_fail(r, "MonkeyNetCatchUpdate PadRumble hook not provided");
        return 0;
    }
    hooks->PadRumble(hooks->user, r, motor, strength, frames);
    return !ae_ram_failed(r);
}

static int ae_p2_monkey_net_set_matrix(AeRam *r, const AePlayer2Hooks *hooks,
                                       uint32_t matrix)
{
    if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->SetRotMatrix : NULL,
                                 "SetRotMatrix", matrix, 0, 0, 0)) return 0;
    return ae_p2_call_address_hook(r, hooks, hooks ? hooks->SetTransMatrix : NULL,
                                    "SetTransMatrix", matrix, 0, 0, 0);
}

static void ae_p2_monkey_net_catch_draw(AeRam *r, const AePlayer2Hooks *hooks,
                                         uint32_t local_matrix)
{
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t static_model;
    uint32_t net_model;
    uint32_t vertices;
    uint32_t draw_index;
    uint32_t draw_context;
    uint32_t point_table;
    uint32_t cloth_matrix;
    uint32_t endpoint_a;
    uint32_t endpoint_b;
    uint32_t endpoint_c;
    uint32_t transformed_a = local_matrix + AE_P2_NET_LOCAL_TRANSFORM_A_OFFSET;
    uint32_t transformed_b = local_matrix + AE_P2_NET_LOCAL_TRANSFORM_B_OFFSET;
    uint32_t transformed_c = local_matrix + AE_P2_NET_LOCAL_TRANSFORM_C_OFFSET;
    uint32_t edge_a = local_matrix + AE_P2_NET_LOCAL_EDGE_A_OFFSET;
    uint32_t edge_b = local_matrix + AE_P2_NET_LOCAL_EDGE_B_OFFSET;
    uint32_t local_scale = local_matrix + AE_P2_NET_LOCAL_SCALE_OFFSET;
    uint32_t gte_scratch = local_matrix + AE_P2_NET_LOCAL_GTE_SCRATCH_OFFSET;
    uint32_t swimming = (uint32_t)ae_player_is_swimming(
            ae_rd16(r, player + AE_P2_PLAYER_STATE));

    if (swimming == 0) {
        for (uint32_t word = 0; word < 8u; ++word)
            ae_wr32(r, local_matrix + word * AE_P2_NET_MATRIX_WORD_SIZE,
                    ae_rd32(r, AE_P2_G_NET_MATRIX_SOURCE +
                               word * AE_P2_NET_MATRIX_WORD_SIZE));
        if (!ae_p2_monkey_net_set_matrix(r, hooks, AE_P2_G_STATIC_MATRIX)) return;
        vertices = ae_rd32(r, AE_P2_G_NET_NORMAL_VERTICES);
        net_model = AE_P2_G_NET_MODEL_NORMAL;
        point_table = AE_P2_G_NET_POINT_TABLE_NORMAL;
        endpoint_a = AE_P2_NET_NORMAL_ENDPOINT_A;
        endpoint_b = AE_P2_NET_NORMAL_ENDPOINT_B;
        endpoint_c = AE_P2_NET_NORMAL_ENDPOINT_C;
    } else {
        if (!ae_p2_call_address_hook(r, hooks,
                                     hooks ? hooks->ModelTransformRigid : NULL,
                                     "ModelTransformRigid", AE_P2_G_NET_MODEL_RIGID,
                                     0x800E9E40u, AE_P2_SCREEN_VERTICES, 0)) return;
        if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->ModelDraw : NULL,
                                     "ModelDraw", ae_rd32(r, AE_P2_G_ORDERING_TABLE),
                                     AE_P2_SCREEN_VERTICES, AE_P2_G_NET_MODEL_RIGID, 0)) return;
        ae_sdk_MatrixLookDir(r, AE_P2_G_NET_LOOK_DIRECTION, local_matrix);
        ae_wr32(r, local_matrix + AE_P2_NET_MATRIX_TRANSLATION_X,
                (uint32_t)ae_rds16(r, AE_P2_G_NET_ORIGIN_X));
        ae_wr32(r, local_matrix + AE_P2_NET_MATRIX_TRANSLATION_Y,
                (uint32_t)ae_rds16(r, AE_P2_G_NET_ORIGIN_Y));
        ae_wr32(r, local_matrix + AE_P2_NET_MATRIX_TRANSLATION_Z,
                (uint32_t)ae_rds16(r, AE_P2_G_NET_ORIGIN_Z));
        if (ae_ram_failed(r) || !ae_p2_monkey_net_set_matrix(r, hooks, local_matrix)) return;
        (void)ae_sdk_CompMatrix(r, AE_P2_G_VIEW_MATRIX, local_matrix, local_matrix);
        if (ae_ram_failed(r)) return;
        vertices = ae_rd32(r, AE_P2_G_NET_SWIM_VERTICES);
        net_model = AE_P2_G_NET_MODEL_SWIMMING;
        point_table = AE_P2_G_NET_POINT_TABLE_SWIMMING;
        endpoint_a = AE_P2_NET_SWIM_ENDPOINT_A;
        endpoint_b = AE_P2_NET_SWIM_ENDPOINT_B;
        endpoint_c = AE_P2_NET_SWIM_ENDPOINT_C;
    }

    if (ae_rd16(r, AE_P2_G_CURRENT_LEVEL) == 0x20u) return;

    draw_index = ae_rd32(r, AE_P2_G_DRAW_BUFFER_INDEX);
    draw_context = AE_P2_G_CAPTURE_DRAW_BASE + draw_index * AE_P2_G_NET_DRAW_CONTEXT_STRIDE;

    if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->RotTransSV : NULL,
                                 "RotTransSV", vertices + endpoint_a * AE_P2_NET_SVECTOR_SIZE,
                                 transformed_a, gte_scratch, 0) ||
        !ae_p2_call_address_hook(r, hooks, hooks ? hooks->RotTransSV : NULL,
                                 "RotTransSV", vertices + endpoint_b * AE_P2_NET_SVECTOR_SIZE,
                                 transformed_b, gte_scratch, 0) ||
        !ae_p2_call_address_hook(r, hooks, hooks ? hooks->RotTransSV : NULL,
                                 "RotTransSV", vertices + endpoint_c * AE_P2_NET_SVECTOR_SIZE,
                                 transformed_c, gte_scratch, 0)) return;

    ae_wr16(r, AE_P2_G_CAPTURE_ANIM_INDEX, 0);
    int32_t midpoint_x = (ae_rds16(r, transformed_a) + ae_rds16(r, transformed_c)) / 2;
    int32_t midpoint_y = (ae_rds16(r, transformed_a + AE_P2_NET_SVECTOR_Y_OFFSET) +
                          ae_rds16(r, transformed_c + AE_P2_NET_SVECTOR_Y_OFFSET)) / 2;
    int32_t midpoint_z = (ae_rds16(r, transformed_a + AE_P2_NET_SVECTOR_Z_OFFSET) +
                          ae_rds16(r, transformed_c + AE_P2_NET_SVECTOR_Z_OFFSET)) / 2;
    ae_wr16(r, edge_a, (uint32_t)(midpoint_x - ae_rds16(r, transformed_b)));
    ae_wr16(r, edge_a + AE_P2_NET_SVECTOR_Y_OFFSET,
            (uint32_t)(midpoint_y - ae_rds16(r, transformed_b + AE_P2_NET_SVECTOR_Y_OFFSET)));
    ae_wr16(r, edge_a + AE_P2_NET_SVECTOR_Z_OFFSET,
            (uint32_t)(midpoint_z - ae_rds16(r, transformed_b + AE_P2_NET_SVECTOR_Z_OFFSET)));
    ae_wr16(r, edge_b, (uint32_t)(ae_rds16(r, transformed_a) - midpoint_x));
    ae_wr16(r, edge_b + AE_P2_NET_SVECTOR_Y_OFFSET,
            (uint32_t)(ae_rds16(r, transformed_a + AE_P2_NET_SVECTOR_Y_OFFSET) - midpoint_y));
    ae_wr16(r, edge_b + AE_P2_NET_SVECTOR_Z_OFFSET,
            (uint32_t)(ae_rds16(r, transformed_a + AE_P2_NET_SVECTOR_Z_OFFSET) - midpoint_z));
    ae_wr32(r, AE_P2_G_CAPTURE_PIVOT, (uint32_t)(int32_t)(int16_t)midpoint_x);
    ae_wr32(r, AE_P2_G_CAPTURE_PIVOT + AE_P2_NET_VECTOR_Y_OFFSET,
            (uint32_t)(int32_t)(int16_t)midpoint_y);
    ae_wr32(r, AE_P2_G_CAPTURE_PIVOT + AE_P2_NET_VECTOR_Z_OFFSET,
            (uint32_t)(int32_t)(int16_t)midpoint_z);
    if (ae_ram_failed(r)) return;

    uint32_t capture_anim_data = ae_rd32(r, AE_P2_G_CAPTURE_ANIM_DATA);
    uint32_t capture_frame = (ae_rd16(r, AE_P2_G_CAPTURE_ANIM_FRAME) + 1u) & 0xFFFFu;
    uint32_t first_frame = ae_rd16(r, capture_anim_data + AE_P2_CAPTURE_ANIM_FIRST);
    uint32_t last_frame = ae_rd16(r, capture_anim_data + AE_P2_CAPTURE_ANIM_LAST);
    if (capture_frame < first_frame || last_frame < capture_frame) capture_frame = first_frame;
    ae_wr16(r, AE_P2_G_CAPTURE_ANIM_FRAME, capture_frame);
    ae_sdk_VectorNormalSS(r, edge_a, AE_P2_G_CAPTURE_NORMAL_B);
    ae_sdk_VectorNormalSS(r, edge_b, AE_P2_G_CAPTURE_NORMAL_A);
    if (ae_ram_failed(r)) return;
    cloth_matrix = ae_rd32(r, AE_P2_G_CAPTURE_CLOTH_MATRIX);
    if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->MatrixFromFrontUp : NULL,
                                 "MatrixFromFrontUp", AE_P2_G_CAPTURE_NORMAL_A,
                                 AE_P2_G_CAPTURE_NORMAL_B, cloth_matrix, 0)) return;

    int32_t flash_frame = ae_rds16(r, AE_P2_G_CAPTURE_FLASH_FRAME);
    if (flash_frame > 0 && flash_frame < (int32_t)AE_P2_NET_FLASH_SCALE_FRAMES) {
        int32_t scale = (flash_frame << 12) / (int32_t)AE_P2_NET_FLASH_SCALE_FRAMES;
        ae_wr32(r, local_scale, (uint32_t)scale);
        ae_wr32(r, local_scale + AE_P2_NET_VECTOR_Y_OFFSET, (uint32_t)(0x2000 - scale));
        ae_wr32(r, local_scale + AE_P2_NET_VECTOR_Z_OFFSET, (uint32_t)scale);
        if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->ScaleMatrixL : NULL,
                                     "ScaleMatrixL", cloth_matrix,
                                     local_scale, 0, 0) ||
            !ae_p2_call_address_hook(r, hooks, hooks ? hooks->RotMatrixY : NULL,
                                     "RotMatrixY", (uint32_t)(scale * 2),
                                     cloth_matrix, 0, 0)) return;
    }

    if (flash_frame > 0) {
        uint32_t animation_entry = capture_anim_data +
                                   ae_rd16(r, AE_P2_G_CAPTURE_ANIM_INDEX) * 8u;
        if (!ae_p2_call_address_hook(r, hooks,
                                     hooks ? hooks->ActorAnimDecodeEntry : NULL,
                                     "ActorAnimDecodeEntry", AE_P2_G_CAPTURE_ACTOR,
                                     animation_entry, 0, 0)) return;
        ae_ModelBuildBoneMatrices(r, AE_P2_G_CAPTURE_ACTOR, 1u);
        if (ae_ram_failed(r)) return;
        if (!ae_p2_call_address_hook(r, hooks,
                                     hooks ? hooks->ModelTransformSkinned : NULL,
                                     "ModelTransformSkinned", AE_P2_G_CAPTURE_ACTOR,
                                     0, 0, 0)) return;

        uint32_t caught_group = ae_MonkeyCaughtGroupValue(r);
        uint32_t caught_anim_group = ae_rd8(r, AE_P2_G_CAUGHT_GROUP_LOOKUP +
                                              (caught_group >> 6));
        static_model = AE_P2_G_PLAYER_STATIC_MODELS +
                       caught_anim_group * AE_P2_NET_STATIC_MODEL_STRIDE;
        uint32_t static_model_header = ae_rd32(r, static_model + AE_P2_NET_STATIC_MODEL_POINTER);
        uint32_t matrix_index = ae_rd16(r, static_model_header + AE_P2_NET_STATIC_MODEL_MESH_INDEX);
        uint32_t model_matrix = AE_P2_G_STATIC_MODEL_MATRICES +
                                matrix_index * AE_P2_NET_MATRIX_STRIDE;
        if (ae_ram_failed(r)) return;
        if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->ModelDrawWithOt : NULL,
                                     "ModelDrawWithOt", draw_context, AE_P2_SCREEN_VERTICES,
                                     AE_P2_G_CAPTURE_ACTOR, caught_group) ||
            !ae_p2_call_address_hook(r, hooks,
                                     hooks ? hooks->ModelTransformRigid : NULL,
                                     "ModelTransformRigid", static_model, model_matrix,
                                     AE_P2_SCREEN_VERTICES, 0) ||
            !ae_p2_call_address_hook(r, hooks, hooks ? hooks->ModelDraw : NULL,
                                     "ModelDraw", draw_context, AE_P2_SCREEN_VERTICES,
                                     static_model, 0)) return;
    }

    point_table = swimming ? AE_P2_G_NET_POINT_TABLE_SWIMMING :
                              AE_P2_G_NET_POINT_TABLE_NORMAL;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ModelTransformRigid : NULL,
                                 "ModelTransformRigid", net_model, local_matrix,
                                 AE_P2_SCREEN_VERTICES, 0)) return;
    if (flash_frame >= (int32_t)AE_P2_NET_FLASH_SCALE_FRAMES) {
        for (uint32_t point = 0; point < 6u; ++point) {
            uint32_t point_record = point_table + point * AE_P2_NET_POINT_RECORD_STRIDE;
            uint32_t bone_matrix = AE_P2_G_STATIC_MODEL_MATRICES +
                ae_rd16(r, point_record + AE_P2_NET_POINT_MATRIX_INDEX) * AE_P2_NET_MATRIX_STRIDE;
            uint32_t vertex_index = ae_rd16(r, point_record + AE_P2_NET_POINT_VERTEX_INDEX);
            if (!ae_p2_monkey_net_set_matrix(r, hooks, bone_matrix) ||
                !ae_p2_call_address_hook(r, hooks,
                                         hooks ? hooks->RotTransPers : NULL,
                                         "RotTransPers",
                                         point_record + AE_P2_NET_POINT_POSITION,
                                         AE_P2_SCREEN_VERTICES +
                                             vertex_index * AE_P2_NET_SCREEN_VERTEX_STRIDE,
                                         gte_scratch, gte_scratch)) return;
        }
    }
    if (ae_ram_failed(r)) return;
    (void)vertices;
    (void)point_table;
    ae_p2_call_address_hook(r, hooks, hooks ? hooks->ModelDraw : NULL,
                            "ModelDraw", draw_context, AE_P2_SCREEN_VERTICES,
                            net_model, 0);
}

void ae_MonkeyNetCatchDraw(AeRam *r, const AePlayer2Hooks *hooks)
{
    ae_p2_monkey_net_catch_draw(r, hooks, AE_P2_G_NET_FRAME_LOCAL_MATRIX);
}

void ae_MonkeyNetCatchUpdate(AeRam *r, const AePlayer2Hooks *hooks,
                             const AePlayerActHooks *act)
{
    uint32_t player = AE_P2_G_PLAYER;
    uint32_t draw_index = ae_rd32(r, AE_P2_G_DRAW_BUFFER_INDEX);
    uint32_t draw_context = AE_P2_G_CAPTURE_DRAW_BASE +
                            draw_index * AE_P2_G_NET_DRAW_CONTEXT_STRIDE;
    int32_t frame = ae_rds16(r, AE_P2_G_CAPTURE_FLASH_FRAME);

    if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->MatrixFromFrontUp : NULL,
                                 "MatrixFromFrontUp", player + AE_P2_PLAYER_FRONT_VECTOR,
                                 player + AE_P2_PLAYER_UP_VECTOR,
                                 ae_rd32(r, player + AE_P2_PLAYER_BONE_MATRICES), 0)) return;
    if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->AnimDecodeFrame : NULL,
                                 "AnimDecodeFrame", player, 0, 0, 0)) return;
    ae_ModelBuildBoneMatrices(r, player, 3u);
    if (ae_ram_failed(r)) return;
    if (!ae_p2_call_address_hook(r, hooks,
                                 hooks ? hooks->ModelTransformSkinned : NULL,
                                 "ModelTransformSkinned", player, 0, 0, 0)) return;
    if (!ae_p2_call_address_hook(r, hooks, hooks ? hooks->ModelDraw : NULL,
                                 "ModelDraw", draw_context, AE_P2_SCREEN_VERTICES,
                                 player, 0)) return;
    ae_p2_monkey_net_catch_draw(r, hooks, AE_P2_G_NET_NESTED_LOCAL_MATRIX);
    if (ae_ram_failed(r)) return;

    if (frame >= 13) {
        if (frame % 5 == 0 && !ae_p2_monkey_net_rumble(r, hooks, 1, 200, 3)) return;
    } else if (frame >= 4) {
        if (!ae_p2_monkey_net_rumble(r, hooks, 0, 1, 1)) return;
    }
    if (frame == 0x25) {
        ae_wr32(r, AE_P2_G_NET_RIBBON_FLAGS,
                ae_rd32(r, AE_P2_G_NET_RIBBON_FLAGS) | 0xEu);
    } else if (frame < 0x26) {
        if (frame == 9) ae_PlayPlayerVoice(r, hooks, 6u, 0);
    } else if (frame == 0x26) {
        ae_PlayPlayerVoice(r, hooks, 5u, 0);
        if (!ae_p2_call_simple_hook(r, hooks,
                                    hooks ? hooks->EventCamStartRandom : NULL,
                                    "EventCamStartRandom")) return;
        ae_GadgetFlashSet(r, AE_P2_G_CAPTURE_PIVOT);
    }
    ae_wr16(r, AE_P2_G_CAPTURE_FLASH_FRAME, (uint32_t)(frame - 1));
    if (frame == 1) ae_PlayerSetMode(r, act, 0);
}
