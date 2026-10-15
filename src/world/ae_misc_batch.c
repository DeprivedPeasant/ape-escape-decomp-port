#include "ae_misc_batch.h"

#include "ae_areaset.h"
#include "ae_gimmick_a.h"
#include "ae_world.h"
#include "ae_zones.h"
#include "../core/ae_sdk.h"
#include "../player/ae_player1.h"
#include "../player/ae_player_act.h"
#include "../player/native_player.h"

#include <stdint.h>

/* Original globals and field offsets used by this batch. */
#define AE_MB_ZONE_BOX_COUNT             0x800B4D30u
#define AE_MB_ZONE_BOX_POINTER           0x800B4D34u
#define AE_MB_ZONE_BOX_STRIDE            0x10u
#define AE_MB_ZONE_CENTER_X              0x00u
#define AE_MB_ZONE_CENTER_Y              0x02u
#define AE_MB_ZONE_CENTER_Z              0x04u
#define AE_MB_ZONE_HALF_X                0x06u
#define AE_MB_ZONE_HALF_Y                0x08u
#define AE_MB_ZONE_HALF_Z                0x0Au
#define AE_MB_ZONE_MASK                  0x0Cu
#define AE_MB_PAD_RUMBLE_PORT0_STATE     0x800E5468u
#define AE_MB_PAD_RUMBLE_PORT1_STATE     0x800E5469u
#define AE_MB_STAGE_EXIT_PENDING         0x800B8CE0u
#define AE_MB_PLAYER                     0x800EC1E0u
#define AE_MB_PLAYER_POSITION_X          0x24u
#define AE_MB_PLAYER_POSITION_Y          0x28u
#define AE_MB_PLAYER_SURFACE_Y           0x13Cu
#define AE_MB_WATER_ACTIVE               0x800BAD7Bu
#define AE_MB_BOSS_UPDATE_DISABLED       0x800AF840u
#define AE_MB_BOSS_KIND_TABLE            0x800A5378u
#define AE_MB_CURRENT_AREA_TAG           0x800EC2D2u
#define AE_MB_CURRENT_AREA_ID            0x800F4476u
#define AE_MB_RC_CAR                     0x800F5A90u
#define AE_MB_RC_CAR_STATE               0x800F4350u
#define AE_MB_STAGE_DRAW_STATE           0x800EC350u
#define AE_MB_STAGE_DRAW_CURRENT         0x150u
#define AE_MB_STAGE_DRAW_MODE            0x126u
#define AE_MB_STAGE_DRAW_DESTINATION     0x04u
#define AE_MB_MENU_ITEM_MINIMUM          0x10u
#define AE_MB_MENU_ITEM_MAXIMUM          0x14u
#define AE_MB_MENU_ITEM_STEP             0x1Cu
#define AE_MB_FADE_PROGRESS              0x00u
#define AE_MB_FADE_MAXIMUM               0x04u
#define AE_MB_FADE_DIRECTION             0x08u
#define AE_MB_FADE_TIMER                 0x0Cu
#define AE_MB_VRAM_SCROLL_STATE          0x800B98F0u
#define AE_MB_VRAM_SCROLL_LIST           0x00u
#define AE_MB_VRAM_SCROLL_COUNT          0x04u
#define AE_MB_VRAM_SCROLL_ENTRY_SIZE     0x10u
#define AE_MB_VRAM_SCROLL_TYPE           0x00u
#define AE_MB_VRAM_SCROLL_ACTIVE         0x01u
#define AE_MB_VRAM_SCROLL_VALUE_X        0x02u
#define AE_MB_VRAM_SCROLL_VALUE_Y        0x03u
#define AE_MB_VRAM_SCROLL_SOURCE_X       0x04u
#define AE_MB_VRAM_SCROLL_SOURCE_Y       0x06u
#define AE_MB_VRAM_SCROLL_RECT_X_OFFSET  0x08u
#define AE_MB_VRAM_SCROLL_RECT_Y_OFFSET  0x0Au
#define AE_MB_VRAM_SCROLL_RECT_WIDTH     0x0Cu
#define AE_MB_VRAM_SCROLL_RECT_HEIGHT    0x0Du
#define AE_MB_VRAM_SCROLL_PHASE_OFFSET   0x0Eu
#define AE_MB_VRAM_SCROLL_SPEED          0x0Fu
#define AE_MB_VRAM_SCROLL_RECT_SIZE      0x08u
#define AE_MB_VRAM_SCROLL_RECT_X         0x00u
#define AE_MB_VRAM_SCROLL_RECT_Y         0x02u
#define AE_MB_VRAM_SCROLL_RECT_W         0x04u
#define AE_MB_VRAM_SCROLL_RECT_H         0x06u
#define AE_MB_VRAM_SCROLL_DEST_STRIDE    0x18u
#define AE_MB_VRAM_SCROLL_AREA_INDEX     0x0Cu
#define AE_MB_VRAM_SCROLL_AREA_FLAGS     0x407Cu
#define AE_MB_VRAM_SCROLL_AREA_INDEX_STRIDE 0x4078u
#define AE_MB_FRAME_COUNTER              0x800EC350u
#define AE_MB_FADE_STATE                 0x800DF7D8u
#define AE_MB_FADE_RED                   0x20u
#define AE_MB_FADE_GREEN                 0x21u
#define AE_MB_FADE_BLUE                  0x22u
#define AE_MB_PROGRESS_FLAGS             0x800DFCC0u
#define AE_MB_INDICATOR_COUNT            0x800BBC50u
#define AE_MB_INDICATOR_RECORDS          0x800BBD34u
#define AE_MB_INDICATOR_STRIDE           0x18u
#define AE_MB_INDICATOR_KIND             0x00u
#define AE_MB_INDICATOR_ANGLE            0x02u
#define AE_MB_INDICATOR_DATA             0x04u
#define AE_MB_INDICATOR_X_VELOCITY       0x0Cu
#define AE_MB_INDICATOR_Y_VELOCITY       0x0Eu
#define AE_MB_INDICATOR_X_DIRECTION      0x10u
#define AE_MB_INDICATOR_ANGLE_DELTA      0x0Au
#define AE_MB_INDICATOR_AGE              0x14u
#define AE_MB_INDICATOR_TIMER            0x16u
#define AE_MB_HUD_GADGET_STATE           0x800F5120u
#define AE_MB_HUD_AREA_KIND              0x800EC2D2u
#define AE_MB_HUD_AREA_RECORDS           0x800B2234u
#define AE_MB_HUD_AREA_RECORD_STRIDE     0x04u
#define AE_MB_HUD_AREA_POSITION_X        0x00u
#define AE_MB_HUD_AREA_POSITION_Y        0x02u
#define AE_MB_HUD_AREA_ICON_KIND         0x88u
#define AE_MB_HUD_ICON_X                 0x94u
#define AE_MB_HUD_ICON_Y                 0x96u
#define AE_MB_AREA_FEATURE_STATE         0x800F5AA0u
#define AE_MB_AREA_FEATURE_ARGUMENT      0x03u
#define AE_MB_AREA_FEATURE_PROGRESS      0x04u
#define AE_MB_AREA_FEATURE_STARTED       0x01u
#define AE_MB_AREA_FEATURE_FINISHED      0x02u
#define AE_MB_HUD_FLAGS                  0x800F4DD0u
#define AE_MB_ARCHIVE_MEMBER_TABLE       0x800B7E50u
#define AE_MB_CD_MANAGER_QUEUE_COUNT     0x70u
#define AE_MB_CD_MANAGER_QUEUE_HEAD      0x44u
#define AE_MB_ARCHIVE_MEMBER_POSITION    0x00u
#define AE_MB_ARCHIVE_MEMBER_LENGTH      0x04u
#define AE_MB_CD_COMMAND_SOURCE          0x00u
#define AE_MB_CD_COMMAND_DESTINATION     0x04u
#define AE_MB_CD_COMMAND_LENGTH          0x08u
#define AE_MB_AREA_PROPS_POINTER         0x800B4D64u
#define AE_MB_AREA_PROP_STRIDE           0x20u
#define AE_MB_AREA_PROP_POSITION_X       0x10u
#define AE_MB_AREA_PROP_LAST_POSITION_Z  0x16u
#define AE_MB_COLLIDER_ROUTE_TABLE_PTR   0x800F4564u
#define AE_MB_COLLIDER_SCRIPT_DATA_PTR   0x800F455Cu
#define AE_MB_COLLIDER_RECORD_STRIDE     0x38u
#define AE_MB_COLLIDER_ROUTE_STRIDE      0x10u
#define AE_MB_COLLIDER_ROUTE_DURATION    0x0Cu
#define AE_MB_COLLIDER_ROUTE_NEXT_INDEX  0x0Eu
#define AE_MB_COLLIDER_POSITION_X        0x00u
#define AE_MB_COLLIDER_POSITION_Y        0x02u
#define AE_MB_COLLIDER_POSITION_Z        0x04u
#define AE_MB_COLLIDER_HALF_X            0x06u
#define AE_MB_COLLIDER_HALF_Y            0x08u
#define AE_MB_COLLIDER_HALF_Z            0x0Au
#define AE_MB_COLLIDER_SPEED              0x0Cu
#define AE_MB_COLLIDER_SCRIPT_SPEED       0x1Cu
#define AE_MB_COLLIDER_PROP_INDEX         0x0Eu
#define AE_MB_COLLIDER_GEOMETRY           0x12u
#define AE_MB_COLLIDER_FLAGS              0x18u
#define AE_MB_COLLIDER_ZONE_MASK          0x20u
#define AE_MB_COLLIDER_ROUTE_INDEX        0x22u
#define AE_MB_COLLIDER_ROUTE_X            0x24u
#define AE_MB_COLLIDER_ROUTE_Y            0x26u
#define AE_MB_COLLIDER_ROUTE_Z            0x28u
#define AE_MB_COLLIDER_FLOW_X             0x2Au
#define AE_MB_COLLIDER_FLOW_Y             0x2Cu
#define AE_MB_COLLIDER_FLOW_Z             0x2Eu
#define AE_MB_COLLIDER_SCRIPT_TIMER        0x32u
#define AE_MB_COLLIDER_SCRIPT_STATE_BYTE   0x34u
#define AE_MB_COLLIDER_MOTION_SCALE        0x1Cu
#define AE_MB_COLLIDER_SCRIPT_START_X      0x24u
#define AE_MB_COLLIDER_SCRIPT_START_Y      0x26u
#define AE_MB_COLLIDER_SCRIPT_START_Z      0x28u
#define AE_MB_COLLIDER_SCRIPT_FLAGS        0x18u
#define AE_MB_PLAYER_ACTOR_POSITION        0x24u
#define AE_MB_PLAYER_ACTOR_VELOCITY        0x30u
#define AE_MB_PLAYER_ACTOR_STATE           0x70u
#define AE_MB_PLAYER_ACTOR_FACING           0x60u
#define AE_MB_PLAYER_ACTOR_BODY_COLLIDER    0x58u
#define AE_MB_PLAYER_ACTOR_HEIGHT_ADJUST    0x46u
#define AE_MB_PLAYER_ACTOR_POSITION_Y       0x28u
#define AE_MB_PLAYER_ACTOR_POSITION_Z       0x2Cu
#define AE_MB_PLAYER_CURRENT_AREA           0x800EC238u
#define AE_MB_RC_CAR_OBJECT                0x800F5A20u
#define AE_MB_RC_CAR_POSITION               0x24u
#define AE_MB_RC_CAR_POSITION_Y             0x28u
#define AE_MB_RC_CAR_POSITION_Z             0x2Cu
#define AE_MB_RC_CAR_STATE_FLAGS            0x70u
#define AE_MB_RC_CAR_BODY_COLLIDER          0x54u
#define AE_MB_RC_CAR_WATER_HEIGHT           0x2Cu
#define AE_MB_AREA_GROUND_STALE_X           0u
#define AE_MB_AREA_GROUND_STALE_Z           0u
#define AE_MB_FALLBACK_STACK               0x801FFF00u
#define AE_MB_COLLIDER_COMMAND_LETTER       0x02u
#define AE_MB_COLLIDER_COMMAND_VALUE        0x04u
#define AE_MB_COLLIDER_COMMAND_RADIUS       0x06u
#define AE_MB_COLLIDER_COMMAND_DURATION     0x08u
#define AE_MB_COLLIDER_COMMAND_DELAY        0x0Au
#define AE_MB_SCROLL_RECT_STACK_SIZE     48u
#define AE_MB_AREA_FEATURE_MEMBER_STACK_SIZE 48u
#define AE_MB_AREA_FEATURE_COMMAND_STACK_SIZE 64u
#define AE_MB_COLLIDER_ROUTE_STACK_SIZE  40u
#define AE_MB_COLLIDER_CARRY_STACK_SIZE  24u

