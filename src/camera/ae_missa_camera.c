#include "../core/ae_missa.h"
#include "../effects/ae_effects.h"
#include "../ui/ae_hud.h"
#include "../player/ae_player1.h"
#include "../player/ae_player_spawn.h"
#include "ae_camupd.h"
#include "../world/ae_areaset.h"

#define AE_MISSA_CAMERA_STATE             0x800E68D0u
#define AE_MISSA_CAMERA_FOCUS_TARGET      0x800B4D44u
#define AE_MISSA_CAMERA_STATE_MODE_TIMER  0x32u
#define AE_MISSA_CAMERA_STATE_EXIT_GUARD  0x41u
#define AE_MISSA_CAMERA_STATE_AIM_DELAY   0x2Fu
#define AE_MISSA_CAMERA_STATE_AIM_GUARD   0x3Eu
#define AE_MISSA_CAMERA_STATE_AIM_ACTIVE  0x32u
#define AE_MISSA_CAMERA_STATE_FOCUS_STATE 0x48u
#define AE_MISSA_PLAYER                   0x800EC1E0u
#define AE_MISSA_PLAYER_POSITION          0x24u
#define AE_MISSA_PLAYER_FACING            0x60u
#define AE_MISSA_PLAYER_AIRBORNE          0x46u
#define AE_MISSA_RADAR_ENABLED            0x800F513Cu
#define AE_MISSA_EVENT_CAMERA             0x800B8CC0u
#define AE_MISSA_EVENT_CAMERA_START_FLAG  0x25u
#define AE_MISSA_EVENT_CAMERA_SLOT        0x26u
#define AE_MISSA_EVENT_CAMERA_STATE       0x20u
#define AE_MISSA_EVENT_CAMERA_DONE        0x29u
#define AE_MISSA_EVENT_TRIGGER_FLAG       0x800AF130u
#define AE_MISSA_INDEXED_HANDLER_TABLE    0x800AF140u
#define AE_MISSA_AREA_SCRIPT_TABLE_56     0x80141390u
#define AE_MISSA_AREA_SCRIPT_TABLE_66     0x8014440Cu
#define AE_MISSA_STAGE_CURRENT_AREA       0x800F4476u
#define AE_MISSA_TEMP_CAMERA_TARGET       0x800E68D8u
#define AE_MISSA_PLAYER_FLAGS             0x800EC2C0u
#define AE_MISSA_PLAYER_ATTACH_POINTER    0x800EC2B8u
#define AE_MISSA_PLAYER_ATTACH_VALUE      0xDCu
#define AE_MISSA_PLAYER_ATTACH_DURATION   0xDEu
#define AE_MISSA_PLAYER_ATTACH_FLAGS      0xE0u
#define AE_MISSA_PARTICLE_TABLE           0x800B175Cu
#define AE_MISSA_EFFECT_GROUP_TABLE       0x800B1828u
#define AE_MISSA_PAD_HELD                 0x800E5478u
#define AE_MISSA_PAD_CHANGED              0x800E5480u
#define AE_MISSA_PAUSE_DIRECTION_STATE    0x800BEF70u
#define AE_MISSA_PAUSE_DIRECTION_TABLE    0x800B2220u
#define AE_MISSA_PAUSE_DIRECTION_COUNT    5u

static int32_t missa_call_indirect(const AeMissaHooks *hooks, AeRam *r,
                                   uint32_t target, uint32_t a0, uint32_t a1,
                                   uint32_t a2, uint32_t a3)
{
    if (hooks == NULL || hooks->CallIndirect == NULL) {
        ae_ram_fail(r, "missa indirect function hook missing");
        return 0;
    }
    return hooks->CallIndirect(hooks->user, r, target, a0, a1, a2, a3);
}

void ae_CameraRadarFocusSelect(AeRam *r, const AeMissaHooks *hooks)
{
    int32_t usable = ae_RadarUsable(r);
    (void)hooks;
    if (usable != 0 && ae_rd32(r, AE_MISSA_RADAR_ENABLED) != 0u) {
        ae_wr32(r, AE_MISSA_CAMERA_FOCUS_TARGET,
                ae_rd32(r, AE_MISSA_RADAR_ENABLED - 4u));
    } else {
        ae_wr32(r, AE_MISSA_CAMERA_FOCUS_TARGET, 0u);
    }
}

