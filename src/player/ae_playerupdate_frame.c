#include "ae_playerupdate_frame.h"
#include "ae_playerupdate_internal.h"
#include "../render/ae_tex_copy.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "native_player.h"
#define TURN_MATRIX 0x1F800028u

#define PU_MODE 0x800F44A0u
#define PU_AREA 0x800F4476u
#define PU_ENTRY 0x800F447Au
#define PU_FRAME_COUNTER 0x800EC350u
#define PU_IDLE_BLOCK 0x800B4D70u
#define PU_IDLE_VARIANT 0x800F4620u
#define PU_CD_STATUS 0x800E38D4u
#define PU_CD_PROGRESS 0x800E3898u
#define PU_CD_SECTOR 0x800E38A0u
#define PU_MODE_ACTOR 0x800B4DC4u
#define PU_MODE_CALLBACK1 0x800B4DC8u
#define PU_MODE_CALLBACK3 0x800B4DC0u
#define PU_GADGET_FLAGS 0x800F5120u
#define PU_PAD_LEFT_WORLD 0x800E54B4u
#define PU_PAD_RIGHT_WORLD 0x800E54BCu
#define PU_AIR 0x800F4DC8u
#define PU_AIR_WARNING 0x800F4DCAu
#define PU_SURFACE_QUERY 0x1F8003F8u
#define PU_SURFACE_WATER 0x1F8003DEu
#define PU_SURFACE_DEPTH 0x1F8003E4u
#define PU_SURFACES 0x800AEF60u
#define PU_ACTIONS 0x800B03ACu
#define PU_TURN_LEFT_MATRIX 0x800AE740u
#define PU_TURN_RIGHT_MATRIX 0x800AE760u
#define PU_SWIM_BOOST 0x800F51E4u
#define PU_HOOP_START 0x800F5126u
#define PU_HURT_BLINK 0x800F4DCEu
#define PU_CLUB_BODY1 0x800F516Cu
#define PU_CLUB_BODY2 0x800F5170u
#define PU_NET_POSITION 0x800F5180u
#define PU_GADGET3_AIM 0x800F5144u
#define PU_HOOP_STATE 0x800F5124u
#define PU_HOOP_AUX 0x800F5128u

#define PU_POSITION 0x024u
#define PU_CONTACT 0x050u
#define PU_ANIM_FRAME 0x05Cu
#define PU_FACING 0x060u
#define PU_UP 0x068u
#define PU_STATE_FLAGS 0x070u
#define PU_DESIRED_DIR 0x074u
#define PU_SAVED_FACING 0x080u
#define PU_PREV_FACING 0x03Cu
#define PU_NEXT_FRAME 0x088u
#define PU_ANIM_FRACTION 0x08Au
#define PU_ANIM_BLEND 0x08Cu
#define PU_ANIM_SPEED 0x08Eu
#define PU_ATTACH_DIRECTION 0x0D8u
#define PU_ATTACH_FLAGS 0x0E0u
#define PU_PREV_ATTACH_FLAGS 0x0E4u
#define PU_STEADY_MOVE 0x0EEu
#define PU_HAZARD_TIMER 0x0F1u
#define PU_FALL_FRAMES 0x0FCu
#define PU_SQUASH_TIMER 0x0FEu
#define PU_FLAGS_OVERRIDE 0x102u
#define PU_GROUND_Y 0x12Cu
#define PU_LAST_POSITION 0x130u
#define PU_HAS_GROUND 0x136u
#define PU_PREV_GROUND_TYPE 0x13Au
#define PU_WATER_TOP 0x13Eu
#define PU_SURFACE_KIND 0x0ECu
#define PU_PITCH 0x07Cu
#define PU_IDLE_TIMER 0x148u
#define PU_IDLE_LOADING 0x14Au
#define PU_IDLE_READY 0x14Bu
#define PU_SLOPE_X 0x104u
#define PU_SLOPE_Z 0x106u
#define PU_PARAM_SPEED 0x800EC034u
#define PU_PARAM_MOVE_SCALE 0x800EC02Cu
#define PU_STATE_ENTRY_BYTES 8u
#define PU_ENTRY_FIRST 0u
#define PU_ENTRY_LAST 2u
#define PU_ENTRY_TURN 4u
#define PU_ENTRY_MOVE 5u
#define PU_ENTRY_ACTION 6u
#define PU_ENTRY_FLAGS 7u
#define PU_LOCAL_NORMAL 0x10u
#define PU_LOCAL_POSITION1 0x28u
#define PU_LOCAL_POSITION2 0x30u
#define PU_LOCAL_POSITION3 0x38u
#define PU_LOCAL_CD_MODE 0x50u
#define PU_IDLE_BUFFER 0x80u
#define PU_IDLE_BLOCK_VARIANT 0x86u
#define PU_MODE_ACTOR_DELAY 0x8Fu
#define PU_MODE_ACTOR_FALL 0x8Eu
#define PU_BODY_DIRECTION 0x18u
#define PU_BODY_RADIUS 0x20u
#define PU_BODY_HEIGHT 0x24u
#define PU_BODY_FLAGS 0x26u
#define PU_BODY_CONTACT 0x2Cu
#define PU_SURFACE_ACCEL_SLIP 4u
#define PU_SURFACE_BUMP_LIMIT 10u
#define PU_SURFACE_SLIP_LIMIT 14u
#define PU_SVECTOR_Y 2u
#define PU_TURN_FACING 8u
#define PU_TURN_TARGET 16u
#define PU_CD_READ_BUFFER 4u
#define PU_CD_READ_SECTORS 8u

static void playerupdate_copy_vector(AeRam *r, uint32_t source, uint32_t target)
{
    uint32_t xy = ae_rd32(r, source), zp = ae_rd32(r, source + PU_SVECTOR_Z);
    ae_wr32(r, target, xy);
    ae_wr32(r, target + PU_SVECTOR_Z, zp);
}

static uint32_t playerupdate_entry(AeRam *r)
{
    return ae_rd32(r, PU_SCRATCH + PU_INPUT_STATE_TABLE) +
        (ae_rd16(r, PU_PLAYER + PU_STATE) & 127u) * PU_STATE_ENTRY_BYTES;
}

static void playerupdate_set_mode(AeRam *r, const AePlayerUpdateFrameHooks *h, int32_t mode)
{
    if (mode == 1) {
        if (!h->PlayerActorCameraTable) {
            ae_ram_fail(r, "PlayerUpdate: missing actor camera-script table service");
            return;
        }
        uint32_t table = h->PlayerActorCameraTable(h->user, r);
        if (ae_ram_failed(r)) return;
        AePlayerActHooks actor = {0};
        if (h->act) actor = *h->act;
        actor.incoming_a2 = table;
        ae_PlayerSetMode(r, &actor, mode);
    } else {
        ae_PlayerSetMode(r, h->act, mode);
    }
}