static int16_t misc_batch_s16(uint32_t value)
{
    return (int16_t)(uint16_t)value;
}

static int32_t misc_batch_subu_s32(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

static int32_t misc_batch_sra(int32_t value, unsigned shift)
{
    if (shift == 0u) return value;
    if (value >= 0) return value >> shift;
    return (int32_t)(-((-(int64_t)value + ((1u << shift) - 1u)) >> shift));
}

static int32_t misc_batch_div(int32_t numerator, int32_t denominator)
{
    if (denominator == 0) return numerator < 0 ? 1 : -1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

static int32_t misc_batch_call(AeRam *r, const AeMiscHooks *h, AeMiscCall callback,
                               const char *name, uint32_t a0, uint32_t a1,
                               uint32_t a2, uint32_t a3)
{
    if (callback == NULL) {
        ae_ram_fail(r, name);
        return 0;
    }
    return callback(h->user, r, a0, a1, a2, a3);
}

static uint32_t misc_batch_cd_int_to_pos(void *user, AeRam *r,
                                         uint32_t a0, uint32_t a1,
                                         uint32_t a2, uint32_t a3)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return (uint32_t)misc_batch_call(r, h, h != NULL ? h->CdIntToPos : NULL,
                                     "CdIntToPos", a0, a1, a2, a3);
}

static uint32_t misc_batch_printf(void *user, AeRam *r,
                                  uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return (uint32_t)misc_batch_call(r, h, h != NULL ? h->Printf : NULL,
                                     "Printf", a0, a1, a2, a3);
}

static uint32_t misc_batch_cd_control(void *user, AeRam *r,
                                      uint32_t a0, uint32_t a1,
                                      uint32_t a2, uint32_t a3)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return (uint32_t)misc_batch_call(r, h, h != NULL ? h->CdControl : NULL,
                                     "CdControl", a0, a1, a2, a3);
}

static uint32_t misc_batch_cd_ready(void *user, AeRam *r,
                                    uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return (uint32_t)misc_batch_call(r, h, h != NULL ? h->CdReadyCallback : NULL,
                                     "CdReadyCallback", a0, a1, a2, a3);
}

static uint32_t misc_batch_vsync(void *user, AeRam *r,
                                uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return (uint32_t)misc_batch_call(r, h, h != NULL ? h->VSync : NULL,
                                     "VSync", a0, a1, a2, a3);
}

static uint32_t misc_batch_anim_frame_ptr(void *user, AeRam *r,
                                          uint32_t instance, int32_t frame)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return (uint32_t)misc_batch_call(r, h, h != NULL ? h->AnimFramePtr : NULL,
                                     "AnimFramePtr", instance, (uint32_t)frame, 0, 0);
}

static int32_t misc_batch_get_solid_height(void *user, AeRam *r,
                                           uint32_t x, uint32_t y,
                                           uint32_t z, uint32_t a3)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return misc_batch_call(r, h, h != NULL ? h->GetSolidHeight : NULL,
                           "GetSolidHeight", x, y, z, a3);
}