void ae_CameraRadarFocusUpdate(AeRam *r)
{
    uint32_t actor = ae_rd32(r, AE_MISSA_CAMERA_FOCUS_TARGET);
    uint32_t scratch = 0x1F800000u;
    uint32_t state = AE_MISSA_CAMERA_STATE;

    if (actor != 0u) {
        int32_t facing_x = ae_rds16(r, actor + AE_MISSA_PLAYER_FACING);
        int32_t facing_y = ae_rds16(r, actor + AE_MISSA_PLAYER_FACING + 2u);
        int32_t facing_z = ae_rds16(r, actor + AE_MISSA_PLAYER_FACING + 4u);
        int32_t position_x = ae_rds16(r, actor + AE_MISSA_PLAYER_POSITION);
        int32_t position_y = ae_rds16(r, actor + AE_MISSA_PLAYER_POSITION + 4u);
        int32_t position_z = ae_rds16(r, actor + AE_MISSA_PLAYER_POSITION + 8u);

        ae_wr8(r, state + AE_MISSA_CAMERA_STATE_FOCUS_STATE, 0u);
        ae_wr16(r, scratch + 0x40u, (uint32_t)(0 - facing_x));
        ae_wr16(r, scratch + 0x42u, facing_y);
        ae_wr16(r, scratch + 0x44u, (uint32_t)(0 - facing_z));
        ae_wr16(r, scratch + 0x18u, position_x);
        ae_wr16(r, scratch + 0x1Au, position_y);
        if (ae_rds16(r, actor + AE_MISSA_PLAYER_AIRBORNE) < 0) {
            ae_wr16(r, scratch + 0x1Au, (uint32_t)(position_y + 0x180));
            ae_wr8(r, state + AE_MISSA_CAMERA_STATE_FOCUS_STATE, 1u);
        }
        int32_t elevated_y = ae_rds16(r, scratch + 0x1Au);
        ae_wr16(r, scratch + 0x10u, position_x);
        ae_wr16(r, scratch + 0x12u, (uint32_t)(elevated_y - 0x80));
        ae_wr16(r, scratch + 0x14u, position_z);
        ae_wr16(r, scratch + 0x1Cu,
                ae_rd16(r, actor + AE_MISSA_PLAYER_POSITION + 8u));
        ae_wr32(r, scratch + 0x68u, ae_rd32(r, actor + 0x30u));
        ae_wr32(r, scratch + 0x6Cu, ae_rd32(r, actor + 0x34u));
        ae_wr32(r, scratch + 0x70u, ae_rd32(r, actor + 0x38u));
    }

    for (uint32_t byte = 0; byte < 8u; ++byte) {
        ae_wr8(r, state + 0x6Cu + byte, ae_rd8(r, scratch + 0x10u + byte));
        ae_wr8(r, state + 0x74u + byte, ae_rd8(r, scratch + 0x40u + byte));
    }
}

void ae_CameraGetSlotTarget(AeRam *r, uint32_t out)
{
    uint32_t slot = ae_rd32(r, AE_MISSA_TEMP_CAMERA_TARGET);
    uint8_t copy[8];
    for (uint32_t i = 0; i < sizeof(copy); ++i) {
        copy[i] = ae_rd8(r, slot + 0x12u + i);
    }
    for (uint32_t i = 0; i < sizeof(copy); ++i) {
        ae_wr8(r, out + i, copy[i]);
    }
}

void ae_BossAimSfx(AeRam *r, const AeMissaHooks *hooks)
{
    if (ae_rd8(r, AE_MISSA_CAMERA_STATE + AE_MISSA_CAMERA_STATE_AIM_DELAY) != 0u ||
        ae_rd8(r, AE_MISSA_CAMERA_STATE + AE_MISSA_CAMERA_STATE_EXIT_GUARD) != 0u) {
        return;
    }
    if (ae_PlayerCanExitStage(r) == 0) {
        return;
    }
    ae_wr8(r, AE_MISSA_CAMERA_STATE + AE_MISSA_CAMERA_STATE_AIM_DELAY, 12u);
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 0x30);
}