static void playerupdate_idle(AeRam *r, const AePlayerUpdateFrameHooks *h, uint32_t locals)
{
    if (ae_rd16(r, PU_MODE) != 0) return;
    int interrupted = ae_rd16(r, PU_LOCK) == 11 || ae_rd32(r, PU_PAD_CLASS) != 0 ||
        (ae_rd32(r, PU_PAD_HELD) & 0xEFFu) != 0 || ae_rd8(r, PU_CAMERA_ACTIVE) == 1;
    for (uint32_t axis = 0; axis < 3; ++axis)
        interrupted |= ae_rds32(r, PU_PLAYER + PU_POSITION + axis * 4u) !=
            ae_rds16(r, PU_PLAYER + PU_LAST_POSITION + axis * 2u);
    if (interrupted) {
        ae_wr16(r, PU_PLAYER + PU_IDLE_TIMER, 0);
        if (ae_rd8(r, PU_PLAYER + PU_IDLE_READY)) {
            ae_HeapFree(r, ae_rd32(r, ae_rd32(r, PU_IDLE_BLOCK) + PU_IDLE_BUFFER));
            ae_HeapFree(r, ae_rd32(r, PU_IDLE_BLOCK));
            ae_PlayerIdleModelSetup(r, 0);
        }
        return;
    }
    uint32_t timer = (ae_rd16(r, PU_PLAYER + PU_IDLE_TIMER) + 1u) & 65535u;
    ae_wr16(r, PU_PLAYER + PU_IDLE_TIMER, timer);
    if (timer <= 900 || ae_rd16(r, PU_PLAYER + PU_STATE) >= 2) return;
    if (ae_rd8(r, PU_PLAYER + PU_IDLE_READY)) {
        ae_PlayerSetMode(r, h->act, 10);
    } else if (!ae_rd8(r, PU_PLAYER + PU_IDLE_LOADING)) {
        uint32_t buffer = ae_HeapAlloc(r, h->area_set, 0xA000u);
        uint32_t block = ae_HeapAlloc(r, h->area_set, 0x88u);
        ae_wr32(r, PU_IDLE_BLOCK, block);
        ae_wr32(r, block + PU_IDLE_BUFFER, buffer);
        ae_ArcGetMemberPos(r, h->player1, locals + PU_LOCAL_POSITION3, 19);
        uint32_t sector = (uint32_t)h->CdPosToInt(h->user, r, locals + PU_LOCAL_POSITION3);
        uint32_t variant = ae_rd32(r, PU_FRAME_COUNTER) & 7u;
        if (variant == ae_rd32(r, PU_IDLE_VARIANT)) variant = (variant + 1u) & 7u;
        ae_wr32(r, PU_IDLE_VARIANT, variant);
        ae_wr16(r, ae_rd32(r, PU_IDLE_BLOCK) + PU_IDLE_BLOCK_VARIANT, variant);
        h->CdIntToPos(h->user, r, sector + variant * 20u, locals + PU_LOCAL_POSITION1);
        ae_wr8(r, locals + PU_LOCAL_CD_MODE, 128);
        h->CdMgrQueueCommand(h->user, r, 14, locals + PU_LOCAL_CD_MODE, PU_CD_STATUS, 0);
        ae_wr32(r, locals + PU_LOCAL_POSITION1 + PU_CD_READ_BUFFER,
            ae_rd32(r, ae_rd32(r, PU_IDLE_BLOCK) + PU_IDLE_BUFFER));
        ae_wr32(r, locals + PU_LOCAL_POSITION1 + PU_CD_READ_SECTORS, 20);
        h->CdMgrQueueCommand(h->user, r, 2, locals + PU_LOCAL_POSITION1, PU_CD_STATUS, 0);
        h->CdMgrQueueCommand(h->user, r, 32, locals + PU_LOCAL_POSITION1, PU_CD_STATUS,
                            PU_PLAYER + PU_IDLE_READY);
        ae_wr8(r, PU_PLAYER + PU_IDLE_LOADING, 1);
        ae_StageRequest(r, h->stage, 3, 0, 0);
        ae_wr32(r, PU_CD_PROGRESS, 0);
        ae_wr32(r, PU_CD_SECTOR, UINT32_MAX);
    }
}

static void playerupdate_attachment(AeRam *r, const AePlayerUpdateFrameHooks *h, uint32_t locals)
{
    if ((ae_rd32(r, PU_PLAYER + PU_CONTACT) >> 28) == 8) {
        uint32_t object = ae_GetObjectByIndex(r, ae_rds16(r, PU_PLAYER + PU_CONTACT));
        ae_wr16(r, locals + PU_LOCAL_POSITION1, ae_rd16(r, object));
        ae_wr16(r, locals + PU_LOCAL_POSITION1 + PU_SVECTOR_Y, ae_rd16(r, object + PU_VECTOR_Y) - 128u);
        ae_wr16(r, locals + PU_LOCAL_POSITION1 + PU_SVECTOR_Z, ae_rd16(r, object + PU_VECTOR_Z));
        if (ae_StageExitIsActive(r)) {
            int32_t area = ae_rd16(r, PU_AREA);
            ae_StageRequest(r, h->stage, area == 87 ? 33 : 16, area, ae_rd16(r, PU_ENTRY));
        }
        ae_wr32(r, PU_PLAYER + PU_CONTACT, 0);
    } else if (ae_rd16(r, PU_MODE) != 13 && ae_rds16(r, PU_PLAYER + PU_HEALTH) > 0) {
        if (ae_rd32(r, PU_PLAYER + PU_ATTACH_FLAGS) & 2u) {
            if (!ae_PlayerIsOnGround(r) || (ae_rd32(r, PU_PAD_HELD) & 10u))
                ae_PlayerSetMode(r, h->act, 5);
            else ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y, 0);
        }
        if ((ae_rd32(r, PU_PLAYER + PU_ATTACH_FLAGS) & 4u) && ae_rd16(r, PU_MODE) != 6 &&
            ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y) >= 0)
            ae_PlayerSetMode(r, h->act, 6);
        if ((ae_rd32(r, PU_PLAYER + PU_ATTACH_FLAGS) & 16u) &&
            ae_rds16(r, PU_PLAYER + PU_HEALTH) > 0 &&
            !(ae_rd32(r, PU_PLAYER + PU_PREV_ATTACH_FLAGS) & 16u)) {
            uint32_t actor = ae_rd32(r, PU_MODE_ACTOR);
            if (ae_rds8(r, actor + PU_MODE_ACTOR_DELAY) >= 2)
                ae_wr8(r, actor + PU_MODE_ACTOR_DELAY, ae_rd8(r, actor + PU_MODE_ACTOR_DELAY) - 1u);
            else if (!ae_rd16(r, PU_MODE)) {
                if (ae_rd32(r, PU_GADGET_FLAGS) & 8u) ae_PlayerSetMode(r, h->act, 3);
                else if ((ae_rd32(r, PU_GADGET_FLAGS) & 4u) &&
                         ae_rds8(r, actor + PU_MODE_ACTOR_FALL) > 0)
                    playerupdate_set_mode(r, h, 1);
            }
        }
    }
    ae_wr32(r, PU_PLAYER + PU_PREV_ATTACH_FLAGS, ae_rd32(r, PU_PLAYER + PU_ATTACH_FLAGS));
}

