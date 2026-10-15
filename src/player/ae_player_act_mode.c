#include "ae_player_act.h"

#include "native_player.h"

#include "../core/ae_sdk.h"

#include <stdint.h>
#include <string.h>

#define AE_PA_MODE_NORMAL 0
#define AE_PA_MODE_ACTOR 1
#define AE_PA_MODE_SCRIPTED 3
#define AE_PA_MODE_CLIMB 5
#define AE_PA_MODE_ROPE 6
#define AE_PA_MODE_SWIM 7
#define AE_PA_MODE_CATCH 9
#define AE_PA_MODE_IDLE_ANIMATION 10
#define AE_PA_MODE_ENTER_SPECIAL_A 11
#define AE_PA_MODE_SQUASHED 12
#define AE_PA_MODE_ENTER_SPECIAL_B 14

#define AE_PA_MODE_ACTOR_BURST_SOUND 0x79
#define AE_PA_MODE_ACTOR_ENTRY_SOUND 0xAC
#define AE_PA_MODE_SCRIPTED_ENTRY_SOUND 0xAF
#define AE_PA_MODE_ACTOR_REST_STATE 8u
#define AE_PA_MODE_IDLE_STATE 0x80u
#define AE_PA_MODE_SWIM_STATE 0x2Eu
#define AE_PA_MODE_CATCH_STATE 0x48u
#define AE_PA_MODE_CLIMB_STATE 0x38u
#define AE_PA_MODE_ROPE_STATE 0x3Du
#define AE_PA_MODE_FALL_STATE 0x24u
#define AE_PA_MODE_IDLE_EXIT_STATE 0u
#define AE_PA_MODE_IDLE_ACTOR_ACTION 10u
#define AE_PA_MODE_IDLE_FALL_SPEED -30
#define AE_PA_MODE_IDLE_FALL_DELTA -0x1E000
#define AE_PA_MODE_IDLE_FALL_HEIGHT 162
#define AE_PA_MODE_SQUASH_FRAMES 30
#define AE_PA_MODE_SQUASH_HEIGHT 0x200u
#define AE_PA_MODE_SPECIAL_GADGET 4
#define AE_PA_MODE_SPECIAL_GADGET_RESET 0u
#define AE_PA_MODE_GADGET_SELECT 9
#define AE_PA_MODE_SPECIAL_STAGE_EXIT 0x20u

#define AE_PA_MODE_PLAYER_GADGET_AFTERIMAGE 0xF3u
#define AE_PA_MODE_IDLE_BUFFER_POINTER 0x80u
#define AE_PA_MODE_IDLE_GADGET_FIELD 0x84u
#define AE_PA_MODE_IDLE_GADGET_STATE 0x46u
#define AE_PA_MODE_IDLE_ACTIVE_STATE 0x32u
#define AE_PA_MODE_IDLE_PLAYER_BLOB 0x80u
#define AE_PA_MODE_PLAYER_IDLE_GADGET 0xF2u
#define AE_PA_MODE_PLAYER_ANIM_SET_STEP_BACK 0x18u
#define AE_PA_MODE_PLAYER_ROPE_FIRST 0xC0u
#define AE_PA_MODE_PLAYER_ROPE_SECOND 0xC6u
#define AE_PA_MODE_PLAYER_ROPE_FRACTION 0xCCu
#define AE_PA_MODE_PLAYER_ROPE_LENGTH 0xCEu
#define AE_PA_MODE_PLAYER_ROPE_NORMAL 0xD0u
#define AE_PA_MODE_ROPE_TEMP 0x801FFEE8u
#define AE_PA_MODE_ACTOR_POSITION_Y 0x1Eu
#define AE_PA_MODE_ACTOR_BURST_POSITION 0x1Cu
#define AE_PA_MODE_ACTOR_MODEL_FLAGS 0x18u
#define AE_PA_MODE_AREA_EXIT_CODE 0x124u
#define AE_PA_MODE_AREA_ID 0x126u
#define AE_PA_MODE_AREA_ARGUMENT 0x12Au
#define AE_PA_MODE_AREA_HEADER_FLAGS 0x152u
#define AE_PA_MODE_AREA_SCRIPT_FLAG 0x100u
#define AE_PA_MODE_IDLE_REQUEST 0x150u
#define AE_PA_MODE_AREA_SLOT 0x800F4350u