static int32_t misc_batch_quadrant_mask(void *user, AeRam *r, int32_t x, int32_t z)
{
    const AeMiscHooks *h = (const AeMiscHooks *)user;
    return misc_batch_call(r, h,
                           h != NULL ? h->GetColliderQuadrantMaskXZ : NULL,
                           "GetColliderQuadrantMaskXZ", (uint32_t)x, (uint32_t)z, 0, 0);
}

static int32_t misc_batch_camera_nudge(void *user, AeRam *r,
                                       uint32_t dx, uint32_t dy,
                                       uint32_t dz, uint32_t unused)
{
    (void)user;
    (void)unused;
    ae_CameraNudge(r, (int32_t)dx, (int32_t)dy, (int32_t)dz);
    return 0;
}

static void misc_batch_player1_hooks(AePlayer1Hooks *out, const AeMiscHooks *h)
{
    *out = (AePlayer1Hooks){0};
    out->user = (void *)h;
    out->CdIntToPos = misc_batch_cd_int_to_pos;
    out->printf = misc_batch_printf;
    out->CdControl = misc_batch_cd_control;
    out->CdReadyCallback = misc_batch_cd_ready;
    out->VSync = misc_batch_vsync;
}

static void misc_batch_player_act_hooks(AePlayerActHooks *out, const AeMiscHooks *h)
{
    *out = (AePlayerActHooks){0};
    out->user = (void *)h;
    out->AnimFramePtr = misc_batch_anim_frame_ptr;
}

static void misc_batch_world_hooks(AeWorldHooks *out, const AeMiscHooks *h)
{
    *out = (AeWorldHooks){0};
    out->user = (void *)h;
    out->GetSolidHeight = misc_batch_get_solid_height;
}

static void misc_batch_gimmick_hooks(AeGimmickAHooks *out)
{
    *out = (AeGimmickAHooks){0};
    out->CameraNudge = misc_batch_camera_nudge;
}

static void misc_batch_zone_hooks(AeZonesHooks *out, const AeMiscHooks *h)
{
    *out = (AeZonesHooks){0};
    out->user = (void *)h;
    out->get_collider_quadrant_mask_xz = misc_batch_quadrant_mask;
}

/* PadRumbleReset @80014208. */
void ae_PadRumbleReset(AeRam *r, int32_t port)
{
    if (port == 0) {
        ae_wr8(r, AE_MB_PAD_RUMBLE_PORT0_STATE, 0u);
    } else if (port == 1) {
        ae_wr8(r, AE_MB_PAD_RUMBLE_PORT1_STATE, 0u);
    }
}

/* GetZoneMask @8004626C. The original negative-Y path accidentally takes
 * the absolute value of delta-X; keep that observable instruction behavior. */
int32_t ae_GetZoneMask(AeRam *r, int32_t x, int32_t y, int32_t z, int32_t fallback)
{
    uint32_t count = ae_rd32(r, AE_MB_ZONE_BOX_COUNT);
    uint32_t box = ae_rd32(r, AE_MB_ZONE_BOX_POINTER);
    uint16_t mask = 0;

    for (uint32_t index = 0; index < count; ++index, box += AE_MB_ZONE_BOX_STRIDE) {
        int32_t delta_x = misc_batch_subu_s32(ae_rds16(r, box + AE_MB_ZONE_CENTER_X), x);
        int32_t delta_y = misc_batch_subu_s32(ae_rds16(r, box + AE_MB_ZONE_CENTER_Y), y);
        int32_t delta_z = misc_batch_subu_s32(ae_rds16(r, box + AE_MB_ZONE_CENTER_Z), z);
        if (delta_x < 0) delta_x = (int32_t)(0u - (uint32_t)delta_x);
        /* Original quirk: a negative Y distance is replaced by -delta_x, not -delta_y. */
        if (delta_y < 0) delta_y = (int32_t)(0u - (uint32_t)delta_x);
        if (delta_z < 0) delta_z = (int32_t)(0u - (uint32_t)delta_z);

        if (delta_x < ae_rds16(r, box + AE_MB_ZONE_HALF_X) &&
            delta_y < ae_rds16(r, box + AE_MB_ZONE_HALF_Y) &&
            delta_z < ae_rds16(r, box + AE_MB_ZONE_HALF_Z)) {
            mask |= ae_rd16(r, box + AE_MB_ZONE_MASK);
        }
    }

    if (mask == 0u) mask = fallback != 0 ? 0xFFFFu : 0x8000u;
    return (int32_t)(uint32_t)mask;
}

/* StageExitPending @8004D104. */
int32_t ae_StageExitPending(AeRam *r)
{
    return ae_rds16(r, AE_MB_STAGE_EXIT_PENDING) != 0;
}

/* PlayerIsSubmerged @8006B9F8. */
int32_t ae_PlayerIsSubmerged(AeRam *r)
{
    if (ae_rd8(r, AE_MB_WATER_ACTIVE) == 0u) return 0;
    int32_t below_water = (int32_t)(ae_rd32(r, AE_MB_PLAYER + AE_MB_PLAYER_POSITION_Y) - 256u);
    return ae_rds16(r, AE_MB_PLAYER + AE_MB_PLAYER_SURFACE_Y) < below_water;
}

/* MenuItemSetRange @8007D290. */
void ae_MenuItemSetRange(AeRam *r, uint32_t item, uint32_t minimum, uint32_t maximum)
{
    ae_wr32(r, item + AE_MB_MENU_ITEM_MINIMUM, minimum);
    ae_wr32(r, item + AE_MB_MENU_ITEM_MAXIMUM, maximum);
}

/* MenuItemSetStep @8007D2A4. */
void ae_MenuItemSetStep(AeRam *r, uint32_t item, uint32_t step)
{
    ae_wr32(r, item + AE_MB_MENU_ITEM_STEP, step);
}

/* ColorFadeReset @8008C390. */
void ae_ColorFadeReset(AeRam *r)
{
    ae_wr32(r, AE_MB_FADE_STATE + AE_MB_FADE_PROGRESS, 0u);
    ae_wr32(r, AE_MB_FADE_STATE + AE_MB_FADE_MAXIMUM, 16u);
    ae_wr32(r, AE_MB_FADE_STATE + AE_MB_FADE_DIRECTION, 0u);
    ae_wr32(r, AE_MB_FADE_STATE + AE_MB_FADE_TIMER, 0u);
    ae_wr8(r, AE_MB_FADE_STATE + AE_MB_FADE_RED, 0x80u);
    ae_wr8(r, AE_MB_FADE_STATE + AE_MB_FADE_GREEN, 0x80u);
    ae_wr8(r, AE_MB_FADE_STATE + AE_MB_FADE_BLUE, 0x80u);
}

/* ProgressFlagGet @8009632C. */
int32_t ae_ProgressFlagGet(AeRam *r, uint32_t flag)
{
    int32_t word_index = (int16_t)((flag >> 5) & 0xFFFFu);
    uint32_t mask = 1u << (flag & 31u);
    return (ae_rd32(r, AE_MB_PROGRESS_FLAGS + (uint32_t)word_index * 4u) & mask) != 0u;
}

/* HudFindGadgetIconPos @8008197C. */
void ae_HudFindGadgetIconPos(AeRam *r)
{
    int32_t area_kind = ae_rds8(r, AE_MB_HUD_AREA_KIND);
    int32_t x = 328;
    int32_t y = 72;
    uint32_t area_record = AE_MB_HUD_AREA_RECORDS;

    for (int32_t index = 0; index < 4; ++index,
         area_record += AE_MB_HUD_AREA_RECORD_STRIDE) {
        if (ae_rds8(r, AE_MB_HUD_GADGET_STATE + AE_MB_HUD_AREA_ICON_KIND +
                       (uint32_t)index) == area_kind) {
            x = ae_rds16(r, area_record + AE_MB_HUD_AREA_POSITION_X) - 24;
            y = ae_rds16(r, area_record + AE_MB_HUD_AREA_POSITION_Y) - 23;
            break;
        }
    }
    ae_wr16(r, AE_MB_HUD_GADGET_STATE + AE_MB_HUD_ICON_X, (uint32_t)x);
    ae_wr16(r, AE_MB_HUD_GADGET_STATE + AE_MB_HUD_ICON_Y, (uint32_t)y);
}