static void playerupdate_restore_velocity(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    ae_wr16(r, PU_SCRATCH, ae_rd16(r, PU_PLAYER + PU_VELOCITY) -
              ae_rd16(r, PU_PLAYER + PU_TARGET_VELOCITY));
    ae_wr16(r, PU_SCRATCH + 2u, 0);
    ae_wr16(r, PU_SCRATCH + 4u, ae_rd16(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Z) -
              ae_rd16(r, PU_PLAYER + PU_TARGET_VELOCITY + PU_VECTOR_Z));
    if (ae_sdk_VectorNormalSS(r, PU_SCRATCH, PU_SCRATCH) > 256) {
        ae_wr32(r, PU_PLAYER + PU_VELOCITY, (uint32_t)(ae_rds32(r, PU_PLAYER + PU_VELOCITY) / 4));
        ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Z,
                (uint32_t)(ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Z) / 4));
    }
    uint32_t target_y = ae_rd32(r, PU_PLAYER + PU_TARGET_VELOCITY + PU_VECTOR_Y);
    int32_t velocity_y = ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y);
    if ((int32_t)(target_y - (uint32_t)velocity_y) >= 8 && velocity_y < 0)
        ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y, target_y - 7u);
    uint32_t flags = ae_rd32(r, PU_PLAYER + PU_ATTACH_FLAGS);
    if (flags & 8u) {
        playerupdate_copy_vector(r, PU_PLAYER + PU_ATTACH_DIRECTION, PU_PLAYER + PU_FACING);
        ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY,
                (uint32_t)ae_rds16(r, PU_PLAYER + PU_ATTACH_DIRECTION) << 5);
        ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + PU_VECTOR_Z,
                (uint32_t)ae_rds16(r, PU_PLAYER + PU_ATTACH_DIRECTION + 4u) << 5);
        ae_PlayerSetState(r, h->act, 6);
        ae_PlaySfx(r, h->sound, 39);
    } else if (flags & 64u) {
        ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y, (uint32_t)-110);
        ae_wr32(r, PU_PLAYER + PU_POSITION_Y, ae_rd32(r, PU_PLAYER + PU_POSITION_Y) - 1u);
        ae_PlayerSetState(r, h->act, 44);
        ae_EffectGroundImpactAtPlayer(r, h->world, h->spawn, h->player2);
        if (!ae_rd16(r, PU_PAD_CLASS))
            playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, PU_PLAYER + PU_DESIRED_DIR);
        else {
            for (uint32_t axis = 0; axis < 2; ++axis)
                ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + axis * 8u,
                    (uint32_t)((ae_rds16(r, PU_PAD_LEFT_WORLD + axis * 4u) *
                               (int32_t)ae_rd16(r, PU_PAD_LEFT_MAG)) >> 2));
            playerupdate_copy_vector(r, PU_PAD_LEFT_WORLD, PU_PLAYER + PU_DESIRED_DIR);
        }
    } else {
        if (flags & 32u) {
            uint32_t state = ae_rd16(r, PU_PLAYER + PU_STATE);
            if (state == 54) ae_PlayerSetMode(r, h->act, 13);
            else if (!state) {
                ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y, (uint32_t)-60);
                ae_wr32(r, PU_PLAYER + PU_POSITION_Y, ae_rd32(r, PU_PLAYER + PU_POSITION_Y) - 1u);
                ae_PlayerSetState(r, h->act, 8);
            } else if (state == 9) {
                int32_t speed = ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y);
                int32_t magnitude = speed < 0 ? (int32_t)(0u - (uint32_t)speed) : speed;
                if (magnitude < 38) {
                    ae_wr32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y, (uint32_t)speed - 20u);
                    playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, PU_PLAYER + PU_DESIRED_DIR);
                    ae_PlayerSetState(r, h->act, 53);
                }
            }
        }
        for (uint32_t axis = 0; axis < 2; ++axis)
            ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + axis * 8u,
                (ae_rd32(r, PU_PLAYER + PU_VELOCITY + axis * 8u) << 12) +
                 ae_rd32(r, PU_PLAYER + PU_FRACTION + axis * 8u));
    }
}

static void playerupdate_water_air(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    ae_wr16(r, PU_PLAYER + PU_PREV_GROUND_TYPE, ae_rd16(r, PU_PLAYER + PU_GROUND_TYPE));
    uint32_t surface = (uint16_t)ae_GetSurfaceType(r, ae_rds32(r, PU_PLAYER + PU_POSITION),
        ae_rds32(r, PU_PLAYER + PU_POSITION_Y), ae_rds32(r, PU_PLAYER + PU_POSITION + PU_VECTOR_Z));
    ae_wr16(r, PU_PLAYER + PU_GROUND_TYPE, surface);
    if ((surface >> 12) != 1) return;
    uint32_t top = ae_rd16(r, PU_SURFACE_WATER);
    int32_t water = (int16_t)(top - ae_rd16(r, PU_SURFACE_DEPTH));
    ae_wr16(r, PU_PLAYER + PU_WATER_TOP, top);
    ae_wr16(r, PU_PLAYER + PU_WATER_Y, (uint32_t)water);
    if ((int32_t)(ae_rd32(r, PU_PLAYER + PU_POSITION_Y) - (uint32_t)water) < 256 &&
        ae_rds16(r, PU_PLAYER + PU_HEALTH) > 0) {
        int32_t air = ae_rds16(r, PU_AIR);
        if (air < 600) {
            air += 15;
            if (air > 600) { air = 600; ae_PlaySfx(r, h->sound, 65); }
            if (air < 450 && ae_rds16(r, PU_AIR_WARNING)) ae_PlaySfx(r, h->sound, 64);
        }
        ae_wr16(r, PU_AIR, (uint32_t)air);
        ae_wr16(r, PU_AIR_WARNING, 0);
    } else if (ae_rd16(r, PU_PLAYER + PU_STATE) - 70u < 2u) {
        int32_t air = ae_rds16(r, PU_AIR) - 1;
        uint32_t frame = ae_rd32(r, PU_FRAME_COUNTER);
        if ((air < 150 && frame % 10u == 0) ||
            (air >= 150 && air < 300 && frame % 25u == 0)) ae_PlaySfx(r, h->sound, 35);
        ae_wr16(r, PU_AIR, (uint32_t)air);
        ae_wr16(r, PU_AIR_WARNING, 1);
        if (air < 1) ae_PlayerDamage(r, 5);
    }
}