static uint32_t pa_mode_add(uint32_t base, uint32_t offset)
{
    return base + offset;
}

static int32_t pa_mode_wrap(uint32_t bits)
{
    if (bits <= 0x7FFFFFFFu) return (int32_t)bits;
    return (int32_t)((int64_t)bits - 0x100000000LL);
}

static int32_t pa_mode_add_s32(int32_t a, int32_t b)
{
    return pa_mode_wrap((uint32_t)a + (uint32_t)b);
}

static int32_t pa_mode_sub_s32(int32_t a, int32_t b)
{
    return pa_mode_wrap((uint32_t)a - (uint32_t)b);
}

static int32_t pa_mode_s16(AeRam *r, uint32_t address)
{
    return ae_rds16(r, address);
}

static int pa_mode_play_sfx(AeRam *r, const AePlayerActHooks *hooks, int32_t sound_id)
{
    if (hooks == NULL || hooks->PlaySfx == NULL) {
        ae_ram_fail(r, "PlaySfx hook not provided");
        return 0;
    }
    hooks->PlaySfx(hooks->user, r, sound_id);
    return 1;
}

static int pa_mode_select_gadget(AeRam *r, const AePlayerActHooks *hooks,
                                 int32_t gadget, int32_t current_gadget)
{
    if (hooks == NULL || hooks->PlayerSelectGadget == NULL) {
        ae_ram_fail(r, "PlayerSelectGadget hook not provided");
        return 0;
    }
    hooks->PlayerSelectGadget(hooks->user, r, gadget, current_gadget);
    return 1;
}

static int pa_mode_aim_leave(AeRam *r, const AePlayerActHooks *hooks)
{
    if (hooks == NULL || hooks->AimModeLeave == NULL) {
        ae_ram_fail(r, "AimModeLeave hook not provided");
        return 0;
    }
    hooks->AimModeLeave(hooks->user, r);
    return 1;
}

static int pa_mode_idle_setup(AeRam *r, const AePlayerActHooks *hooks, uint32_t blob)
{
    if (hooks == NULL || hooks->PlayerIdleModelSetup == NULL) {
        ae_ram_fail(r, "PlayerIdleModelSetup hook not provided");
        return 0;
    }
    hooks->PlayerIdleModelSetup(hooks->user, r, blob);
    return 1;
}

static int pa_mode_heap_free(AeRam *r, const AePlayerActHooks *hooks, uint32_t pointer)
{
    if (hooks == NULL || hooks->HeapFree == NULL) {
        ae_ram_fail(r, "HeapFree hook not provided");
        return 0;
    }
    hooks->HeapFree(hooks->user, r, pointer);
    return 1;
}

static int pa_mode_stage_request(AeRam *r, const AePlayerActHooks *hooks,
                                 int32_t code, int32_t area, int32_t argument)
{
    if (hooks == NULL || hooks->StageRequest == NULL) {
        ae_ram_fail(r, "StageRequest hook not provided");
        return 0;
    }
    hooks->StageRequest(hooks->user, r, code, area, argument);
    return 1;
}

static int pa_mode_camera_script(AeRam *r, const AePlayerActHooks *hooks,
                                 int32_t index, int32_t argument, uint32_t table)
{
    if (hooks == NULL || hooks->CameraAreaScriptDispatch == NULL) {
        ae_ram_fail(r, "CameraAreaScriptDispatch hook not provided");
        return 0;
    }
    hooks->CameraAreaScriptDispatch(hooks->user, r, index, argument, table);
    return 1;
}

static int pa_mode_player_state(AeRam *r, const AePlayerActHooks *hooks, uint32_t state)
{
    ae_PlayerSetState(r, hooks, state);
    return !ae_ram_failed(r);
}

static void pa_mode_zero_move_velocity(AeRam *r)
{
    const uint32_t velocity = pa_mode_add(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_VELOCITY);
    ae_wr32(r, pa_mode_add(velocity, AE_PA_VECTOR_X), 0);
    ae_wr32(r, pa_mode_add(velocity, AE_PA_VECTOR_Y), 0);
    ae_wr32(r, pa_mode_add(velocity, AE_PA_VECTOR_Z), 0);
}