/* CameraRcCarActive @8004A978. */
int32_t ae_CameraRcCarActive(AeRam *r)
{
    if (ae_BossKindForArea(r, NULL) != 0) return 0;
    if (ae_rds8(r, AE_MB_CURRENT_AREA_TAG) != 7 ||
        ae_rd32(r, AE_MB_RC_CAR) == 0u) return 0;

    uint32_t state = AE_MB_RC_CAR_STATE;
    if (ae_rd16(r, state + AE_MB_STAGE_DRAW_CURRENT) != 0u) return 0;
    uint16_t mode = ae_rd16(r, state + AE_MB_STAGE_DRAW_MODE);
    switch (mode) {
    case 58: case 53: case 54: case 80: case 82: case 84:
        return 0;
    default:
        return mode != 0x61u;
    }
}

/* BossUpdate @8005AC28. Overlay bodies stay as named hooks. */
int32_t ae_BossUpdate(AeRam *r, const AeMiscHooks *h, uint32_t actor)
{
    if (ae_rd8(r, AE_MB_BOSS_UPDATE_DISABLED) != 0u) return 0;
    int32_t kind = ae_BossKindForArea(r, NULL);
    if (kind < 1 || kind > 5) return 0;

    uint32_t table_target = ae_rd32(r, AE_MB_BOSS_KIND_TABLE + (uint32_t)kind * 4u);
    const char *name = NULL;
    AeMiscCall callback = NULL;
    switch (table_target) {
    case 0x8005AC78u: name = "FUN_801402d8"; callback = h != NULL ? h->FUN_801402d8 : NULL; break;
    case 0x8005AC88u: name = "Area53_BossUpdate"; callback = h != NULL ? h->Area53_BossUpdate : NULL; break;
    case 0x8005AC98u: name = "FUN_80143854"; callback = h != NULL ? h->FUN_80143854 : NULL; break;
    case 0x8005ACA8u: name = "Area46_BossUpdate"; callback = h != NULL ? h->Area46_BossUpdate : NULL; break;
    case 0x8005ACC0u: name = "Area57_BossUpdate"; callback = h != NULL ? h->Area57_BossUpdate : NULL; break;
    default: return 0;
    }
    int32_t result = misc_batch_call(r, h, callback, name, actor, 0, 0, 0);
    return kind == 1 ? 1 : result != 0;
}

/* Indicator_Register @800716EC. */
int32_t ae_Indicator_Register(AeRam *r, const AeMiscHooks *h,
                           int32_t kind, uint32_t data, int32_t angle)
{
    uint32_t count = ae_rd32(r, AE_MB_INDICATOR_COUNT);
    if (count == 4u) {
        (void)misc_batch_call(r, h, h != NULL ? h->exit : NULL,
                              "exit", 1u, 0, 0, 0);
    }

    uint32_t record = AE_MB_INDICATOR_RECORDS + count * AE_MB_INDICATOR_STRIDE;
    ae_wr16(r, record + AE_MB_INDICATOR_KIND, (uint32_t)kind);
    ae_wr16(r, record + AE_MB_INDICATOR_ANGLE, (uint32_t)angle);
    for (uint32_t byte = 0; byte < 8u; ++byte) {
        ae_wr8(r, record + AE_MB_INDICATOR_DATA + byte, ae_rd8(r, data + byte));
    }
    ae_wr16(r, record + AE_MB_INDICATOR_AGE, 0u);
    ae_wr16(r, record + AE_MB_INDICATOR_TIMER, 0u);
    ae_wr16(r, record + AE_MB_INDICATOR_X_VELOCITY, (uint32_t)ae_sdk_rsin(r, angle));
    ae_wr16(r, record + AE_MB_INDICATOR_Y_VELOCITY, 0u);
    ae_wr16(r, record + AE_MB_INDICATOR_X_DIRECTION, (uint32_t)ae_sdk_rcos(r, angle));
    ae_wr16(r, record + AE_MB_INDICATOR_ANGLE_DELTA, 0u);
    ae_wr32(r, AE_MB_INDICATOR_COUNT, count + 1u);
    return (int32_t)count;
}

static void misc_batch_write_move_rect(AeRam *r, uint32_t address,
                                       uint32_t x, uint32_t y,
                                       uint32_t width, uint32_t height)
{
    ae_wr16(r, address + AE_MB_VRAM_SCROLL_RECT_X, x);
    ae_wr16(r, address + AE_MB_VRAM_SCROLL_RECT_Y, y);
    ae_wr16(r, address + AE_MB_VRAM_SCROLL_RECT_W, width);
    ae_wr16(r, address + AE_MB_VRAM_SCROLL_RECT_H, height);
}

static void misc_batch_set_draw_move(AeRam *r, const AeMiscHooks *h,
                                     uint32_t destination, uint32_t rect,
                                     uint32_t source_x, uint32_t source_y)
{
    (void)misc_batch_call(r, h, h != NULL ? h->SetDrawMove : NULL,
                          "SetDrawMove", destination, rect, source_x, source_y);
}

/* VramScrollUpdate @800691F8. Scroll arithmetic and display state updates stay
 * in C; the actual PsyQ DR_MOVE command is a named GPU hook. */