static void playerupdate_turn(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    const uint32_t facing = PU_SCRATCH + PU_TURN_FACING, target = PU_SCRATCH + PU_TURN_TARGET;
    playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, facing);
    uint32_t mode = ae_rd8(r, playerupdate_entry(r) + PU_ENTRY_TURN) & 15u;
    if (ae_rd8(r, PU_AIM_MODE) != 1) {
        switch (mode) {
        case 0:
            if (ae_rd16(r, PU_SCRATCH + PU_INPUT_CLASS)) {
                playerupdate_copy_vector(r, PU_PAD_LEFT_WORLD, facing);
                if (!ae_rds16(r, facing) && !ae_rds16(r, facing + 4u))
                    playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, facing);
            }
            break;
        case 1: case 2: case 4:
            if (ae_rd16(r, PU_SCRATCH + PU_INPUT_CLASS)) {
                playerupdate_copy_vector(r, PU_PAD_LEFT_WORLD, target);
                ae_TurnTowardsY(r, h->turn, target, facing, mode == 2 ? 341 : 113);
            }
            if (mode == 4) {
                uint32_t held = ae_rd32(r, PU_PAD_HELD);
                int32_t pitch_target = 0;
                if (held & 512u) pitch_target = 768;
                else if (ae_rd16(r, PU_PAD_CLASS)) pitch_target = (int32_t)ae_rd16(r, PU_PAD_LEFT_MAG) * 6 - 768;
                else if (held & 10u) pitch_target = -768;
                uint32_t pitch = ae_rd32(r, PU_PLAYER + PU_PITCH);
                int32_t difference = (int32_t)(pitch - (uint32_t)pitch_target);
                pitch = difference > 32 ? pitch - 32u : difference < -32 ? pitch + 32u : (uint32_t)pitch_target;
                ae_wr32(r, PU_PLAYER + PU_PITCH, pitch);
                ae_wr16(r, target, 0);
                ae_wr16(r, target + 2u, (uint32_t)ae_sdk_rsin(r, (int32_t)pitch));
                ae_wr16(r, target + 4u, 0u - (uint32_t)ae_sdk_rcos(r, (int32_t)pitch));
                ae_sdk_MatrixFromDirXZ(r, facing, TURN_MATRIX);
                ae_sdk_ApplyMatrixSV(r, TURN_MATRIX, target, PU_PLAYER + PU_SAVED_FACING);
            }
            break;
        case 5:
            if (!(ae_rd32(r, PU_PAD_HELD) & 512u) && ae_rd16(r, PU_PAD_GESTURE)) {
                ae_wr16(r, target + 2u, 0);
                ae_wr16(r, target, 0u - ae_rd16(r, PU_PAD_RIGHT_WORLD));
                ae_wr16(r, target + 4u, 0u - ae_rd16(r, PU_PAD_RIGHT_WORLD + 4u));
                ae_TurnTowardsY(r, h->turn, target, facing, 56);
            }
            break;
        case 6:
            if (ae_rd16(r, PU_SCRATCH + PU_INPUT_GESTURE)) {
                playerupdate_copy_vector(r, PU_PAD_RIGHT_WORLD, target);
                ae_TurnTowardsY(r, h->turn, target, facing, 341);
            }
            break;
        case 7:
            ae_TurnTowardsY(r, h->turn, PU_PLAYER + PU_DESIRED_DIR, PU_PLAYER + PU_FACING, 341);
            playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, facing);
            break;
        case 8: case 9:
            ae_sdk_ApplyMatrixSV(r, mode == 8 ? PU_TURN_LEFT_MATRIX : PU_TURN_RIGHT_MATRIX,
                                PU_PLAYER + PU_FACING, facing);
            break;
        case 10:
            for (uint32_t axis = 0; axis < 2; ++axis) {
                int32_t direction = ae_rds16(r, facing + axis * 4u);
                ae_wr16(r, facing + axis * 4u,
                    (uint32_t)(direction >= 2048 ? 4096 : direction < -2047 ? -4096 : 0));
            }
            ae_sdk_VectorNormalSS(r, facing, facing);
            break;
        default: break;
        }
    }
    ae_sdk_VectorNormalSS(r, facing, PU_PLAYER + PU_FACING);
    ae_PlayerApplyGadgetStance(r, h->act);
}

static void playerupdate_advance_frame(AeRam *r)
{
    ae_wr16(r, PU_PLAYER + PU_ANIM_FRAME, ae_rd16(r, PU_PLAYER + PU_ANIM_FRAME) + 1u);
}

static void playerupdate_move(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    uint32_t mode = ae_rd8(r, playerupdate_entry(r) + PU_ENTRY_MOVE);
    if (ae_rd8(r, PU_AIM_MODE) == 1) mode = 0;
    int32_t magnitude = (int32_t)ae_rd16(r, PU_SCRATCH + PU_INPUT_LEFT_MAG);
    switch (mode) {
    case 0: case 8: case 9:
        if (mode == 8) ae_wr8(r, PU_PLAYER + PU_STEADY_MOVE, 0);
        if (mode == 9) ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Y, 0);
        ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY, 0);
        ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Z, 0);
        playerupdate_advance_frame(r);
        break;
    case 1: case 14:
        for (uint32_t axis = 0; axis < 3; ++axis) {
            if (axis == 1 && mode == 1) continue;
            uint32_t source = mode == 14 && axis == 1 ? PU_MOVE_VELOCITY : PU_TARGET_VELOCITY;
            int32_t product = (int32_t)(ae_rd32(r, PU_PLAYER + source + axis * 4u) *
                                       (mode == 1 ? 4000u : 3800u));
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 4u, (uint32_t)(product / 4096));
        }
        if (mode == 1) ae_wr8(r, PU_PLAYER + PU_STEADY_MOVE, 0);
        playerupdate_advance_frame(r);
        break;
    case 2:
        playerupdate_advance_frame(r);
        for (uint32_t axis = 0; axis < 2; ++axis)
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u,
                (uint32_t)(((ae_rds16(r, PU_PLAYER + PU_FACING + axis * 4u) / 2) * magnitude) >> 2));
        break;
    case 3: case 11:
        for (uint32_t axis = 0; axis < 2; ++axis) {
            int32_t speed = 0;
            if (ae_rd16(r, PU_PAD_CLASS) && (mode == 11 || ae_rds16(r, PU_PAD_LEFT_CAM) > 0))
                speed = ae_rds16(r, PU_PLAYER + PU_FACING + axis * 4u) * magnitude / 8;
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u, (uint32_t)speed);
        }
        if (mode == 11) playerupdate_advance_frame(r);
        else {
            uint32_t fraction = (ae_rd16(r, PU_PLAYER + PU_ANIM_FRACTION) +
                                 ae_rd16(r, PU_PAD_LEFT_MAG) * 32u) & 65535u;
            ae_wr16(r, PU_PLAYER + PU_ANIM_FRACTION, fraction);
            if (fraction > 4095) {
                ae_wr16(r, PU_PLAYER + PU_ANIM_FRACTION, fraction - 4096u);
                playerupdate_advance_frame(r);
            }
        }
        break;
    case 4: case 7:
        playerupdate_advance_frame(r);
        for (uint32_t axis = 0; axis < 2; ++axis) {
            uint32_t direction = mode == 4 ? PU_PLAYER + PU_FACING : PU_PAD_LEFT_WORLD;
            int32_t input = ae_rds16(r, direction + axis * 4u) * magnitude;
            int32_t speed = (int32_t)((uint32_t)(input >> 7) * ae_rd32(r, PU_PARAM_SPEED));
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u, (uint32_t)(speed >> 5));
        }
        break;
    case 5:
        playerupdate_advance_frame(r);
        for (uint32_t axis = 0; axis < 2; ++axis) {
            int32_t speed = (int32_t)((uint32_t)(magnitude * ae_rds16(r, PU_PAD_LEFT_WORLD + axis * 4u)) *
                                    ae_rd32(r, PU_PARAM_MOVE_SCALE));
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u, (uint32_t)(speed >> 10));
        }
        break;
    case 6: {
        ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY, 0);
        ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Z, 0);
        ae_wr8(r, PU_PLAYER + PU_STEADY_MOVE, 0);
        ae_wr16(r, PU_PLAYER + PU_ANIM_BLEND, 65535);
        uint32_t fraction = (ae_rd16(r, PU_PLAYER + PU_ANIM_FRACTION) +
                             ae_rd16(r, PU_PLAYER + PU_ANIM_SPEED)) & 65535u;
        ae_wr16(r, PU_PLAYER + PU_ANIM_FRAME, ae_rd16(r, PU_PLAYER + PU_ANIM_FRAME) + (fraction >> 12));
        ae_wr16(r, PU_PLAYER + PU_ANIM_FRACTION, fraction & 4095u);
        break;
    }
    case 10:
        for (uint32_t axis = 0; axis < 2; ++axis)
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u,
                    (uint32_t)ae_rds16(r, PU_PAD_LEFT_WORLD + axis * 4u) << 3);
        playerupdate_advance_frame(r);
        break;
    case 12: {
        int diving = ae_rd16(r, PU_PLAYER + PU_STATE) == 70;
        int fast = !diving && (ae_rd32(r, PU_SCRATCH + PU_INPUT_HELD) & 10u);
        int start_boost = (diving || fast) && (ae_rd32(r, PU_SCRATCH + PU_INPUT_CHANGED) & 10u) &&
                          !ae_rds16(r, PU_SWIM_BOOST);
        if (diving) {
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY, 0);
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Y, (uint32_t)-40960);
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Z, 0);
        } else {
            for (uint32_t axis = 0; axis < 3; ++axis)
                ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 4u,
                    (uint32_t)(ae_rds16(r, PU_PLAYER + PU_SAVED_FACING + axis * 2u) * (fast ? 32 : 21)));
            if (fast) playerupdate_advance_frame(r);
        }
        if (start_boost) ae_wr16(r, PU_SWIM_BOOST, 30);
        else if (fast) h->player2->PadRumble(h->player2->user, r, 1, 74, 2);
        int32_t boost = ae_rds16(r, PU_SWIM_BOOST);
        if (boost > 0) {
            h->player2->PadRumble(h->player2->user, r, 1, boost * 8, 1);
            if (boost == 28) ae_PlaySfx(r, h->sound, 62);
            if (boost > 10) ae_PlayerSpawnDustBehind(r, h->player2);
            ae_wr16(r, PU_SWIM_BOOST, (uint32_t)(boost - 1));
        }
        ae_PlayerAccelXYZ(r, 4096);
        playerupdate_advance_frame(r);
        break;
    }
    case 15: case 16:
        for (uint32_t axis = 0; axis < 2; ++axis) {
            int32_t speed = (int32_t)((uint32_t)ae_rds16(r, PU_PLAYER + PU_FACING + axis * 4u) *
                                     ae_rd32(r, PU_PARAM_SPEED));
            ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u, (uint32_t)(speed >> (mode == 15 ? 3 : 4)));
        }
        if (mode == 15) ae_PlayerAccelXZ(r, 24576);
        else if (ae_rd16(r, PU_HOOP_START)) {
            ae_wr16(r, PU_HOOP_START, 0);
            ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY, ae_rd32(r, PU_PLAYER + PU_MOVE_VELOCITY));
            ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + PU_VECTOR_Z,
                    ae_rd32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Z));
            ae_PlaySfx(r, h->sound, 39);
        }
        playerupdate_advance_frame(r);
        break;
    default: break;
    }
}