static void pa_mode_copy_bytes(AeRam *r, uint32_t destination, uint32_t source, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        ae_wr8(r, destination + i, ae_rd8(r, source + i));
    }
}

static void pa_mode_enter_actor(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t actor = ae_rd32(r, AE_PA_MODE_ACTOR_POINTER);
    const uint32_t actor_model = ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_MODEL));
    uint16_t flags;

    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_X),
            ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_POSITION + AE_PA_VECTOR_X)));
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y),
            ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_POSITION + AE_PA_VECTOR_Y)));
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Z),
            ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_POSITION + AE_PA_VECTOR_Z)));
    pa_mode_copy_bytes(r, pa_mode_add(player, AE_PA_PLAYER_FACING),
                       pa_mode_add(actor, AE_PA_ACTOR_FACING), AE_PA_MODE_ACTOR_ROTATION_SIZE);
    flags = (uint16_t)(ae_rd16(r, pa_mode_add(actor_model, AE_PA_MODEL_FLAGS)) | 0x8000u);
    ae_wr16(r, pa_mode_add(actor_model, AE_PA_MODEL_FLAGS), flags);
    pa_mode_zero_move_velocity(r);

    if (!pa_mode_camera_script(r, hooks, 0, 0, hooks == NULL ? 0 : hooks->incoming_a2)) return;
    if (ae_rd16(r, AE_PA_CURRENT_AREA) != 0x52 &&
        !pa_mode_play_sfx(r, hooks, AE_PA_MODE_ACTOR_ENTRY_SOUND)) return;
    if (!pa_mode_select_gadget(r, hooks, AE_PA_MODE_GADGET_SELECT,
            ae_rds8(r, pa_mode_add(player, AE_PA_PLAYER_CURRENT_GADGET)))) return;
    if (hooks == NULL || hooks->EffectGroundImpactAtPlayer == NULL) {
        ae_ram_fail(r, "EffectGroundImpactAtPlayer hook not provided");
        return;
    }
    hooks->EffectGroundImpactAtPlayer(hooks->user, r);
}

static void pa_mode_enter_scripted(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t actor = ae_rd32(r, AE_PA_MODE_ACTOR_POINTER);
    const uint32_t actor_model = ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_MODEL));
    uint16_t flags;

    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_X),
            ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_POSITION + AE_PA_VECTOR_X)));
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y),
            ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_POSITION + AE_PA_VECTOR_Y)));
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Z),
            ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_POSITION + AE_PA_VECTOR_Z)));
    pa_mode_copy_bytes(r, pa_mode_add(player, AE_PA_PLAYER_FACING),
                       pa_mode_add(actor, AE_PA_ACTOR_FACING), AE_PA_MODE_ACTOR_ROTATION_SIZE);
    flags = (uint16_t)(ae_rd16(r, pa_mode_add(actor_model, AE_PA_MODEL_FLAGS)) | 0x8000u);
    ae_wr16(r, pa_mode_add(actor_model, AE_PA_MODEL_FLAGS), flags);
    pa_mode_zero_move_velocity(r);
    if (!pa_mode_play_sfx(r, hooks, AE_PA_MODE_SCRIPTED_ENTRY_SOUND)) return;
    if (!pa_mode_select_gadget(r, hooks, AE_PA_MODE_GADGET_SELECT,
            ae_rds8(r, pa_mode_add(player, AE_PA_PLAYER_CURRENT_GADGET)))) return;
}