void ae_VramScrollUpdate(AeRam *r, const AeMiscHooks *h)
{
    if (ae_rd32(r, AE_MB_VRAM_SCROLL_STATE + AE_MB_VRAM_SCROLL_LIST) == 0u) return;

    uint32_t entry = ae_rd32(r, AE_MB_VRAM_SCROLL_STATE + AE_MB_VRAM_SCROLL_COUNT);
    uint32_t destination = ae_rd32(r, AE_MB_STAGE_DRAW_STATE + AE_MB_STAGE_DRAW_DESTINATION);
    uint32_t area_index = ae_rd32(r, AE_MB_STAGE_DRAW_STATE + AE_MB_VRAM_SCROLL_AREA_INDEX);
    uint32_t area_words = AE_MB_STAGE_DRAW_STATE + AE_MB_VRAM_SCROLL_AREA_FLAGS +
                          area_index * AE_MB_VRAM_SCROLL_AREA_INDEX_STRIDE;
    int32_t frame = (int32_t)ae_rd32(r, AE_MB_FRAME_COUNTER);
    uint32_t rect = (h != NULL && h->stack_pointer != 0u ? h->stack_pointer : AE_MB_FALLBACK_STACK) -
                    AE_MB_SCROLL_RECT_STACK_SIZE;

    while (ae_rd8(r, entry + AE_MB_VRAM_SCROLL_TYPE) != 0u) {
        uint32_t kind = ae_rd8(r, entry + AE_MB_VRAM_SCROLL_TYPE);
        if (kind == 2u) {
            int32_t numerator = (int32_t)((uint32_t)frame *
                                               ae_rd8(r, entry + AE_MB_VRAM_SCROLL_SPEED));
            if (numerator < 0) numerator += 3;
            int32_t phase = misc_batch_sra(numerator, 2);
            int32_t divisor = ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_HEIGHT);
            int32_t remainder = phase % divisor;
            if (ae_rd8(r, entry + AE_MB_VRAM_SCROLL_PHASE_OFFSET) == 0u) {
                remainder = divisor - remainder;
            }

            misc_batch_write_move_rect(r, rect,
                ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_X_OFFSET),
                (uint32_t)(ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_Y_OFFSET) + remainder),
                ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_WIDTH),
                (uint32_t)(ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_HEIGHT) - remainder));
            misc_batch_set_draw_move(r, h, destination, rect,
                ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_X),
                ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_Y));

            uint32_t destination_word = ae_rd32(r, destination);
            uint32_t area_word = ae_rd32(r, area_words);
            ae_wr32(r, destination,
                    (destination_word & 0xFF000000u) | (area_word & 0x00FFFFFFu));
            ae_wr32(r, area_words,
                    (area_word & 0xFF000000u) | (destination & 0x00FFFFFFu));
            destination += AE_MB_VRAM_SCROLL_DEST_STRIDE;

            if (remainder != 0) {
                misc_batch_write_move_rect(r, rect,
                    ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_X_OFFSET),
                    ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_Y_OFFSET),
                    ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_WIDTH),
                    (uint32_t)(ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_Y) +
                               ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_HEIGHT) - remainder));
                misc_batch_set_draw_move(r, h, destination, rect,
                    ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_X),
                    (uint32_t)(ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_Y) +
                               ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_HEIGHT) - remainder));
                destination_word = ae_rd32(r, destination);
                area_word = ae_rd32(r, area_words);
                ae_wr32(r, destination,
                        (destination_word & 0xFF000000u) | (area_word & 0x00FFFFFFu));
                ae_wr32(r, area_words,
                        (area_word & 0xFF000000u) | (destination & 0x00FFFFFFu));
                destination += AE_MB_VRAM_SCROLL_DEST_STRIDE;
            }
        } else if (kind == 1u) {
            if (ae_rd8(r, entry + AE_MB_VRAM_SCROLL_ACTIVE) == 0u) {
                int32_t numerator = frame + ae_rd8(r, entry + AE_MB_VRAM_SCROLL_PHASE_OFFSET);
                int32_t divisor = ae_rd8(r, entry + AE_MB_VRAM_SCROLL_SPEED);
                if (numerator % divisor == 0) {
                    misc_batch_write_move_rect(r, rect,
                        ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_X),
                        ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_Y),
                        ae_rd8(r, entry + AE_MB_VRAM_SCROLL_VALUE_X),
                        ae_rd8(r, entry + AE_MB_VRAM_SCROLL_VALUE_Y));
                    misc_batch_set_draw_move(r, h, destination, rect,
                        ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_X_OFFSET),
                        ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_Y_OFFSET));
                    uint32_t destination_word = ae_rd32(r, destination);
                    uint32_t area_word = ae_rd32(r, area_words);
                    ae_wr32(r, destination,
                            (destination_word & 0xFF000000u) | (area_word & 0x00FFFFFFu));
                    ae_wr32(r, area_words,
                            (area_word & 0xFF000000u) | (destination & 0x00FFFFFFu));
                    destination += AE_MB_VRAM_SCROLL_DEST_STRIDE;
                }
            }
        } else if (kind == 3u) {
            uint32_t add_y = ae_rd32(r, AE_MB_STAGE_DRAW_STATE + AE_MB_VRAM_SCROLL_AREA_INDEX) != 0u
                           ? 0x100u : 0u;
            misc_batch_write_move_rect(r, rect,
                ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_X),
                (uint32_t)(ae_rd16(r, entry + AE_MB_VRAM_SCROLL_SOURCE_Y) + add_y),
                ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_WIDTH),
                ae_rd8(r, entry + AE_MB_VRAM_SCROLL_RECT_HEIGHT));
            misc_batch_set_draw_move(r, h, destination, rect,
                ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_X_OFFSET),
                ae_rd16(r, entry + AE_MB_VRAM_SCROLL_RECT_Y_OFFSET));
            uint32_t destination_word = ae_rd32(r, destination);
            uint32_t area_word = ae_rd32(r, area_words);
            ae_wr32(r, destination,
                    (destination_word & 0xFF000000u) | (area_word & 0x00FFFFFFu));
            ae_wr32(r, area_words,
                    (area_word & 0xFF000000u) | (destination & 0x00FFFFFFu));
            destination += AE_MB_VRAM_SCROLL_DEST_STRIDE;
        }
        entry += AE_MB_VRAM_SCROLL_ENTRY_SIZE;
    }
    ae_wr32(r, AE_MB_STAGE_DRAW_STATE + AE_MB_STAGE_DRAW_DESTINATION, destination);
}

/* AreaFeature30Update @8009B7FC. CD queueing and archive lookup use their
 * already-verified game ports; the overlay and GPU packet builder stay hooks. */
void ae_AreaFeature30Update(AeRam *r, const AeMiscHooks *h)
{
    uint8_t state = ae_rd8(r, AE_MB_AREA_FEATURE_STATE);
    if (state == 1u) {
        if (ae_rd8(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_STARTED) == 0u) {
            ae_wr32(r, AE_MB_HUD_FLAGS, 0u);
            ae_PlayerSetMode(r, NULL, 11);
            if (ae_rd32(r, AE_PLAYER1_CD_MANAGER) != 0u) return;

            uint32_t stack_base = h != NULL && h->stack_pointer != 0u
                                ? h->stack_pointer : AE_MB_FALLBACK_STACK;
            uint32_t member_position = stack_base - AE_MB_AREA_FEATURE_MEMBER_STACK_SIZE;
            uint32_t command_parameters = stack_base - AE_MB_AREA_FEATURE_COMMAND_STACK_SIZE;
            AePlayer1Hooks player_hooks;
            misc_batch_player1_hooks(&player_hooks, h);
            (void)ae_ArcGetMemberPos(r, &player_hooks, member_position, 0x180u);

            (void)ae_CdMgrQueueCommand(r, &player_hooks, 0x0Eu,
                AE_PLAYER1_CD_MANAGER + AE_MB_CD_MANAGER_QUEUE_COUNT,
                AE_PLAYER1_CD_MANAGER + AE_MB_CD_MANAGER_QUEUE_HEAD, 0u);
            ae_wr32(r, command_parameters + AE_MB_CD_COMMAND_SOURCE,
                    ae_rd32(r, member_position + AE_MB_ARCHIVE_MEMBER_POSITION));
            ae_wr32(r, command_parameters + AE_MB_CD_COMMAND_DESTINATION, 0x80136000u);
            ae_wr32(r, command_parameters + AE_MB_CD_COMMAND_LENGTH,
                    ae_rd32(r, member_position + AE_MB_ARCHIVE_MEMBER_LENGTH) >> 11);
            (void)ae_CdMgrQueueCommand(r, &player_hooks, 2,
                command_parameters, AE_PLAYER1_CD_MANAGER + AE_MB_CD_MANAGER_QUEUE_HEAD, 0u);
            (void)ae_CdMgrQueueCommand(r, &player_hooks, 0x20,
                command_parameters, AE_PLAYER1_CD_MANAGER + AE_MB_CD_MANAGER_QUEUE_HEAD,
                AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_FINISHED);
            ae_wr8(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_STARTED, state);
            ae_wr16(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_PROGRESS, 0u);
            return;
        }

        int32_t progress = ae_rds16(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_PROGRESS);
        if (progress < 240) {
            progress = misc_batch_subu_s32(progress, -16);
            ae_wr16(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_PROGRESS,
                    (uint32_t)progress);
        }
        (void)misc_batch_call(r, h, h != NULL ? h->ScreenBandDraw : NULL,
                              "ScreenBandDraw", 30u, 30u, (uint32_t)progress, 0u);
        if (ae_rd8(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_FINISHED) == 0u) return;

        (void)misc_batch_call(r, h, h != NULL ? h->FUN_80137248 : NULL,
                              "FUN_80137248",
                              ae_rd8(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_ARGUMENT),
                              0, 0, 0);
        ae_wr8(r, AE_MB_AREA_FEATURE_STATE, 2u);
        ae_wr16(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_PROGRESS, 240u);
        (void)misc_batch_call(r, h, h != NULL ? h->ScreenBandDraw : NULL,
                              "ScreenBandDraw", 30u, 30u, 240u, 0u);
        return;
    }

    if (state != 2u) return;
    int32_t progress = ae_rds16(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_PROGRESS);
    if (progress > 0) {
        progress = misc_batch_subu_s32(progress, 16);
        ae_wr16(r, AE_MB_AREA_FEATURE_STATE + AE_MB_AREA_FEATURE_PROGRESS,
                (uint32_t)progress);
        (void)misc_batch_call(r, h, h != NULL ? h->ScreenBandDraw : NULL,
                              "ScreenBandDraw", 30u, 30u, (uint32_t)progress, 0u);
        return;
    }

    ae_wr32(r, AE_MB_AREA_FEATURE_STATE, 0u);
    ae_wr32(r, AE_MB_HUD_FLAGS, 50u);
    ae_PlayerSetMode(r, NULL, 0);
}