static void playerupdate_add_slope(AeRam *r)
{
    for (uint32_t axis = 0; axis < 2; ++axis)
        ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u,
            ae_rd32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u) +
            (uint32_t)(ae_rds16(r, PU_PLAYER + PU_SLOPE_X + axis * 2u) * 56));
}

static void playerupdate_wall_velocity(AeRam *r)
{
    if (ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y) < 0)
        for (uint32_t axis = 0; axis < 2; ++axis)
            ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + axis * 8u,
                (uint32_t)(ae_rds32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u) >> 1));
}

/* Returns zero for the lethal ground hazard, which skips transitions/actions.
 * The normal local is intentionally untouched on the death path. */
static int playerupdate_ground(AeRam *r, const AePlayerUpdateFrameHooks *h, uint32_t locals)
{
    uint32_t normal = locals + PU_LOCAL_NORMAL;
    if (ae_rds16(r, PU_PLAYER + PU_HEALTH) <= 0) {
        if ((ae_rd32(r, PU_PLAYER + PU_STATE_FLAGS) & 240u) != 224u) {
            ae_PlayerSetState(r, h->act, 34);
            ae_PlayerEnterSwimIfInWater(r);
            h->XaPlay(h->user, r, (ae_rd32(r, PU_FRAME_COUNTER) & 1u) ? 13 : 15);
        }
        return 1;
    }
    ae_wr16(r, normal + 2u, 512);
    ae_wr16(r, normal, 0u - ae_rd16(r, PU_PLAYER + PU_SLOPE_X));
    ae_wr16(r, normal + 4u, 0u - ae_rd16(r, PU_PLAYER + PU_SLOPE_Z));
    ae_sdk_VectorNormalSS(r, normal, normal);
    if (ae_rds32(r, PU_PLAYER + PU_POSITION_Y) < ae_rds32(r, PU_PLAYER + PU_GROUND_Y)) {
        ae_wr16(r, PU_PLAYER + PU_SURFACE_KIND, 65535);
        ae_PlayerAccelXZ(r, ae_rd16(r, PU_PLAYER + PU_STATE) == 43 ? 12288 : 4096);
    } else {
        uint32_t surface = (ae_rd16(r, PU_SURFACE_QUERY) >> 1) & 7u;
        for (uint32_t axis = 0; axis < 2; ++axis) {
            int32_t length = ae_player_vec_length3(ae_rds16(r, PU_PLAYER + PU_SLOPE_X + axis * 2u), 512, 0);
            if (length) {
                int32_t velocity = (int32_t)(ae_rd32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u) << 9);
                if (length == -1 && velocity == INT32_MIN) {
                    ae_ram_fail(r, "PlayerUpdate: slope division overflow");
                    return 0;
                }
                ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 8u, (uint32_t)(velocity / length));
            }
        }
        if (ae_rd16(r, PU_SURFACE_QUERY) & 1u) {
            if (surface == 0) {
                ae_PlayerDamage(r, 5);
                ae_PlayerSetState(r, h->act, 45);
                /* Original leaves a1 from PlayerSetState: the checker must
                 * supply its observable fallback argument, not invent zero. */
                ae_PlayPlayerVoice(r, h->player2, 10, h->PlayerVoiceFallback(h->user, r));
                if (ae_rds8(r, PU_PLAYER + PU_GADGET) == 4)
                    ae_PlayerSelectGadget(r, h->player2, h->act, h->spawn, h->body, h->sound, 4, 4);
                return 0;
            }
            if (surface < 4 && ae_rd16(r, PU_PLAYER + PU_STATE) != 77) {
                ae_PlayerSetState(r, h->act, 77);
                ae_PlayerSetTintEffect(r, (int32_t)surface);
                h->player2->PadRumble(h->player2->user, r, 1, 150, 12);
                ae_wr8(r, PU_PLAYER + PU_HAZARD_TIMER, 15);
                ae_PlayerDamage(r, 1);
                if (ae_rd8(r, PU_AIM_ACTIVE) == 1) ae_AimRequestClear(r);
                if (ae_rd8(r, PU_AIM_MODE) == 1) ae_AimModeLeave(r, h->player_spawn);
                ae_PlaySfx(r, h->sound, surface == 1 ? 52 : surface == 2 ? 51 : 53);
            }
        } else if (surface >= 6) {
            ae_PlayerAccelXZ(r, 0);
            if ((ae_rd32(r, PU_PLAYER + PU_STATE_FLAGS) & 240u) != 160u)
                ae_PlayerOnWallBump(r, h->player2, normal);
            playerupdate_wall_velocity(r);
            playerupdate_add_slope(r);
            ae_PlayerAccelXZ(r, 16384);
        } else {
            uint32_t tuning = PU_SURFACES + surface * 16u;
            int32_t limit = ae_rds16(r, tuning + PU_SURFACE_SLIP_LIMIT);
            int32_t slope_x = ae_rds16(r, PU_PLAYER + PU_SLOPE_X);
            int32_t slope_z = ae_rds16(r, PU_PLAYER + PU_SLOPE_Z);
            if (!ae_rd8(r, PU_PLAYER + PU_STEADY_MOVE) ||
                slope_x > limit || slope_x < -limit || slope_z > limit || slope_z < -limit) {
                ae_PlayerAccelXZ(r, ae_rds32(r, tuning + PU_SURFACE_ACCEL_SLIP));
                ae_wr8(r, PU_PLAYER + PU_STEADY_MOVE, 0);
                playerupdate_add_slope(r);
            }
            uint32_t flags = ae_rd32(r, PU_PLAYER + PU_STATE_FLAGS) & 240u;
            if (((ae_rd32(r, PU_PLAYER + PU_ATTACH_FLAGS) & 0x04000000u) ||
                 ae_rds16(r, PU_PLAYER + PU_SURFACE_KIND) == -1) && flags != 160u && flags != 192u) {
                limit = ae_rds16(r, tuning + PU_SURFACE_BUMP_LIMIT);
                if ((slope_x < 0 ? -slope_x : slope_x) > limit ||
                    (slope_z < 0 ? -slope_z : slope_z) > limit)
                    ae_PlayerOnWallBump(r, h->player2, normal);
            }
            if (!ae_rd8(r, PU_PLAYER + PU_STEADY_MOVE)) {
                ae_PlayerSpawnDust(r, h->player2);
                if ((ae_rd32(r, PU_PLAYER + PU_STATE_FLAGS) & 240u) == 160u)
                    playerupdate_wall_velocity(r);
                if (!ae_PlayerAccelXZ(r, ae_rds32(r, tuning + PU_SURFACE_ACCEL_SLIP)))
                    ae_wr8(r, PU_PLAYER + PU_STEADY_MOVE, 1);
            } else if (ae_PlayerAccelXZ(r, ae_rds32(r, tuning))) {
                ae_wr8(r, PU_PLAYER + PU_STEADY_MOVE, 0);
                if (surface == 5 && (ae_rd32(r, PU_PLAYER + PU_STATE_FLAGS) & 240u) == 48u &&
                    (ae_rd32(r, PU_FRAME_COUNTER) & 32u)) ae_PlaySfx(r, h->sound, 131);
            }
        }
        if (ae_rd16(r, PU_PLAYER + PU_STATE) == 47) {
            int32_t x = ae_rds32(r, PU_PLAYER + PU_VELOCITY);
            int32_t z = ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Z);
            uint32_t sum = (x < 0 ? 0u - (uint32_t)x : (uint32_t)x) +
                           (z < 0 ? 0u - (uint32_t)z : (uint32_t)z);
            if ((int32_t)sum / 2 > 10) {
                h->memset(h->user, r, locals + PU_LOCAL_POSITION2, 0, 8);
                for (uint32_t axis = 0; axis < 3; ++axis)
                    ae_wr16(r, locals + PU_LOCAL_POSITION2 + axis * 2u,
                            ae_rd16(r, PU_PLAYER + PU_POSITION + axis * 4u));
                playerupdate_copy_vector(r, locals + PU_LOCAL_POSITION2, locals + PU_LOCAL_POSITION1);
                ae_SpawnParticle(r, h->spawn, 4, locals + PU_LOCAL_POSITION1);
            }
        }
        if (ae_rd16(r, PU_PLAYER + PU_STATE) == 72 && ae_rd16(r, PU_PLAYER + PU_ANIM_FRAME) % 5u == 0) {
            uint32_t position = locals + PU_LOCAL_POSITION3;
            h->memset(h->user, r, position, 0, 8);
            int32_t angle = (int32_t)(ae_rd32(r, PU_FRAME_COUNTER) * 360u);
            ae_wr16(r, position, ae_rd16(r, PU_PLAYER + PU_POSITION) + (uint32_t)(ae_sdk_rsin(r, angle) / 32));
            angle = (int32_t)(ae_rd32(r, PU_FRAME_COUNTER) * 360u);
            ae_wr16(r, position + 2u, ae_rd16(r, PU_PLAYER + PU_POSITION_Y) -
                     (uint32_t)(ae_sdk_rsin(r, angle) / 32) - 128u);
            angle = (int32_t)(ae_rd32(r, PU_FRAME_COUNTER) * 360u);
            ae_wr16(r, position + 4u, ae_rd16(r, PU_PLAYER + PU_POSITION + PU_VECTOR_Z) +
                     (uint32_t)(ae_sdk_rcos(r, angle) / 32));
            playerupdate_copy_vector(r, position, locals + PU_LOCAL_POSITION2);
            ae_SpawnParticle(r, h->spawn, 259, locals + PU_LOCAL_POSITION2);
        }
        ae_wr16(r, PU_PLAYER + PU_SURFACE_KIND, surface);
    }
    if (ae_PlayerCheckWaterEntry(r, h->player2, h->spawn)) {
        ae_PlayerSetState(r, h->act, 70);
        ae_PlayerResetAir(r);
        int32_t gadget = ae_rds8(r, PU_PLAYER + PU_GADGET);
        if (gadget == 3) ae_wr32(r, PU_GADGET3_AIM, 0);
        else if (gadget == 4) {
            ae_wr16(r, PU_HOOP_STATE, 0);
            for (uint32_t i = 0; i < 4; ++i) ae_wr32(r, PU_HOOP_AUX + i * 4u, 0);
        }
    }
    return 1;
}