static void pa_mode_leave_actor(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t actor = ae_rd32(r, AE_PA_MODE_ACTOR_POINTER);
    const int32_t actor_state = ae_rds8(r, pa_mode_add(actor, AE_PA_ACTOR_FALL_STATE));
    const uint32_t actor_model = ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_MODEL));
    const uint32_t player = AE_PLAYER_ACT_PLAYER;

    if (actor_state <= 0) {
        uint32_t callback;
        if (!pa_mode_play_sfx(r, hooks, AE_PA_MODE_ACTOR_BURST_SOUND)) return;
        if (hooks == NULL || hooks->EffectBigBurst == NULL) {
            ae_ram_fail(r, "EffectBigBurst hook not provided");
            return;
        }
        hooks->EffectBigBurst(hooks->user, r,
                pa_mode_s16(r, pa_mode_add(actor, AE_PA_MODE_ACTOR_BURST_POSITION)),
                pa_mode_s16(r, pa_mode_add(actor, AE_PA_MODE_ACTOR_BURST_POSITION + AE_PA_SVECTOR_Y)),
                pa_mode_s16(r, pa_mode_add(actor, AE_PA_MODE_ACTOR_BURST_POSITION + AE_PA_SVECTOR_Z)));
        callback = ae_rd32(r, AE_PA_MODE_CALLBACK_POINTER);
        if (hooks->PlayerModeCallback == NULL) {
            ae_ram_fail(r, "PlayerModeCallback hook not provided");
            return;
        }
        hooks->PlayerModeCallback(hooks->user, r, callback, 1);
    } else {
        ae_wr16(r, pa_mode_add(actor_model, AE_PA_MODE_ACTOR_MODEL_FLAGS),
                ae_rd16(r, pa_mode_add(actor_model, AE_PA_MODE_ACTOR_MODEL_FLAGS)) & 0x7FFFu);
        ae_wr16(r, pa_mode_add(actor_model, AE_PA_VECTOR_X),
                ae_rd16(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_X)));
        ae_wr16(r, pa_mode_add(actor_model, AE_PA_SVECTOR_Y),
                ae_rd16(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y)));
        ae_wr16(r, pa_mode_add(actor_model, AE_PA_SVECTOR_Z),
                ae_rd16(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Z)));
        pa_mode_copy_bytes(r, pa_mode_add(actor, AE_PA_ACTOR_FACING),
                           pa_mode_add(player, AE_PA_PLAYER_FACING), AE_PA_MODE_ACTOR_TRANSFORM_SIZE);
    }

    ae_wr8(r, pa_mode_add(actor, AE_PA_ACTOR_MODE), AE_PA_MODE_IDLE_ACTOR_ACTION);
    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_ACTOR_REST_STATE)) return;

    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_Y),
            (uint32_t)AE_PA_MODE_IDLE_FALL_SPEED);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_MOVE_VEL + AE_PA_VECTOR_Y),
            (uint32_t)AE_PA_MODE_IDLE_FALL_DELTA);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_TARGET_MOVE_VEL + AE_PA_VECTOR_Y),
            (uint32_t)AE_PA_MODE_IDLE_FALL_DELTA);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_ATTACH_FLAGS), 0);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y),
            (uint32_t)pa_mode_sub_s32(ae_rds32(r,
                    pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y)),
                    AE_PA_MODE_IDLE_FALL_HEIGHT));

    if (ae_rd8(r, pa_mode_add(AE_PA_MODE_IDLE_BLOCK, AE_PA_MODE_IDLE_GADGET_STATE)) == 1 &&
        !pa_mode_aim_leave(r, hooks)) return;
    if (!pa_mode_select_gadget(r, hooks,
            ae_rds8(r, pa_mode_add(player, AE_PA_MODE_PLAYER_GADGET_AFTERIMAGE)),
            ae_rds8(r, pa_mode_add(player, AE_PA_PLAYER_CURRENT_GADGET)))) return;
}