static int32_t misc_batch_inside_axis(int32_t point, int32_t center, int32_t half)
{
    return point >= misc_batch_subu_s32(center, half) &&
           point <= (int32_t)((uint32_t)center + (uint32_t)half);
}

/* ColliderScriptFollowPath @800892C4 (table slot 2). Interpolates one route
 * point per frame and carries the player/props by the same signed delta. */
int32_t ae_ColliderScriptFollowPath(AeRam *r, const AeMiscHooks *h,
                                    uint32_t collider, int32_t index)
{
    int32_t route_index = ae_rds16(r, collider + AE_MB_COLLIDER_ROUTE_INDEX);
    if (route_index < 0) return 0;

    uint32_t route_table = ae_rd32(r, AE_MB_COLLIDER_ROUTE_TABLE_PTR);
    uint32_t route = route_table + (uint32_t)route_index * AE_MB_COLLIDER_ROUTE_STRIDE;
    int32_t speed = ae_rds16(r, collider + AE_MB_COLLIDER_SPEED);
    int32_t duration = (int32_t)ae_rds16(r, route + AE_MB_COLLIDER_ROUTE_DURATION) * 2;
    int32_t step[3];
    const uint32_t route_position_offsets[3] = {
        AE_MB_COLLIDER_POSITION_X,
        AE_MB_COLLIDER_POSITION_Y,
        AE_MB_COLLIDER_POSITION_Z
    };
    const uint32_t collider_target_offsets[3] = {
        AE_MB_COLLIDER_ROUTE_X, AE_MB_COLLIDER_ROUTE_Y, AE_MB_COLLIDER_ROUTE_Z
    };
    const uint32_t collider_position_offsets[3] = {
        AE_MB_COLLIDER_POSITION_X, AE_MB_COLLIDER_POSITION_Y, AE_MB_COLLIDER_POSITION_Z
    };
    uint32_t stack_base = AE_MB_FALLBACK_STACK;
    uint32_t delta_address = stack_base - AE_MB_COLLIDER_ROUTE_STACK_SIZE;
    uint32_t target_address = stack_base - AE_MB_COLLIDER_ROUTE_STACK_SIZE;

    for (unsigned axis = 0; axis < 3; ++axis) {
        int32_t difference = misc_batch_subu_s32(
            ae_rds16(r, route + route_position_offsets[axis]),
            ae_rds16(r, collider + collider_target_offsets[axis]));
        int32_t product = (int32_t)((uint32_t)speed * (uint32_t)difference);
        step[axis] = misc_batch_div(product, duration);
    }

    int16_t next_position[3];
    for (unsigned axis = 0; axis < 3; ++axis) {
        uint32_t old_target = ae_rd16(r, collider + collider_target_offsets[axis]);
        next_position[axis] = misc_batch_s16(old_target + (uint32_t)step[axis]);
        uint32_t old_position = ae_rd16(r, collider + collider_position_offsets[axis]);
        ae_wr16(r, delta_address + axis * 2u, next_position[axis] - (int32_t)old_position);
    }

    AeGimmickAHooks gimmick_hooks;
    misc_batch_gimmick_hooks(&gimmick_hooks);
    (void)ae_GimmickCarryBodiesByDelta(r, &gimmick_hooks, collider, delta_address,
                                        (int32_t)(int16_t)index);
    for (unsigned axis = 0; axis < 3; ++axis) {
        ae_wr16(r, target_address + axis * 2u, (uint32_t)next_position[axis]);
    }
    (void)ae_GimmickCarryBodiesSwing(r, &gimmick_hooks, collider, target_address,
                                      (int32_t)(int16_t)index);

    int32_t prop_index = ae_rds16(r, collider + AE_MB_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t props = ae_rd32(r, AE_MB_AREA_PROPS_POINTER);
        uint32_t prop = props + (uint32_t)prop_index * AE_MB_AREA_PROP_STRIDE;
        for (unsigned axis = 0; axis < 3; ++axis) {
            ae_wr16(r, prop + AE_MB_AREA_PROP_POSITION_X + axis * 2u,
                    (uint32_t)next_position[axis]);
        }
        (void)ae_GimmickSwingFirstStep(r, collider);
    }

    for (unsigned axis = 0; axis < 3; ++axis) {
        ae_wr16(r, collider + collider_position_offsets[axis], (uint32_t)next_position[axis]);
    }
    AeZonesHooks zones;
    misc_batch_zone_hooks(&zones, h);
    ae_wr16(r, collider + AE_MB_COLLIDER_ZONE_MASK,
            (uint32_t)ae_ColliderComputeQuadrantMask(r, &zones, collider));
    (void)ae_GimmickSwingAdvance(r, collider);

    uint16_t elapsed = (uint16_t)(ae_rd16(r, collider + AE_MB_COLLIDER_SPEED) + 1u);
    ae_wr16(r, collider + AE_MB_COLLIDER_SPEED, elapsed);
    uint16_t swing_timer = (uint16_t)(ae_rd16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER) + 1u);
    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, swing_timer);
    if ((int16_t)elapsed < (int16_t)(ae_rds16(r, route + AE_MB_COLLIDER_ROUTE_DURATION) * 2)) return 0;

    ae_wr16(r, collider + AE_MB_COLLIDER_SPEED, 0u);
    uint32_t next_route_index = ae_rd16(r, route + AE_MB_COLLIDER_ROUTE_NEXT_INDEX);
    ae_wr16(r, collider + AE_MB_COLLIDER_ROUTE_INDEX, next_route_index);
    uint32_t next_route = route_table + (uint32_t)(int16_t)next_route_index * AE_MB_COLLIDER_ROUTE_STRIDE;
    int32_t next_duration = ae_rds16(r, next_route + AE_MB_COLLIDER_ROUTE_DURATION);
    uint32_t destination_route = next_duration > 0 ? route : next_route;
    if (next_duration > 0) {
        for (unsigned axis = 0; axis < 3; ++axis) {
            ae_wr16(r, collider + collider_target_offsets[axis],
                    ae_rd16(r, destination_route + route_position_offsets[axis]));
        }
    } else {
        ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, 0u);
        for (unsigned axis = 0; axis < 3; ++axis) {
            ae_wr16(r, collider + collider_target_offsets[axis],
                    ae_rd16(r, destination_route + route_position_offsets[axis]));
        }
        ae_wr16(r, collider + AE_MB_COLLIDER_ROUTE_INDEX,
                ae_rd16(r, next_route + AE_MB_COLLIDER_ROUTE_NEXT_INDEX));
    }
    return 0;
}