static void playerupdate_tilt(AeRam *r, uint32_t normal)
{
    uint32_t mode = ae_rd8(r, playerupdate_entry(r) + PU_ENTRY_TURN) >> 4;
    if (mode == 0) {
        ae_wr16(r, PU_PLAYER + PU_UP, 0);
        ae_wr16(r, PU_PLAYER + PU_UP + 2u, 4096);
        ae_wr16(r, PU_PLAYER + PU_UP + 4u, 0);
    } else if (mode >= 1 && mode <= 3) {
        for (uint32_t axis = 0; axis < 2; ++axis) {
            int32_t slope = ae_rds16(r, normal + axis * 4u);
            if (mode < 3) slope /= mode == 1 ? 4 : 2;
            ae_wr16(r, normal + axis * 4u,
                (uint32_t)((slope + ae_rds16(r, PU_PLAYER + PU_UP + axis * 4u)) >> 1));
        }
        ae_sdk_VectorNormalSS(r, normal, PU_PLAYER + PU_UP);
    } else if (mode == 4) {
        ae_wr16(r, normal + 2u, 4096);
        ae_wr16(r, PU_PLAYER + PU_UP + 2u, 4096);
        for (uint32_t axis = 0; axis < 2; ++axis) {
            int32_t slope = ae_rd16(r, PU_PLAYER + PU_STATE) == 70 ? 0 :
                (-ae_rds16(r, PU_PAD_RIGHT_WORLD + axis * 4u) * (int32_t)ae_rd16(r, PU_PAD_RIGHT_MAG)) >> 8;
            slope = (int16_t)slope;
            ae_wr16(r, normal + axis * 4u, (uint32_t)slope);
            int32_t up = ae_rds16(r, PU_PLAYER + PU_UP + axis * 4u);
            int32_t difference = slope - up;
            ae_wr16(r, PU_PLAYER + PU_UP + axis * 4u,
                     (uint32_t)(difference > 128 ? up + 128 : difference < -128 ? up - 128 : slope));
        }
    }
    playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, PU_PLAYER + PU_PREV_FACING);
}