static void pa_mode_leave_scripted(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t actor = ae_rd32(r, AE_PA_MODE_ACTOR_POINTER);
    const uint32_t actor_model = ae_rd32(r, pa_mode_add(actor, AE_PA_ACTOR_MODEL));
    const uint32_t facing = pa_mode_add(player, AE_PA_PLAYER_FACING);
    const uint32_t position = pa_mode_add(player, AE_PA_PLAYER_POSITION);
    uint32_t model_flags;

    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_ACTOR_REST_STATE)) return;
    ae_wr16(r, pa_mode_add(actor_model, AE_PA_VECTOR_X), ae_rd16(r, position + AE_PA_VECTOR_X));
    ae_wr16(r, pa_mode_add(actor_model, AE_PA_SVECTOR_Y),
            (uint16_t)(ae_rd16(r, position + AE_PA_VECTOR_Y) + 0x80u));
    ae_wr16(r, pa_mode_add(actor_model, AE_PA_SVECTOR_Z), ae_rd16(r, position + AE_PA_VECTOR_Z));
    pa_mode_copy_bytes(r, pa_mode_add(actor, AE_PA_ACTOR_FACING), facing,
                       AE_PA_MODE_ACTOR_ROTATION_SIZE);
    model_flags = ae_rd16(r, pa_mode_add(actor_model, AE_PA_MODE_ACTOR_MODEL_FLAGS));
    ae_wr16(r, pa_mode_add(actor_model, AE_PA_MODE_ACTOR_MODEL_FLAGS), model_flags & 0x7FFFu);
    ae_wr8(r, pa_mode_add(actor, AE_PA_ACTOR_MODE), AE_PA_MODE_IDLE_ACTOR_ACTION);

    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_Y),
            (uint32_t)AE_PA_MODE_IDLE_FALL_SPEED);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_MOVE_VEL + AE_PA_VECTOR_Y),
            (uint32_t)AE_PA_MODE_IDLE_FALL_DELTA);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_TARGET_MOVE_VEL + AE_PA_VECTOR_Y),
            (uint32_t)AE_PA_MODE_IDLE_FALL_DELTA);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_ATTACH_FLAGS), 0);
    ae_wr32(r, pa_mode_add(player, AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y),
            (uint32_t)pa_mode_sub_s32(ae_rds32(r, position + AE_PA_VECTOR_Y), 2));
    ae_wr16(r, pa_mode_add(facing, AE_PA_VECTOR_X),
            (uint16_t)pa_mode_wrap(0u - ae_rd16(r, pa_mode_add(facing, AE_PA_VECTOR_X))));
    ae_wr16(r, pa_mode_add(facing, AE_PA_SVECTOR_Z),
            (uint16_t)pa_mode_wrap(0u - ae_rd16(r, pa_mode_add(facing, AE_PA_SVECTOR_Z))));
    if (!pa_mode_select_gadget(r, hooks,
            ae_rds8(r, pa_mode_add(player, AE_PA_MODE_PLAYER_GADGET_AFTERIMAGE)),
            ae_rds8(r, pa_mode_add(player, AE_PA_PLAYER_CURRENT_GADGET)))) return;
}

static void pa_mode_enter_climb(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t direction = pa_mode_add(player, AE_PA_PLAYER_DESIRED_DIR);
    const uint32_t target = pa_mode_add(player, AE_PA_PLAYER_CLIMB_TARGET);
    const uint32_t position = pa_mode_add(player, AE_PA_PLAYER_POSITION);

    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_CLIMB_STATE)) return;
    ae_wr16(r, direction + AE_PA_SVECTOR_Y, 0);
    ae_wr16(r, direction + AE_PA_SVECTOR_X,
            ae_rd16(r, target + AE_PA_SVECTOR_X) - ae_rd16(r, position + AE_PA_SVECTOR_X));
    ae_wr16(r, direction + AE_PA_SVECTOR_Z,
            ae_rd16(r, target + AE_PA_SVECTOR_Z) - ae_rd16(r, position + AE_PA_VECTOR_Z));
    (void)ae_sdk_VectorNormalSS(r, direction, direction);
}

