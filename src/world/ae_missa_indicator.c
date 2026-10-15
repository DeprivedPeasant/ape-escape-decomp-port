#include "../core/ae_missa.h"

#include "ae_misc.h"
#include "ae_stage.h"
#include "ae_areaset.h"
#include "ae_zako2.h"
#include "../player/ae_player1.h"
#include "../player/ae_player_spawn.h"
#include "../player/ae_player_act.h"

#define AE_MISSA_INDICATOR_TABLE          0x800BBD34u
#define AE_MISSA_INDICATOR_STRIDE         24u
#define AE_MISSA_INDICATOR_KIND           0x00u
#define AE_MISSA_INDICATOR_POSITION       0x04u
#define AE_MISSA_INDICATOR_TIMER          0x0Au
#define AE_MISSA_INDICATOR_FRAME          0x14u
#define AE_MISSA_INDICATOR_STATE          0x16u
#define AE_MISSA_INDICATOR_HINT_GLOBAL    0x800BBC50u
#define AE_MISSA_INDICATOR_HINT_POINTER   0x144u
#define AE_MISSA_INDICATOR_HINT_DONE      0x149u
#define AE_MISSA_INDICATOR_HINT_REQUEST   0x14Au
#define AE_MISSA_INDICATOR_HINT_STATE     (AE_MISSA_INDICATOR_HINT_GLOBAL + AE_MISSA_INDICATOR_HINT_POINTER)
#define AE_MISSA_INDICATOR_CD_STATUS      0x800E38D4u
#define AE_MISSA_INDICATOR_FLAGS          0x800F4DD0u
#define AE_MISSA_INDICATOR_FLAG_MASK      0x10u
#define AE_MISSA_INDICATOR_HEAP_BYTES     0x800u
#define AE_MISSA_INDICATOR_MEMBER         20u
#define AE_MISSA_INDICATOR_PICKUP_KIND    1u
#define AE_MISSA_INDICATOR_PICKUP_MODE    3
#define AE_MISSA_INDICATOR_PICKUP_COUNT   3u
#define AE_MISSA_INDICATOR_HINT_FRAME     29u
#define AE_MISSA_INDICATOR_PICKUP_FRAME   58u
#define AE_MISSA_INDICATOR_PICKUP_TIMER   90u
#define AE_MISSA_INDICATOR_PICKUP_Y_BIAS  320u
#define AE_MISSA_INDICATOR_SLOT_BYTES     104u
#define AE_MISSA_INDICATOR_HINT_LOCAL     40u
#define AE_MISSA_INDICATOR_CD_LOCAL       16u
#define AE_MISSA_INDICATOR_LOC_LOCAL      24u
#define AE_MISSA_INDICATOR_LOC_REQUEST    8u
#define AE_MISSA_INDICATOR_PICKUP_LOCAL   64u
#define AE_MISSA_INDICATOR_STAGE_ACTION   3
#define AE_MISSA_INDICATOR_PLAYER_MODE    11
#define AE_MISSA_INDICATOR_SOUND          15u

static int32_t missa_indicator_hook(AeRam *r, const AeMissaHooks *hooks,
                                    AeMissaHookFn callback,
                                    const char *name)
{
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "Indicator_Activate: %s hook missing", name);
        return 0;
    }
    return callback(hooks->user, r, 0u, 0u, 0u, 0u);
}

static uint32_t missa_indicator_frame_pointer(const AeMissaHooks *hooks,
                                               uint32_t local_offset)
{
    uint32_t stack_pointer = hooks != NULL && hooks->stack_pointer != 0u
        ? hooks->stack_pointer : 0x80200000u;
    return stack_pointer - AE_MISSA_INDICATOR_SLOT_BYTES + local_offset;
}

/* 800717B4. The hint protocol and pickup requests are game logic; message
 * opening, CD conversion/queueing and sound remain named native boundaries. */