static void playerupdate_gadget_effects(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    if (ae_rds8(r, PU_PLAYER + PU_GADGET) == 1 && ae_rd16(r, PU_PLAYER + PU_STATE) == 129) {
        ae_wr16(r, PU_PLAYER + PU_ANIM_BLEND, 65534);
        uint32_t table = ae_rd32(r, PU_SCRATCH + PU_INPUT_STATE_TABLE);
        uint32_t next = ae_rd16(r, table + 2u * PU_STATE_ENTRY_BYTES +
                               (ae_rds16(r, PU_PAD_RIGHT_CAM + 2u) < 0 ? PU_ENTRY_FIRST : PU_ENTRY_LAST));
        if (next != ae_rd16(r, PU_PLAYER + PU_NEXT_FRAME)) {
            ae_wr16(r, PU_PLAYER + PU_NEXT_FRAME, next);
            ae_wr16(r, PU_PLAYER + PU_ANIM_FRACTION, 0);
        }
        int32_t input = ae_rds16(r, PU_PAD_RIGHT_CAM + 2u) * 4;
        int32_t amount = input < 0 ? -input : input;
        if (amount > 4096) amount = 4096;
        int32_t previous = (int32_t)ae_rd16(r, PU_PLAYER + PU_ANIM_FRACTION);
        int32_t delta = amount - previous;
        ae_wr16(r, PU_PLAYER + PU_ANIM_FRACTION,
                 (uint32_t)(delta > 128 ? previous + 128 : delta < -128 ? previous - 128 : amount));
        if (ae_rd16(r, PU_LOCK) != 11 && ae_rds16(r, PU_PLAYER + PU_HEALTH) > 0 &&
            (ae_rds32(r, PU_PLAYER + PU_POSITION_Y) < ae_rds32(r, PU_PLAYER + PU_GROUND_Y) ||
             ae_rds16(r, PU_PAD_TRAVEL_RIGHT) > 40)) {
            uint32_t body = ae_CollisionBodyAlloc(r);
            if (body) {
                playerupdate_copy_vector(r, PU_PLAYER + PU_FACING, body + PU_BODY_DIRECTION);
                for (uint32_t axis = 0; axis < 3; ++axis)
                    ae_wr32(r, body + axis * 4u, (uint32_t)ae_rds16(r, PU_NET_POSITION + axis * 2u));
                ae_wr16(r, body + PU_BODY_RADIUS, 128);
                ae_CollisionBodyRegister(r, h->body, 8, body, PU_PLAYER);
                ae_wr16(r, body + PU_BODY_HEIGHT, 256);
                ae_wr16(r, body + PU_BODY_FLAGS, 0);
            }
        }
    }
    if (ae_rds8(r, PU_PLAYER + PU_GADGET) == 0) {
        uint32_t body = ae_rd32(r, PU_CLUB_BODY1);
        if (body && ae_rd32(r, body + PU_BODY_CONTACT)) h->player2->PadRumble(h->player2->user, r, 0, 1, 1);
        body = ae_rd32(r, PU_CLUB_BODY2);
        if (body && ae_rd32(r, body + PU_BODY_CONTACT)) h->player2->PadRumble(h->player2->user, r, 0, 1, 1);
        ae_wr32(r, PU_CLUB_BODY1, 0);
        ae_wr32(r, PU_CLUB_BODY2, 0);
    }
    if (ae_rds8(r, PU_PLAYER + PU_GADGET) == 6 && ae_rd16(r, PU_PLAYER + PU_STATE) == 132 &&
        ae_rds32(r, PU_PLAYER + PU_VELOCITY + PU_VECTOR_Y) < -7) {
        uint32_t body = ae_CollisionBodyAlloc(r);
        if (body) {
            ae_wr16(r, body + PU_BODY_DIRECTION, 0);
            ae_wr16(r, body + PU_BODY_DIRECTION + 2u, (uint32_t)-4096);
            ae_wr16(r, body + PU_BODY_DIRECTION + 4u, 0);
            ae_wr32(r, body, ae_rd32(r, PU_PLAYER + PU_POSITION));
            ae_wr32(r, body + PU_VECTOR_Y, ae_rd32(r, PU_PLAYER + PU_POSITION_Y) - 256u);
            ae_wr32(r, body + PU_VECTOR_Z, ae_rd32(r, PU_PLAYER + PU_POSITION + PU_VECTOR_Z));
            ae_wr16(r, body + PU_BODY_RADIUS, 128);
            ae_CollisionBodyRegister(r, h->body, 6, body, PU_PLAYER);
            ae_wr16(r, body + PU_BODY_HEIGHT, 18);
            ae_wr16(r, body + PU_BODY_FLAGS, 513);
        }
    }
    if (ae_rds8(r, PU_PLAYER + PU_GADGET) == 7 && ae_rd16(r, PU_LOCK) != 11 &&
        (ae_rd32(r, PU_PAD_HELD) & ae_rd32(r, PU_PAD_CHANGED) & 1024u))
        ae_PlayerSelectGadget(r, h->player2, h->act, h->spawn, h->body, h->sound, 7, 7);
}