/* ColliderScriptPlayerMove @8008A4B0 (table slot 9). */
int32_t ae_ColliderScriptPlayerMove(AeRam *r, const AeMiscHooks *h,
                                    uint32_t collider, int32_t index)
{
    uint16_t geometry = ae_rd16(r, collider + AE_MB_COLLIDER_GEOMETRY);
    uint16_t motion_type = geometry & 0x0F00u;
    uint16_t scale = ae_rd16(r, collider + AE_MB_COLLIDER_MOTION_SCALE);
    int32_t motion[3] = {0, 0, 0};
    const uint32_t flow_offsets[3] = {
        AE_MB_COLLIDER_FLOW_X, AE_MB_COLLIDER_FLOW_Y, AE_MB_COLLIDER_FLOW_Z
    };
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (ae_rds16(r, collider + flow_offsets[axis]) != 0) {
            motion[axis] = misc_batch_s16((uint32_t)ae_rd16(r, collider + flow_offsets[axis]) * scale);
        }
    }

    uint32_t player = AE_MB_PLAYER;
    uint32_t player_mode = ae_rd32(r, player + AE_MB_PLAYER_ACTOR_STATE) & 0xF0u;
    int32_t y_adjust = 0;
    if (motion_type == 0x0300u && player_mode == 0x60u) {
        y_adjust = ae_rd16(r, player + AE_MB_PLAYER_ACTOR_HEIGHT_ADJUST);
    }
    int32_t player_x = (int32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION);
    int32_t player_y = (int32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION_Y);
    int32_t player_z = (int32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION_Z);
    int32_t inside_player = misc_batch_inside_axis(player_x,
        ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_X), ae_rds16(r, collider + AE_MB_COLLIDER_HALF_X)) &&
        misc_batch_inside_axis(misc_batch_subu_s32(player_y, -y_adjust),
        ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_Y), ae_rds16(r, collider + AE_MB_COLLIDER_HALF_Y)) &&
        misc_batch_inside_axis(player_z,
        ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_Z), ae_rds16(r, collider + AE_MB_COLLIDER_HALF_Z));

    if (inside_player) {
        if (motion_type == 0x0800u) {
            (void)misc_batch_call(r, h, h != NULL ? h->FUN_8013fc18 : NULL,
                                  "FUN_8013fc18", (uint32_t)motion[0],
                                  (uint32_t)motion[2], 0, 0);
        } else if (motion_type == 0x0300u &&
                   ae_rds16(r, AE_MB_CURRENT_AREA_ID) == 29) {
            (void)misc_batch_call(r, h, h != NULL ? h->FUN_8013fc18 : NULL,
                                  "FUN_8013fc18", (uint32_t)motion[0],
                                  (uint32_t)motion[2], 0, 0);
        }

        if (motion_type == 0x0300u && player_mode == 0x80u) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                int32_t half_motion = motion[axis] / 2;
                uint32_t address = player + AE_MB_PLAYER_ACTOR_VELOCITY + axis * 4u;
                ae_wr32(r, address, ae_rd32(r, address) + (uint32_t)half_motion);
            }
            ae_CameraNudge(r, motion[0], 0, motion[2]);
        } else if (motion_type == 0x0300u) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                uint32_t address = player + AE_MB_PLAYER_ACTOR_VELOCITY + axis * 4u;
                ae_wr32(r, address, ae_rd32(r, address) + (uint32_t)motion[axis]);
            }
            ae_CameraNudge(r, motion[0], 0, motion[2]);

            if (player_mode == 0x60u) {
                AeWorldHooks world;
                misc_batch_world_hooks(&world, h);
                int32_t ground = ae_GetGroundHeight(r, &world,
                    (int32_t)((uint32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION) + (uint32_t)motion[0]),
                    (int32_t)((uint32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION_Y) + (uint32_t)motion[1]),
                    (int32_t)((uint32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION_Z) + (uint32_t)motion[2]));
                int32_t moved_y = misc_batch_subu_s32(
                    (int32_t)ae_rd32(r, player + AE_MB_PLAYER_ACTOR_POSITION_Y), -motion[1]);
                if (moved_y < ground) {
                    AePlayerActHooks player_hooks;
                    misc_batch_player_act_hooks(&player_hooks, h);
                    ae_PlayerSetState(r, &player_hooks, 8u);
                }
            }
        } else if (motion_type != 0x0800u &&
                   (motion_type != 0x0100u || player_mode == 0xC0u)) {
            const uint32_t position_offsets[3] = {
                AE_MB_PLAYER_ACTOR_POSITION,
                AE_MB_PLAYER_ACTOR_POSITION_Y,
                AE_MB_PLAYER_ACTOR_POSITION_Z
            };
            for (unsigned axis = 0; axis < 3; ++axis) {
                uint32_t position = player + position_offsets[axis];
                ae_wr32(r, position, ae_rd32(r, position) + (uint32_t)motion[axis]);
            }
            ae_CameraNudge(r, motion[0], motion[1], motion[2]);
        }
    }

    uint32_t rc_car = AE_MB_RC_CAR_OBJECT;
    if (ae_rd32(r, rc_car + AE_MB_RC_CAR_STATE_FLAGS) == 0u) return 0;
    int32_t rc_x = (int32_t)ae_rd32(r, rc_car + AE_MB_RC_CAR_POSITION);
    int32_t rc_y = (int32_t)ae_rd32(r, rc_car + AE_MB_RC_CAR_POSITION_Y);
    int32_t rc_z = (int32_t)ae_rd32(r, rc_car + AE_MB_RC_CAR_POSITION_Z);
    if (!misc_batch_inside_axis(rc_x, ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_X),
                                ae_rds16(r, collider + AE_MB_COLLIDER_HALF_X)) ||
        !misc_batch_inside_axis(rc_y, ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_Y),
                                ae_rds16(r, collider + AE_MB_COLLIDER_HALF_Y)) ||
        !misc_batch_inside_axis(rc_z, ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_Z),
                                ae_rds16(r, collider + AE_MB_COLLIDER_HALF_Z))) return 0;

    if (motion_type != 0x0300u && motion_type != 0x0800u) {
        for (unsigned axis = 0; axis < 3; ++axis) {
            uint32_t address = rc_car + AE_MB_RC_CAR_POSITION + axis * 4u;
            ae_wr32(r, address, ae_rd32(r, address) + (uint32_t)motion[axis]);
        }
    }
    (void)index;
    return 0;
}

/* ColliderScriptCarry @8008A91C (table slot 10). */
int32_t ae_ColliderScriptCarry(AeRam *r, const AeMiscHooks *h,
                               uint32_t collider, int32_t index)
{
    uint32_t stack_base = AE_MB_FALLBACK_STACK;
    uint32_t delta_address = stack_base - AE_MB_COLLIDER_CARRY_STACK_SIZE;
    for (unsigned axis = 0; axis < 3; ++axis) {
        int32_t delta = misc_batch_subu_s32(
            ae_rd16(r, collider + AE_MB_COLLIDER_POSITION_X + axis * 2u),
            ae_rd16(r, collider + AE_MB_COLLIDER_SCRIPT_START_X + axis * 2u));
        ae_wr16(r, delta_address + axis * 2u, (uint32_t)delta);
    }
    AeGimmickAHooks gimmick_hooks;
    misc_batch_gimmick_hooks(&gimmick_hooks);
    (void)ae_GimmickCarryBodiesByDelta(r, &gimmick_hooks, collider,
                                        delta_address, (int32_t)(int16_t)index);

    if (ae_rds16(r, AE_MB_PLAYER_CURRENT_AREA) == (int16_t)index &&
        (ae_rds16(r, collider + AE_MB_COLLIDER_FLOW_X) != 0 ||
         ae_rds16(r, collider + AE_MB_COLLIDER_FLOW_Y) != 0 ||
         ae_rds16(r, collider + AE_MB_COLLIDER_FLOW_Z) != 0)) {
        for (unsigned axis = 0; axis < 3; ++axis) {
            ae_wr16(r, AE_MB_PLAYER + AE_MB_PLAYER_ACTOR_FACING + axis * 2u,
                    ae_rd16(r, collider + AE_MB_COLLIDER_FLOW_X + axis * 2u));
        }
    }

    int32_t prop_index = ae_rds16(r, collider + AE_MB_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = ae_rd32(r, AE_MB_AREA_PROPS_POINTER) +
                        (uint32_t)prop_index * AE_MB_AREA_PROP_STRIDE;
        for (unsigned axis = 0; axis < 3; ++axis) {
            ae_wr16(r, prop + AE_MB_AREA_PROP_POSITION_X + axis * 2u,
                    ae_rd16(r, collider + AE_MB_COLLIDER_POSITION_X + axis * 2u));
        }
    }

    for (unsigned axis = 0; axis < 3; ++axis) {
        ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_START_X + axis * 2u,
                ae_rd16(r, collider + AE_MB_COLLIDER_POSITION_X + axis * 2u));
    }
    AeZonesHooks zones;
    misc_batch_zone_hooks(&zones, h);
    ae_wr16(r, collider + AE_MB_COLLIDER_ZONE_MASK,
            (uint32_t)ae_ColliderComputeQuadrantMask(r, &zones, collider));
    return 0;
}