void ae_Indicator_Activate(AeRam *r, const AeMissaHooks *hooks,
                           int32_t index, int32_t sector)
{
    uint32_t indicator = AE_MISSA_INDICATOR_TABLE +
                         (uint32_t)index * AE_MISSA_INDICATOR_STRIDE;
    int32_t kind = ae_rds16(r, indicator + AE_MISSA_INDICATOR_KIND);
    uint32_t state = ae_rd16(r, indicator + AE_MISSA_INDICATOR_STATE);

    if (kind < 0 || kind > 2) return;

    if (kind == 2) {
        if (state != 0u || ae_rds16(r, indicator + AE_MISSA_INDICATOR_TIMER) > 0)
            return;

        ae_wr16(r, indicator + AE_MISSA_INDICATOR_STATE, 1u);
        ae_wr16(r, indicator + AE_MISSA_INDICATOR_FRAME,
                AE_MISSA_INDICATOR_PICKUP_FRAME);
        ae_wr16(r, indicator + AE_MISSA_INDICATOR_TIMER,
                AE_MISSA_INDICATOR_PICKUP_TIMER);

        uint32_t pickup_position = missa_indicator_frame_pointer(
            hooks, AE_MISSA_INDICATOR_PICKUP_LOCAL);
        ae_wr16(r, pickup_position,
                ae_rd16(r, indicator + AE_MISSA_INDICATOR_POSITION));
        ae_wr16(r, pickup_position + 2u,
                ae_rd16(r, indicator + AE_MISSA_INDICATOR_POSITION + 2u) -
                AE_MISSA_INDICATOR_PICKUP_Y_BIAS);
        ae_wr16(r, pickup_position + 4u,
                ae_rd16(r, indicator + AE_MISSA_INDICATOR_POSITION + 4u));
        for (uint32_t request = 0; request < AE_MISSA_INDICATOR_PICKUP_COUNT;
             ++request) {
            ae_SpawnPickups(r, hooks != NULL ? hooks->zako2 : NULL,
                            AE_MISSA_INDICATOR_PICKUP_KIND, pickup_position,
                            AE_MISSA_INDICATOR_PICKUP_MODE, 0u);
        }
        (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL,
                         AE_MISSA_INDICATOR_SOUND);
        return;
    }

    if (state == 1u || ae_MessageBoxBusy(r) != 0) return;
    (void)missa_indicator_hook(r, hooks,
        hooks != NULL ? hooks->MessageBoxOpen : NULL, "MessageBoxOpen");

    ae_wr16(r, indicator + AE_MISSA_INDICATOR_STATE, 1u);
    ae_wr16(r, indicator + AE_MISSA_INDICATOR_FRAME,
            AE_MISSA_INDICATOR_HINT_FRAME);

    uint32_t cd_position = missa_indicator_frame_pointer(
        hooks, AE_MISSA_INDICATOR_HINT_LOCAL);
    uint32_t cd_command = missa_indicator_frame_pointer(
        hooks, AE_MISSA_INDICATOR_CD_LOCAL);
    uint32_t cd_location = missa_indicator_frame_pointer(
        hooks, AE_MISSA_INDICATOR_LOC_LOCAL);
    ae_wr32(r, AE_MISSA_INDICATOR_HINT_STATE,
            ae_HeapAlloc(r, hooks != NULL ? hooks->area_set : NULL,
                         AE_MISSA_INDICATOR_HEAP_BYTES));
    ae_wr8(r, AE_MISSA_INDICATOR_HINT_GLOBAL + AE_MISSA_INDICATOR_HINT_REQUEST,
           0u);

    ae_ArcGetMemberPos(r, hooks != NULL ? hooks->player1 : NULL,
                       cd_position, AE_MISSA_INDICATOR_MEMBER);
    uint32_t start_sector = hooks != NULL && hooks->player1 != NULL &&
                            hooks->player1->CdPosToInt != NULL
        ? hooks->player1->CdPosToInt(hooks->player1->user, r,
                                     cd_position, 0u, 0u, 0u)
        : 0u;
    if (hooks == NULL || hooks->player1 == NULL ||
        hooks->player1->CdIntToPos == NULL ||
        hooks->player1->CdPosToInt == NULL) {
        ae_ram_fail(r, "Indicator_Activate: CD conversion hooks missing");
        return;
    }
    (void)hooks->player1->CdIntToPos(hooks->player1->user, r,
                                     start_sector + (uint32_t)sector,
                                     cd_location, 0u, 0u);

    uint32_t status = AE_MISSA_INDICATOR_CD_STATUS;
    uint32_t request_flag = AE_MISSA_INDICATOR_HINT_GLOBAL +
                            AE_MISSA_INDICATOR_HINT_REQUEST;
    ae_wr8(r, cd_command, 0x80u);
    (void)ae_CdMgrQueueCommand(r, hooks->player1, 14, cd_command,
                               status, 0u);
    ae_wr32(r, cd_location + 4u,
            ae_rd32(r, AE_MISSA_INDICATOR_HINT_STATE));
    ae_wr32(r, cd_location + AE_MISSA_INDICATOR_LOC_REQUEST, 1u);
    (void)ae_CdMgrQueueCommand(r, hooks->player1, 2, cd_location,
                               status, 0u);
    (void)ae_CdMgrQueueCommand(r, hooks->player1, 32, cd_location,
                               status, request_flag);

    ae_wr8(r, AE_MISSA_INDICATOR_HINT_GLOBAL + AE_MISSA_INDICATOR_HINT_DONE,
           1u);
    (void)ae_StageRequest(r, hooks != NULL ? hooks->stage : NULL,
                          AE_MISSA_INDICATOR_STAGE_ACTION, 0, 0);
    ae_PlayerSetMode(r, hooks != NULL ? hooks->player_act : NULL,
                     AE_MISSA_INDICATOR_PLAYER_MODE);
    ae_wr32(r, AE_MISSA_INDICATOR_FLAGS,
            ae_rd32(r, AE_MISSA_INDICATOR_FLAGS) &
            ~AE_MISSA_INDICATOR_FLAG_MASK);
    ae_AimRequestClear(r);
    ae_wr32(r, AE_MISSA_INDICATOR_FLAGS,
            ae_rd32(r, AE_MISSA_INDICATOR_FLAGS));
    ae_CameraFixedModeOff(r);
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL,
                     AE_MISSA_INDICATOR_SOUND);
}