void ae_AimRequestStart(AeRam *r, const AeMissaHooks *hooks)
{
    uint32_t state = AE_MISSA_CAMERA_STATE;
    if (ae_rd8(r, state + AE_MISSA_CAMERA_STATE_AIM_ACTIVE) == 1u ||
        ae_PlayerCanExitStage(r) == 0 ||
        ae_rd8(r, state + AE_MISSA_CAMERA_STATE_AIM_GUARD) != 0u ||
        ae_rd8(r, state + AE_MISSA_CAMERA_STATE_EXIT_GUARD) != 0u) {
        return;
    }

    int32_t boss_kind = ae_BossKindForArea(r,
        hooks != NULL ? hooks->area_set : NULL);
    if (boss_kind != 0) {
        ae_BossAimSfx(r, hooks);
        return;
    }

    ae_wr8(r, state + AE_MISSA_CAMERA_STATE_AIM_ACTIVE, 1u);
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 4);
}

int32_t ae_CameraAreaScriptDispatch(AeRam *r, const AeMissaHooks *hooks,
                                    uint32_t script_index, uint32_t argument,
                                    uint32_t arg2, uint32_t arg3)
{
    uint32_t area = ae_rd16(r, AE_MISSA_STAGE_CURRENT_AREA);
    uint32_t table;
    if (area == 56u) table = AE_MISSA_AREA_SCRIPT_TABLE_56;
    else if (area == 66u) table = AE_MISSA_AREA_SCRIPT_TABLE_66;
    else {
        ae_ram_fail(r, "CameraAreaScriptDispatch called outside areas 56 and 66");
        return 0;
    }
    uint32_t target = ae_rd32(r, table + script_index * 4u);
    (void)arg2;
    return missa_call_indirect(hooks, r, target, argument, argument, table, arg3);
}

int32_t ae_CallIndexedHandler(AeRam *r, const AeMissaHooks *hooks,
                              uint32_t argument, uint32_t handler_index,
                              uint32_t arg2, uint32_t arg3)
{
    uint32_t entry = AE_MISSA_INDEXED_HANDLER_TABLE + handler_index * 4u;
    uint32_t target = ae_rd32(r, entry);
    return missa_call_indirect(hooks, r, target, argument, entry,
                               arg2, arg3);
}

void ae_StageExitStart(AeRam *r, const AeMissaHooks *hooks)
{
    uint32_t camera_state = AE_MISSA_CAMERA_STATE;
    uint32_t exit_camera = AE_MISSA_EVENT_CAMERA;
    uint32_t mode = ae_rd16(r, camera_state + 0x6Au);
    if (mode == 0u) {
        ae_StageExitRequest(r, hooks != NULL ? hooks->camupd : NULL);
        return;
    }
    ae_wr8(r, exit_camera + AE_MISSA_EVENT_CAMERA_START_FLAG, 0u);
    ae_wr8(r, exit_camera + AE_MISSA_EVENT_CAMERA_SLOT, 0u);
    ae_wr16(r, exit_camera + AE_MISSA_EVENT_CAMERA_STATE, 0u);
    ae_wr8(r, exit_camera + AE_MISSA_EVENT_CAMERA_DONE, 1u);
    ae_wr8(r, exit_camera + 0x24u,
           ae_rd8(r, camera_state + 0x6Au) + 1u);
}

void ae_EventCamStartVictory(AeRam *r, const AeMissaHooks *hooks)
{
    (void)ae_EventCamCreate(r, hooks != NULL ? hooks->camupd : NULL, 4, 0, 0);
    uint32_t state = AE_MISSA_CAMERA_STATE;
    ae_wr8(r, state + AE_MISSA_CAMERA_STATE_EXIT_GUARD, 1u);
    ae_wr16(r, state + 0x30u, 0u);
    ae_wr8(r, state + AE_MISSA_CAMERA_STATE_AIM_GUARD, 1u);
    ae_CameraFixedModeOff(r);
    ae_wr8(r, AE_MISSA_EVENT_TRIGGER_FLAG, 24u);
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 9);
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 10);
}