/* ColliderResetToStart @8008BDB4. */
void ae_ColliderResetToStart(AeRam *r, uint32_t collider)
{
    uint16_t flags = ae_rd16(r, collider + AE_MB_COLLIDER_FLAGS);
    ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS, (flags & 0x8000u) | 1u);
    for (unsigned axis = 0; axis < 3; ++axis) {
        ae_wr16(r, collider + AE_MB_COLLIDER_POSITION_X + axis * 2u,
                ae_rd16(r, collider + AE_MB_COLLIDER_SCRIPT_START_X + axis * 2u));
    }

    int32_t prop_index = ae_rds16(r, collider + AE_MB_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = ae_rd32(r, AE_MB_AREA_PROPS_POINTER) +
                        (uint32_t)prop_index * AE_MB_AREA_PROP_STRIDE;
        for (unsigned axis = 0; axis < 3; ++axis) {
            ae_wr16(r, prop + AE_MB_AREA_PROP_POSITION_X + axis * 2u,
                    ae_rd16(r, collider + AE_MB_COLLIDER_POSITION_X + axis * 2u));
        }
        ae_wr16(r, prop + AE_MB_AREA_PROP_LAST_POSITION_Z, 0xFFFFu);
    }
    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, 0u);
    ae_wr16(r, collider + AE_MB_COLLIDER_SPEED, 0u);
}

static void misc_batch_command_reset_phase(AeRam *r, uint32_t collider)
{
    uint16_t flags = ae_rd16(r, collider + AE_MB_COLLIDER_FLAGS);
    ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS, (flags & 0x8000u) | 1u);
    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 0u);
    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, 0u);
}

/* ColliderScriptCommand @8008B980. Five command letters have handlers in the
 * executable's jump table; the remaining entries return without side effects. */
int32_t ae_ColliderScriptCommand(AeRam *r, const AeMiscHooks *h,
                                 uint32_t collider, uint32_t command,
                                 int32_t distance, int32_t index)
{
    uint16_t command_letter = ae_rd16(r, command + AE_MB_COLLIDER_COMMAND_LETTER);
    int32_t command_code = (int16_t)(uint16_t)(command_letter - 65u);
    if (command_code < 0 || command_code >= 26) return 0;

    uint16_t collider_flags = ae_rd16(r, collider + AE_MB_COLLIDER_FLAGS);
    uint16_t collider_kind = ae_rd16(r, collider + AE_MB_COLLIDER_GEOMETRY) & 0x00FFu;
    uint16_t shape_kind = ae_rd16(r, collider + AE_MB_COLLIDER_GEOMETRY) & 0x0F00u;
    uint32_t player = AE_MB_PLAYER;

    switch (command_code) {
    case 2: /* C: reset after travelling past the command's distance. */
        if (ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_VALUE) < distance) {
            ae_ColliderResetToStart(r, collider);
        }
        break;

    case 12: { /* M: player contact / command phase switch. */
        if (ae_rds16(r, player + AE_MB_PLAYER_ACTOR_BODY_COLLIDER) == (int16_t)index) {
            uint16_t phase = collider_flags & 0xFu;
            uint16_t player_mode = ae_rd16(r, player + AE_MB_PLAYER_ACTOR_STATE) & 0xF0u;
            if (phase == 1u && player_mode != 0x60u) {
                ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS,
                        (collider_flags & 0x8000u) | 2u);
                if (collider_kind == 0u) {
                    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 32u);
                    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER,
                            ae_rd16(r, command + AE_MB_COLLIDER_COMMAND_VALUE));
                    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, phase);
                }
            } else if (phase == 2u &&
                       ae_rds16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE) == 0 &&
                       collider_kind == 0u) {
                ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS,
                        (collider_flags & 0x8000u) | 1u);
                ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 0u);
                ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, 0u);
            }
        } else if ((collider_flags & 0xFu) == 2u &&
                   (collider_kind == 2u || collider_kind == 8u || collider_kind == 12u) &&
                   ae_rds16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER) == 0) {
            misc_batch_command_reset_phase(r, collider);
        }
        break;
    }

    case 18: { /* S: advance when the referenced collider's script is ready. */
        if ((collider_flags & 0xFu) != 1u) break;
        int32_t referenced_index = ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_VALUE);
        uint32_t collider_table = ae_rd32(r, AE_MB_COLLIDER_SCRIPT_DATA_PTR);
        uint32_t referenced = collider_table +
                              (uint32_t)referenced_index * AE_MB_COLLIDER_RECORD_STRIDE;
        if ((ae_rd16(r, referenced + AE_MB_COLLIDER_FLAGS) & 0xFu) != 2u) break;
        ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS,
                (collider_flags & 0x8000u) | 2u);
        if (collider_kind == 0u) {
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 32u);
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER,
                    ae_rd16(r, command + AE_MB_COLLIDER_COMMAND_RADIUS));
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, 1u);
        }
        break;
    }

    case 19: { /* T: activate the timed phase inside the command radius. */
        int32_t measured_distance = distance;
        int32_t radius_index = ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_VALUE);
        if (radius_index >= 0) {
            uint32_t records = ae_rd32(r, AE_MB_COLLIDER_SCRIPT_DATA_PTR);
            uint32_t target = records + (uint32_t)radius_index * AE_MB_COLLIDER_RECORD_STRIDE;
            int32_t dx = misc_batch_subu_s32(
                ae_rds16(r, target + AE_MB_COLLIDER_POSITION_X),
                ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_X));
            int32_t dy = misc_batch_subu_s32(
                ae_rds16(r, target + AE_MB_COLLIDER_POSITION_Y),
                ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_Y));
            int32_t dz = misc_batch_subu_s32(
                ae_rds16(r, target + AE_MB_COLLIDER_POSITION_Z),
                ae_rds16(r, collider + AE_MB_COLLIDER_POSITION_Z));
            measured_distance = ae_player_vec_length3(dx, dy, dz);
        }
        if (measured_distance < ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_RADIUS)) {
            if ((collider_flags & 0xFu) == 1u) {
                ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS,
                        (collider_flags & 0x8000u) | 2u);
                if (collider_kind == 0u) {
                    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 32u);
                    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, 0u);
                    ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, 1u);
                    ae_wr16(r, collider + AE_MB_COLLIDER_FLAGS,
                            ae_rd16(r, collider + AE_MB_COLLIDER_FLAGS) | 0x100u);
                }
            }
        } else if ((collider_flags & 0x10Fu) == 2u &&
                   (ae_rd16(r, collider + AE_MB_COLLIDER_GEOMETRY) & 0xFFu) != 0u &&
                   (collider_kind == 2u || collider_kind == 8u || collider_kind == 12u) &&
                   ae_rds16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER) == 0) {
            misc_batch_command_reset_phase(r, collider);
        }
        break;
    }

    case 25: { /* Z: animate the timed phase and its per-frame progress. */
        if ((collider_flags & 0xFu) != 2u) break;
        int32_t travel = ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_VALUE);
        int32_t duration = ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_DURATION);
        if (ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_RADIUS) == 0 ||
            shape_kind == 0x0600u) {
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, (uint32_t)travel);
            break;
        }

        int32_t tick = (int32_t)ae_rds16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER) + 1;
        ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, (uint32_t)tick);
        int32_t phase_state = ae_rds16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE);
        if (phase_state == 0) {
            if (tick > ae_rds16(r, command + AE_MB_COLLIDER_COMMAND_DELAY)) {
                ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, 0u);
                ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, 1u);
                ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 0u);
                break;
            }
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 0u);
            break;
        }

        if (tick > duration) {
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_TIMER, 0u);
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_STATE_BYTE, 0u);
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, 0u);
            break;
        }
        int32_t half_duration = misc_batch_sra(duration + ((duration < 0) ? 1 : 0), 1);
        if (shape_kind == 0x0300u && tick < half_duration) {
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED,
                    (uint32_t)misc_batch_div(travel * tick, half_duration));
        } else if (shape_kind == 0x0300u) {
            int32_t tail = tick - half_duration;
            int32_t retreat = misc_batch_div(travel * tail, half_duration);
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, (uint32_t)(travel - retreat));
        } else {
            ae_wr16(r, collider + AE_MB_COLLIDER_SCRIPT_SPEED, (uint32_t)travel);
        }
        break;
    }

    default:
        break;
    }
    (void)h;
    return 0;
}