static void playerupdate_finish_motion(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    for (uint32_t axis = 0; axis < 3; ++axis) {
        int32_t velocity = (int32_t)(ae_rd32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 4u) +
                                   ae_rd32(r, PU_PLAYER + PU_FRACTION + axis * 4u));
        int32_t limit = axis == 1 ? 524288 : 262144;
        if (velocity < -limit) velocity = -limit;
        if (velocity > limit) velocity = limit;
        ae_wr32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 4u, (uint32_t)velocity);
    }
    ae_wr16(r, PU_PLAYER + PU_FALL_FRAMES,
        ae_rds32(r, PU_PLAYER + PU_MOVE_VELOCITY + PU_VECTOR_Y) == 524288 ?
        ae_rd16(r, PU_PLAYER + PU_FALL_FRAMES) + 1u : 0);
    ae_wr32(r, PU_PLAYER + PU_ATTACH_FLAGS, 0);
    h->PlayerAnimateAndDraw(h->user, r);
    for (uint32_t axis = 0; axis < 3; ++axis) {
        int32_t move = ae_rds32(r, PU_PLAYER + PU_MOVE_VELOCITY + axis * 4u);
        int32_t velocity = move / 4096;
        ae_wr32(r, PU_PLAYER + PU_VELOCITY + axis * 4u, (uint32_t)velocity);
        ae_wr32(r, PU_PLAYER + PU_FRACTION + axis * 4u,
                (uint32_t)move - ((uint32_t)velocity << 12));
        ae_wr32(r, PU_PLAYER + PU_TARGET_VELOCITY + axis * 4u, (uint32_t)velocity);
        ae_wr16(r, PU_PLAYER + PU_LAST_POSITION + axis * 2u,
                ae_rd16(r, PU_PLAYER + PU_POSITION + axis * 4u));
    }
    uint32_t set = PU_STATE_SETS +
        (uint32_t)(ae_rds8(r, PU_PLAYER + PU_GADGET) + 1) * PU_STATE_SET_SIZE;
    uint32_t table = ae_rd32(r, (ae_rd16(r, PU_PLAYER + PU_STATE) < 128 ? PU_STATE_SETS : set) + PU_STATE_SET_STATES);
    ae_wr32(r, PU_SCRATCH + PU_INPUT_STATE_TABLE, table);
    ae_wr32(r, PU_SCRATCH + PU_INPUT_EVENTS, ae_rd32(r, set + PU_STATE_SET_EVENTS));
    ae_PlayerRunAnimEvents(r, h->body, h->anim, table, ae_rd32(r, PU_STATE_SETS + PU_STATE_SET_EVENTS));
    if (ae_rds8(r, PU_PLAYER + PU_GADGET) != 9)
        ae_PlayerRunAnimEvents(r, h->body, h->anim, ae_rd32(r, PU_SCRATCH + PU_INPUT_STATE_TABLE),
                             ae_rd32(r, PU_SCRATCH + PU_INPUT_EVENTS));
    uint32_t blend = ae_rd16(r, PU_PLAYER + PU_ANIM_BLEND);
    if (blend) ae_wr16(r, PU_PLAYER + PU_ANIM_BLEND, blend >= 65534 ? 0 : blend - 1u);
    if (ae_rds16(r, PU_PLAYER + PU_SQUASH_TIMER) > 0)
        ae_wr16(r, PU_PLAYER + PU_SQUASH_TIMER, ae_rd16(r, PU_PLAYER + PU_SQUASH_TIMER) - 1u);
    if (ae_rds16(r, PU_HURT_BLINK) > 0) ae_wr16(r, PU_HURT_BLINK, ae_rd16(r, PU_HURT_BLINK) - 1u);
    if (ae_rd16(r, PU_MODE) == 10) {
        uint32_t timer = ae_rd16(r, PU_PLAYER + PU_IDLE_TIMER);
        if (ae_rd32(r, PU_PAD_CLASS) || (ae_rd32(r, PU_PAD_HELD) & 4095u) || !timer)
            ae_PlayerSetMode(r, h->act, 0);
        else ae_wr16(r, PU_PLAYER + PU_IDLE_TIMER, timer - 1u);
    }
}

void ae_PlayerUpdate(AeRam *r, const AePlayerUpdateFrameHooks *h)
{
    if (!h || !h->player2 || !h->player2->PadRumble ||
        !h->PlayerUpdateLocalFrame || !h->PlayerCheckGadgetSelect ||
        !h->PlayerAnimateAndDraw || !h->XaPlay || !h->CdMgrQueueCommand ||
        !h->CdIntToPos || !h->CdPosToInt || !h->memset || !h->PlayerVoiceFallback ||
        !h->PlayerModeCallback || !h->PlayerAction || !h->FUN_80142808) {
        ae_ram_fail(r, "PlayerUpdate: missing service");
        return;
    }
    uint32_t locals = h->PlayerUpdateLocalFrame(h->user, r);
    playerupdate_idle(r, h, locals);
    playerupdate_copy_input(r);
    playerupdate_select_tables(r);
    int32_t ground = ae_GetGroundHeightEx(r, h->world, ae_rds32(r, PU_PLAYER + PU_POSITION),
        ae_rds32(r, PU_PLAYER + PU_POSITION_Y), ae_rds32(r, PU_PLAYER + PU_POSITION + PU_VECTOR_Z),
        PU_PLAYER + PU_SLOPE_X);
    ae_wr32(r, PU_PLAYER + PU_GROUND_Y, (uint32_t)(int32_t)(int16_t)ground);
    ae_wr16(r, PU_PLAYER + PU_HAS_GROUND, (ae_rd16(r, PU_SURFACE_QUERY) & 15u) != 1u);
    playerupdate_attachment(r, h, locals);
    if (ae_ram_failed(r)) return;
    switch (ae_rd16(r, PU_MODE)) {
    case 1: h->PlayerModeCallback(h->user, r, ae_rd32(r, PU_MODE_CALLBACK1)); return;
    case 2: h->FUN_80142808(h->user, r); return;
    case 3: h->PlayerModeCallback(h->user, r, ae_rd32(r, PU_MODE_CALLBACK3)); return;
    case 8: ae_MonkeyNetCatchUpdate(r, h->player2, h->act); return;
    case 12: ae_PlayerDrawSquashedWithFx(r, h->player2, h->spawn); return;
    case 13: ae_PlayerDrawSquashedFlattened(r, h->player2, h->spawn); return;
    case 5: h->PlayerCheckGadgetSelect(h->user, r); ae_PlayerUpdateClimb(r, h->player2, h->act); break;
    case 6: h->PlayerCheckGadgetSelect(h->user, r); ae_PlayerUpdateRope(r, h->player2, h->act); break;
    case 7: h->PlayerCheckGadgetSelect(h->user, r); ae_PlayerUpdateHeldByObject(r, h->player2, h->act); break;
    case 10: ae_PlayerUpdateIdleAnim(r, h->player2, h->act); break;
    default:
        playerupdate_restore_velocity(r, h);
        playerupdate_water_air(r, h);
        playerupdate_prepare_vertical_move(r);
        if (h->PlayerCheckGadgetSelect(h->user, r)) {
            playerupdate_select_tables(r);
            if (ae_rd16(r, PU_PLAYER + PU_STATE) >= 128)
                ae_PlayerSetState(r, h->act,
                    ae_rds32(r, PU_PLAYER + PU_POSITION_Y) < ae_rds32(r, PU_PLAYER + PU_GROUND_Y) ? 36 : 0);
        }
        playerupdate_turn(r, h);
        playerupdate_move(r, h);
        if (playerupdate_ground(r, h, locals)) {
            ae_PlayerRunTransitions(r, h->player2, h->act, ae_rd32(r, PU_STATE_SETS + PU_STATE_SET_TRANSITIONS));
            if (ae_rds8(r, PU_PLAYER + PU_GADGET) != 9)
                ae_PlayerRunTransitions(r, h->player2, h->act, ae_rd32(r, PU_SCRATCH + PU_INPUT_TRANSITIONS));
            uint32_t action = ae_rd8(r, playerupdate_entry(r) + PU_ENTRY_ACTION);
            if (action) h->PlayerAction(h->user, r, ae_rd32(r, PU_ACTIONS + action * 4u));
        }
        break;
    }
    ae_PlayerOnContact(r, h->player2, h->act, h->world, h->spawn, h->sound);
    uint32_t flags = ae_rd16(r, PU_MODE) == 10 ? 18 : ae_rd8(r, playerupdate_entry(r) + PU_ENTRY_FLAGS);
    if (ae_rd16(r, PU_PLAYER + PU_FLAGS_OVERRIDE)) flags &= 240u;
    ae_wr32(r, PU_PLAYER + PU_STATE_FLAGS, flags);
    playerupdate_tilt(r, locals + PU_LOCAL_NORMAL);
    playerupdate_gadget_effects(r, h);
    playerupdate_finish_motion(r, h);
}