void ae_EventCamStartDefault(AeRam *r, const AeMissaHooks *hooks)
{
    (void)ae_EventCamCreate(r, hooks != NULL ? hooks->camupd : NULL, 1, 0, 0);
    uint32_t state = AE_MISSA_CAMERA_STATE;
    ae_wr8(r, state + AE_MISSA_CAMERA_STATE_EXIT_GUARD, 1u);
    ae_wr16(r, state + 0x30u, 0u);
    ae_CameraFixedModeOff(r);
}

void ae_PlayerAttachSet(AeRam *r, const AeMissaHooks *hooks,
                        uint32_t attach_record, uint32_t duration,
                        uint32_t arg2, uint32_t arg3)
{
    uint32_t player = AE_MISSA_PLAYER;
    uint32_t flags = ae_rd32(r, player + AE_MISSA_PLAYER_ATTACH_FLAGS) | 8u;
    uint32_t attached_actor = ae_rd32(r, attach_record);
    uint32_t attach_value = ae_rd16(r, attach_record + 4u);
    ae_wr16(r, player + AE_MISSA_PLAYER_ATTACH_DURATION, duration);
    ae_wr32(r, player + AE_MISSA_PLAYER_ATTACH_FLAGS, flags);
    ae_wr32(r, AE_MISSA_PLAYER_ATTACH_POINTER, attached_actor);
    ae_wr16(r, player + AE_MISSA_PLAYER_ATTACH_VALUE, attach_value);
    ae_PlayerFxSpawn116(r, hooks != NULL ? hooks->spawn : NULL);
    (void)arg2;
    (void)arg3;
}

void ae_PlayerSetBounceFlag(AeRam *r)
{
    uint32_t flags = ae_rd32(r, AE_MISSA_PLAYER_FLAGS);
    ae_wr32(r, AE_MISSA_PLAYER_FLAGS, flags | 0x40u);
}

void ae_PadDirectionCode(AeRam *r, const AeMissaHooks *hooks)
{
    uint32_t input = ae_rd32(r, AE_MISSA_PAD_HELD) &
                     ae_rd32(r, AE_MISSA_PAD_CHANGED);
    static const uint16_t masks[8] = {0x1000u, 0x4000u, 0x8000u, 0x2000u,
                                      0x0004u, 0x0001u, 0x0008u, 0x0002u};
    uint32_t direction = 0u;
    for (uint32_t i = 0; i < 8u; ++i) {
        if ((input & masks[i]) != 0u) direction = i + 1u;
    }
    if (direction == 0u) return;

    uint32_t code = (ae_rd32(r, AE_MISSA_PAUSE_DIRECTION_STATE) << 4) | direction;
    ae_wr32(r, AE_MISSA_PAUSE_DIRECTION_STATE, code);
    for (uint32_t i = 0; i < AE_MISSA_PAUSE_DIRECTION_COUNT; ++i) {
        if (code == ae_rd32(r, AE_MISSA_PAUSE_DIRECTION_TABLE + i * 4u)) {
            uint32_t stage_state = 0x800F4350u;
            uint32_t player = AE_MISSA_PLAYER;
            if (i == 0u) ae_wr16(r, stage_state + 0x154u, 49u);
            else if (i == 1u) ae_wr16(r, stage_state + 0x156u, 49u);
            else if (i == 2u) ae_wr16(r, stage_state + 0x154u, 99u);
            else if (i == 3u) ae_wr16(r, stage_state + 0x156u, 99u);
            else ae_wr16(r, player + 0x148u, 890u);
            (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 13);
            ae_wr32(r, AE_MISSA_PAUSE_DIRECTION_STATE, 0u);
            return;
        }
    }
}