static int32_t pa_mode_divide(AeRam *r, int32_t numerator, int32_t denominator)
{
    if (denominator == 0) {
        ae_ram_fail(r, "PlayerSetMode rope length division by zero");
        return 0;
    }
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

static void pa_mode_enter_rope(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t position = pa_mode_add(player, AE_PA_PLAYER_POSITION);
    const uint32_t rope_a = pa_mode_add(player, AE_PA_MODE_PLAYER_ROPE_FIRST);
    const uint32_t rope_b = pa_mode_add(player, AE_PA_MODE_PLAYER_ROPE_SECOND);
    const uint32_t temp = AE_PA_MODE_ROPE_TEMP;
    int32_t a_length;
    int32_t b_length;
    int32_t total_length;
    int32_t fraction;
    int32_t square_sum;

    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_ROPE_STATE)) return;
    a_length = ae_player_vec_length3(
            pa_mode_sub_s32(ae_rds32(r, position + AE_PA_VECTOR_X), pa_mode_s16(r, rope_a)),
            0,
            pa_mode_sub_s32(ae_rds32(r, position + AE_PA_VECTOR_Z), pa_mode_s16(r, rope_a + AE_PA_SVECTOR_Z)));
    b_length = ae_player_vec_length3(
            pa_mode_sub_s32(ae_rds32(r, position + AE_PA_VECTOR_X), pa_mode_s16(r, rope_b)),
            0,
            pa_mode_sub_s32(ae_rds32(r, position + AE_PA_VECTOR_Z), pa_mode_s16(r, rope_b + AE_PA_SVECTOR_Z)));
    total_length = pa_mode_add_s32(a_length, b_length);
    fraction = pa_mode_divide(r, pa_mode_wrap((uint32_t)b_length << 12), total_length);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, player + AE_PA_MODE_PLAYER_ROPE_FRACTION, (uint16_t)fraction);
    ae_wr16(r, temp + AE_PA_SVECTOR_X,
            ae_rd16(r, rope_b) - ae_rd16(r, rope_a));
    ae_wr16(r, temp + AE_PA_SVECTOR_Y,
            ae_rd16(r, rope_b + AE_PA_SVECTOR_Y) - ae_rd16(r, rope_a + AE_PA_SVECTOR_Y));
    ae_wr16(r, temp + AE_PA_SVECTOR_Z,
            ae_rd16(r, rope_b + AE_PA_SVECTOR_Z) - ae_rd16(r, rope_a + AE_PA_SVECTOR_Z));
    square_sum = ae_sdk_VectorNormalSS(r, temp, player + AE_PA_MODE_PLAYER_ROPE_NORMAL);
    ae_wr16(r, player + AE_PA_MODE_PLAYER_ROPE_LENGTH,
            (uint16_t)ae_player_int_sqrt(square_sum));
}

static void pa_mode_enter_idle(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t idle_actor = ae_rd32(r, AE_PA_MODE_IDLE_ACTOR_POINTER);
    const uint32_t idle_block = AE_PA_MODE_IDLE_BLOCK;
    const int32_t current_gadget = ae_rds8(r, player + AE_PA_MODE_PLAYER_IDLE_GADGET);
    uint32_t idle_blob;

    ae_wr16(r, idle_actor + AE_PA_MODE_IDLE_GADGET_FIELD, (uint16_t)current_gadget);
    if (!pa_mode_select_gadget(r, hooks, AE_PA_MODE_GADGET_SELECT, current_gadget)) return;
    ae_wr16(r, player + AE_PA_PLAYER_STATE, AE_PA_MODE_IDLE_STATE);
    ae_wr16(r, player + AE_PA_PLAYER_ANIM_FRAME, 0);
    if (ae_rd8(r, idle_block + AE_PA_MODE_IDLE_GADGET_STATE) == 1 &&
        !pa_mode_aim_leave(r, hooks)) return;
    if (ae_rd8(r, idle_block + AE_PA_MODE_IDLE_ACTIVE_STATE) == 1) {
        ae_wr8(r, idle_block + AE_PA_MODE_IDLE_ACTIVE_STATE, 0);
    }
    idle_blob = ae_rd32(r,
            ae_rd32(r, AE_PA_MODE_IDLE_ACTOR_POINTER) + AE_PA_MODE_IDLE_PLAYER_BLOB);
    (void)pa_mode_idle_setup(r, hooks, idle_blob);
}

static void pa_mode_leave_idle(AeRam *r, const AePlayerActHooks *hooks, int32_t mode)
{
    const uint32_t idle_actor = ae_rd32(r, AE_PA_MODE_IDLE_ACTOR_POINTER);
    const int32_t gadget = pa_mode_s16(r, idle_actor + AE_PA_MODE_IDLE_GADGET_FIELD);
    const uint32_t blob_buffer = ae_rd32(r, idle_actor + AE_PA_MODE_IDLE_BUFFER_POINTER);

    ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
    if (!pa_mode_select_gadget(r, hooks, gadget, -1)) return;
    if (!pa_mode_heap_free(r, hooks, blob_buffer)) return;
    if (!pa_mode_heap_free(r, hooks, idle_actor)) return;
    if (!pa_mode_idle_setup(r, hooks, 0)) return;
    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_IDLE_EXIT_STATE)) return;
    ae_wr16(r, AE_PA_MODE_TRANSITION_FLAGS, 0);
}

static void pa_mode_enter_catch(AeRam *r, const AePlayerActHooks *hooks, int32_t mode)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;

    ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_CATCH_STATE)) return;
    ae_wr32(r, player + AE_PA_PLAYER_TARGET_MOVE_VEL + AE_PA_VECTOR_Z, 0);
    ae_wr32(r, player + AE_PA_PLAYER_TARGET_MOVE_VEL + AE_PA_VECTOR_X, 0);
    ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_Z, 0);
    ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_X, 0);
    ae_wr32(r, AE_PA_MODE_HUD_FLAGS, 0);
    if (!pa_mode_aim_leave(r, hooks)) return;
    if (hooks == NULL || hooks->AimRequestClear == NULL) {
        ae_ram_fail(r, "AimRequestClear hook not provided");
        return;
    }
    hooks->AimRequestClear(hooks->user, r);
}

static void pa_mode_enter_squash(AeRam *r, const AePlayerActHooks *hooks, int32_t mode)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;

    ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
    ae_wr16(r, player + AE_PA_PLAYER_SQUASH_FRAME, AE_PA_MODE_SQUASH_FRAMES);
    ae_wr32(r, player + AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y,
            ae_rd32(r, player + AE_PA_PLAYER_POSITION + AE_PA_VECTOR_Y) - AE_PA_MODE_SQUASH_HEIGHT);
    if (!pa_mode_player_state(r, hooks, AE_PA_MODE_FALL_STATE)) return;
    ae_wr16(r, player + AE_PA_PLAYER_ANIM_BLEND, 0);
}

static void pa_mode_dispatch_request13(AeRam *r, int32_t mode)
{
    ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
    ae_wr16(r, AE_PA_MODE_REQUEST13_FLAGS, 0);
}

static void pa_mode_leave_controller8(AeRam *r, const AePlayerActHooks *hooks, int32_t mode)
{
    const uint32_t area = AE_PA_MODE_AREA_SLOT;
    const uint32_t area_id = ae_rd16(r, area + AE_PA_MODE_AREA_ID);
    const uint32_t area_argument = ae_rd16(r, area + AE_PA_MODE_AREA_ARGUMENT);
    const uint32_t exit_code = ae_rd16(r, area + AE_PA_MODE_AREA_EXIT_CODE);
    int32_t goal_reached;

    if (mode == 9) {
        const uint32_t player = AE_PLAYER_ACT_PLAYER;
        ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
        if (!pa_mode_player_state(r, hooks, AE_PA_MODE_CATCH_STATE)) return;
        ae_wr32(r, player + AE_PA_PLAYER_TARGET_MOVE_VEL + AE_PA_VECTOR_Z, 0);
        ae_wr32(r, player + AE_PA_PLAYER_TARGET_MOVE_VEL + AE_PA_VECTOR_X, 0);
        ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_Z, 0);
        ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_X, 0);
        ae_wr32(r, AE_PA_MODE_HUD_FLAGS, 0);
        return;
    }

    ae_wr32(r, area + AE_PA_MODE_AREA_SCRIPT_FLAG, 0);
    if (mode == 0 && exit_code == AE_PA_MODE_SPECIAL_STAGE_EXIT) {
        if (!pa_mode_stage_request(r, hooks, 3, (int32_t)area_id, (int32_t)area_argument)) return;
        ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
        return;
    }

    if (exit_code != AE_PA_MODE_SPECIAL_STAGE_EXIT) {
        if (hooks == NULL || hooks->StageGoalReached == NULL) {
            ae_ram_fail(r, "StageGoalReached hook not provided");
            return;
        }
        goal_reached = hooks->StageGoalReached(hooks->user, r);
        if (goal_reached != 0) {
            (void)pa_mode_stage_request(r, hooks, 0x12, (int32_t)area_id, (int32_t)area_argument);
            return;
        }
    }

    if (!pa_mode_stage_request(r, hooks, 3, (int32_t)area_id, (int32_t)area_argument)) return;
    if (exit_code != AE_PA_MODE_SPECIAL_STAGE_EXIT) {
        if (hooks == NULL || hooks->HudPanelA_Start == NULL) {
            ae_ram_fail(r, "HudPanelA_Start hook not provided");
            return;
        }
        hooks->HudPanelA_Start(hooks->user, r, 0);
    }
    ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
}

void ae_PlayerSetMode(AeRam *r, const AePlayerActHooks *hooks, int32_t mode)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const int32_t current_mode = ae_rd16(r, AE_PLAYER_ACT_MODE);

    if (mode == AE_PA_MODE_ENTER_SPECIAL_A || mode == AE_PA_MODE_ENTER_SPECIAL_B) {
        ae_wr16(r, AE_PA_MODE_REQUEST_FLAGS, AE_PA_MODE_ENTER_SPECIAL_A);
        ae_wr32(r, AE_PA_MODE_HUD_FLAGS,
                ae_rd32(r, AE_PA_MODE_HUD_FLAGS) & ~0x10u);
        if (mode == AE_PA_MODE_ENTER_SPECIAL_A) {
            ae_wr32(r, AE_PA_MODE_REQUEST_MODE11_VALUE, 14);
        }
        ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_X, 0);
        ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_Y, 0);
        ae_wr32(r, player + AE_PA_PLAYER_VELOCITY + AE_PA_VECTOR_Z, 0);
        if (ae_rds8(r, player + AE_PA_PLAYER_CURRENT_GADGET) == AE_PA_MODE_SPECIAL_GADGET &&
            ae_rd16(r, AE_PA_GADGET4_MODE) != AE_PA_MODE_SPECIAL_GADGET_RESET) {
            ae_wr16(r, AE_PA_GADGET4_MODE, 0);
            ae_wr32(r, AE_PA_GADGET4_AUX, 0);
        }
        return;
    }

    if (current_mode == 0 || current_mode == 5 || current_mode == 6) {
        if (mode < 0 || mode >= 14) {
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            return;
        }
        switch (mode) {
        case 0:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            ae_wr16(r, AE_PA_MODE_REQUEST_FLAGS, 0);
            ae_wr32(r, AE_PA_MODE_STAGE_ENTRY_FLAGS, 0);
            return;
        case 1:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            pa_mode_enter_actor(r, hooks);
            return;
        case 2:
        case 4:
        case 11:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            return;
        case 3:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            pa_mode_enter_scripted(r, hooks);
            return;
        case 5:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            pa_mode_enter_climb(r, hooks);
            return;
        case 6:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            pa_mode_enter_rope(r, hooks);
            return;
        case 7:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            (void)pa_mode_player_state(r, hooks, AE_PA_MODE_SWIM_STATE);
            return;
        case 8:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            ae_wr16(r, AE_PA_MODE_GADGET4_EFFECT, 0x26u);
            return;
        case 9:
            pa_mode_enter_catch(r, hooks, mode);
            return;
        case 10:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            pa_mode_enter_idle(r, hooks);
            return;
        case 12:
            pa_mode_enter_squash(r, hooks, mode);
            return;
        case 13:
            pa_mode_dispatch_request13(r, mode);
            return;
        default:
            ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
            return;
        }
    }

    if (current_mode == 1) {
        ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
        pa_mode_leave_actor(r, hooks);
        return;
    }
    if (current_mode == 3) {
        ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
        pa_mode_leave_scripted(r, hooks);
        return;
    }
    if (current_mode == 8) {
        pa_mode_leave_controller8(r, hooks, mode);
        return;
    }
    if (current_mode == AE_PA_MODE_IDLE_ANIMATION) {
        pa_mode_leave_idle(r, hooks, mode);
        return;
    }
    if (mode == 13) {
        pa_mode_dispatch_request13(r, mode);
        return;
    }
    ae_wr16(r, AE_PLAYER_ACT_MODE, (uint16_t)mode);
}
