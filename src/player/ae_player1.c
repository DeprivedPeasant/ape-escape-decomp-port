#include "ae_player1.h"
#include "../core/ae_pose.h"
#include "../core/ae_sdk.h"
#include "../player/ae_player_spawn.h"
#include "../world/ae_areaset.h"

#include <limits.h>

static uint32_t player1_call_hook(AeRam *r, const AePlayer1Hooks *hooks,
                                  AePlayer1HookFn hook, const char *name,
                                  uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (hooks == NULL || hook == NULL) {
        ae_ram_fail(r, "AePlayer1Hooks.%s hook not provided", name);
        return 0;
    }
    return hook(hooks->user, r, a0, a1, a2, a3);
}

static int32_t player1_sra32(int32_t value, uint32_t shift) {
    shift &= 31u;
    if (shift == 0) return value;
    if (value >= 0) return (int32_t)((uint32_t)value >> shift);
    return (int32_t)~((~(uint32_t)value) >> shift);
}

static int player1_divide(AeRam *r, int32_t numerator, int32_t denominator,
                          int32_t *quotient, const char *function_name) {
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(r, "%s: original signed divide trap", function_name);
        return 0;
    }
    *quotient = numerator / denominator;
    return 1;
}

void ae_TrailRingReset(AeRam *r) {
    ae_wr32(r, AE_PLAYER1_TRAIL_RING, 0);
    for (uint32_t entry = 0; entry < AE_PLAYER1_TRAIL_RING_RESET_COUNT; ++entry) {
        ae_wr32(r, AE_PLAYER1_TRAIL_RING + AE_PLAYER1_TRAIL_RING_FIRST_WORD -
                   entry * AE_PLAYER1_TRAIL_RING_STRIDE, 0);
    }
}

void ae_AimRequestClear(AeRam *r) {
    uint32_t request = AE_PLAYER1_AIM_STATE + AE_PLAYER1_AIM_REQUEST;
    if (ae_rd8(r, request) == 1u) ae_wr8(r, request, 0);
}

void ae_CameraFixedModeOff(AeRam *r) {
    ae_wr32(r, AE_PLAYER1_CAMERA_MODE, 0);
    ae_wr8(r, AE_PLAYER1_CAMERA_FLAGS, 0);
}

int32_t ae_IsArea53(AeRam *r) {
    return ae_rd16(r, AE_PLAYER1_AREA_ID) == AE_PLAYER1_CURRENT_AREA_IS_53;
}

void ae_HudShowHealth(AeRam *r, int32_t mode) {
    uint32_t flags_address = AE_PLAYER1_HUD + AE_PLAYER1_HUD_FLAGS;

    if (mode == 0) {
        ae_wr16(r, AE_PLAYER1_HUD + AE_PLAYER1_HUD_HEALTH_TIMER, 0);
    } else if (mode == 1) {
        uint32_t flags = ae_rd32(r, flags_address);
        if ((flags & AE_PLAYER1_HUD_VISIBLE_BIT) != 0) {
            int32_t timer = ae_rds16(r, AE_PLAYER1_HUD + AE_PLAYER1_HUD_HEALTH_TIMER);
            if (timer >= AE_PLAYER1_HUD_TIMER_LIMIT) {
                ae_wr16(r, AE_PLAYER1_HUD + AE_PLAYER1_HUD_HEALTH_TIMER,
                        AE_PLAYER1_HUD_TIMER_VALUE);
            }
        } else {
            ae_wr16(r, AE_PLAYER1_HUD + AE_PLAYER1_HUD_HEALTH_TIMER, 0);
        }
    }

    ae_wr32(r, flags_address, ae_rd32(r, flags_address) | AE_PLAYER1_HUD_VISIBLE_BIT);
}

void ae_PlayerDamage(AeRam *r, int32_t amount) {
    if (amount == 0) return;

    uint32_t level = ae_rd16(r, AE_PLAYER1_CURRENT_LEVEL);
    if (level == AE_PLAYER1_LEVEL_NO_DAMAGE_A || level == AE_PLAYER1_LEVEL_NO_DAMAGE_B) return;

    uint32_t health_address = AE_PLAYER1_PLAYER + AE_PLAYER1_PLAYER_HEALTH;
    int32_t health = ae_rds16(r, health_address);
    int32_t remaining = (int32_t)((uint32_t)health - (uint32_t)amount);
    if (remaining < 0) remaining = 0;
    ae_wr16(r, health_address, (uint32_t)remaining);

    ae_HudShowHealth(r, 1);
    ae_wr16(r, AE_PLAYER1_HUD + AE_PLAYER1_HUD_DAMAGE, (uint32_t)amount);
    ae_wr16(r, AE_PLAYER1_HUD + AE_PLAYER1_HUD_DURATION, AE_PLAYER1_DAMAGE_HUD_FRAMES);
    ae_wr16(r, AE_PLAYER1_PLAYER + AE_PLAYER1_PLAYER_INVULN_TIMER,
            AE_PLAYER1_DAMAGE_INVULN_FRAMES);
    ae_wr8(r, AE_PLAYER1_PLAYER + AE_PLAYER1_PLAYER_HURT, 1);
}

uint32_t ae_ArcGetMemberPos(AeRam *r, const AePlayer1Hooks *hooks,
                            uint32_t output, uint32_t member_index) {
    uint32_t member_word = ae_rd32(r, AE_PLAYER1_ARC_MEMBER_TABLE +
                                      member_index * AE_PLAYER1_ARC_MEMBER_STRIDE);
    uint32_t sector = member_word & AE_PLAYER1_ARC_MEMBER_SECTOR_MASK;
    (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->CdIntToPos : NULL,
                            "CdIntToPos", sector, output, 0, 0);
    uint32_t sector_offset = (member_word >> AE_PLAYER1_ARC_MEMBER_OFFSET_SHIFT)
                           << AE_PLAYER1_ARC_SECTOR_BYTES_SHIFT;
    ae_wr32(r, output + AE_PLAYER1_ARC_MEMBER_OFFSET_FIELD, sector_offset);
    return output;
}

uint32_t ae_ActorAnimDecodeEntry(AeRam *r, uint32_t actor, uint32_t entry) {
    uint32_t current_frame = ae_rd16(r, actor + AE_PLAYER1_ACTOR_ANIM_FRAME);
    uint32_t first_frame = ae_rd16(r, entry + AE_PLAYER1_ANIM_ENTRY_FIRST_FRAME);
    int32_t frame_delta = (int32_t)(current_frame - first_frame);
    int32_t scaled_delta = (int32_t)((uint32_t)frame_delta << AE_PLAYER1_ANIM_FRACTION_BITS);
    int32_t duration = (int32_t)ae_rd16(r, entry + AE_PLAYER1_ANIM_ENTRY_DURATION);
    int32_t scaled_quotient;
    if (!player1_divide(r, scaled_delta, duration, &scaled_quotient, "ActorAnimDecodeEntry")) return 0;

    int32_t rounded_quotient = scaled_quotient;
    if (rounded_quotient < 0) rounded_quotient += AE_PLAYER1_ANIM_NEGATIVE_BIAS;
    int32_t integral_fraction = player1_sra32(rounded_quotient, AE_PLAYER1_ANIM_FRACTION_BITS)
                              * (int32_t)AE_PLAYER1_ANIM_ONE;
    uint32_t fraction = (uint32_t)scaled_quotient - (uint32_t)integral_fraction;

    int32_t frame_step;
    if (!player1_divide(r, frame_delta, duration, &frame_step, "ActorAnimDecodeEntry")) return 0;
    uint32_t frame_a = (uint32_t)frame_step + ae_rd16(r, entry + AE_PLAYER1_ANIM_ENTRY_FRAME_BASE);
    uint32_t frame_b = frame_a + 1u;
    if (ae_ram_failed(r)) return 0;
    return ae_AnimDecodeLerp(r, actor, frame_a, frame_b, fraction);
}

void ae_PlayerIdleModelSetup(AeRam *r, uint32_t blob) {
    uint32_t player = AE_PLAYER1_PLAYER;
    if (blob == 0) {
        ae_wr32(r, AE_PLAYER1_IDLE_MODEL_BLOCK, 0);
        ae_wr8(r, player + AE_PLAYER1_PLAYER_IDLE_LOADING, 0);
        ae_wr8(r, player + AE_PLAYER1_PLAYER_IDLE_READY, 0);
        ae_wr16(r, player + AE_PLAYER1_PLAYER_IDLE_TIMER, 0);
        return;
    }

    uint32_t block = ae_rd32(r, AE_PLAYER1_IDLE_MODEL_BLOCK);
    uint32_t state_sets = AE_PLAYER1_PLAYER_STATE_SETS;

    uint32_t player_frames = blob + ae_rd16(r, blob + AE_PLAYER1_IDLE_BLOB_PLAYER_FRAMES_FIELD) * 4u;
    ae_wr32(r, block + AE_PLAYER1_IDLE_BLOCK_PLAYER_FRAMES, player_frames);
    ae_wr32(r, state_sets + AE_PLAYER1_IDLE_STATESET_PLAYER_FRAMES, player_frames);

    uint32_t states = blob + ae_rd16(r, blob + AE_PLAYER1_IDLE_BLOB_STATES_FIELD) * 4u;
    ae_wr32(r, block + AE_PLAYER1_IDLE_BLOCK_STATES, states);
    ae_wr32(r, state_sets + AE_PLAYER1_IDLE_STATESET_STATES, states);

    uint32_t transitions = blob + ae_rd16(r, blob + AE_PLAYER1_IDLE_BLOB_TRANSITIONS_FIELD) * 4u;
    ae_wr32(r, block + AE_PLAYER1_IDLE_BLOCK_TRANSITIONS, transitions);
    ae_wr32(r, state_sets + AE_PLAYER1_IDLE_STATESET_TRANSITIONS, transitions);

    uint32_t events = blob + ae_rd16(r, blob + AE_PLAYER1_IDLE_BLOB_EVENTS_FIELD) * 4u;
    ae_wr32(r, block + AE_PLAYER1_IDLE_BLOCK_EVENTS, events);
    ae_wr32(r, state_sets + AE_PLAYER1_IDLE_STATESET_EVENTS, events);

    uint32_t model_header = blob + ae_rd16(r, blob + AE_PLAYER1_IDLE_BLOB_MODEL_HEADER_FIELD) * 4u;
    ae_ModelInit(r, NULL, block + AE_PLAYER1_IDLE_BLOCK_MODEL_INSTANCE, model_header);
    uint32_t secondary_frames = blob + ae_rd16(r, blob + AE_PLAYER1_IDLE_BLOB_SECONDARY_FRAMES_FIELD) * 4u;
    ae_wr32(r, block + AE_PLAYER1_IDLE_BLOCK_SECONDARY_FRAMES, secondary_frames);
}

void ae_AimModeEnter(AeRam *r, const AePlayer1Hooks *hooks) {
    uint32_t aim_mode = AE_PLAYER1_AIM_STATE + AE_PLAYER1_AIM_MODE;
    if (ae_rd8(r, aim_mode) == 1u) return;
    if (ae_IsArea53(r) != 0) return;

    ae_wr8(r, aim_mode, 1);
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound_hw : NULL, 4);
}

void ae_EventCamStartRandom(AeRam *r, const AePlayer1Hooks *hooks) {
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound_hw : NULL, 0x52);

    uint32_t previous = ae_rd16(r, AE_PLAYER1_EVENT_CAMERA_SEED);
    uint32_t next = previous * 5u + 1u;
    uint32_t create_flags = ((next >> 8) & 1u) | 2u;
    ae_wr16(r, AE_PLAYER1_EVENT_CAMERA_SEED, next);
    (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->EventCamCreate : NULL,
                            "EventCamCreate", create_flags, 0, 0, 0);

    ae_wr8(r, AE_PLAYER1_AIM_STATE + AE_PLAYER1_EVENT_CAMERA_ACTIVE, 1);
    ae_wr16(r, AE_PLAYER1_AIM_STATE + AE_PLAYER1_EVENT_CAMERA_TIMER, 0);
    ae_wr8(r, AE_PLAYER1_AIM_STATE + AE_PLAYER1_EVENT_CAMERA_FLAGS, 0);
    ae_wr32(r, AE_PLAYER1_CAMERA_MODE, 0);
}

static uint32_t player1_cd_entry_address(int32_t index) {
    return AE_PLAYER1_CD_MANAGER + AE_PLAYER1_CD_QUEUE +
           (uint32_t)index * AE_PLAYER1_CD_QUEUE_ENTRY_SIZE;
}

static int32_t player1_cd_next_index(int32_t current) {
    int32_t candidate = current + 1;
    int32_t rounded = candidate < 0 ? current + (int32_t)AE_PLAYER1_CD_QUEUE_CAPACITY : candidate;
    int32_t remainder_base = player1_sra32(rounded, 4) * 16;
    return candidate - remainder_base;
}

int32_t ae_CdMgrIssueCommand(AeRam *r, const AePlayer1Hooks *hooks, uint32_t queue_entry) {
    uint32_t manager = AE_PLAYER1_CD_MANAGER;
    uint32_t command = ae_rd8(r, queue_entry + AE_PLAYER1_CD_QUEUE_COMMAND);
    uint32_t payload = ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_POINTER);
    uint32_t result = ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_RESULT);

    if (command == AE_PLAYER1_CD_SPECIAL_COMMAND) {
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_A,
                ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_B));
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_B,
                ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_B));
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_C,
                ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_DONE_FLAG));
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_D,
                ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_C));
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_RESULT_BUFFER_E,
                ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_C));

        uint32_t status = player1_call_hook(r, hooks, hooks != NULL ? hooks->CdControl : NULL,
                                            "CdControl", AE_PLAYER1_CD_SPECIAL_CONTROL_COMMAND,
                                            payload, result, 0);
        if (ae_ram_failed(r) || status != 1u) return 0;

        (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->CdReadyCallback : NULL,
                                "CdReadyCallback", AE_PLAYER1_CD_READY_CALLBACK_ARG, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_STATE, AE_PLAYER1_CD_PENDING_STATE);
        ae_wr8(r, queue_entry + AE_PLAYER1_CD_QUEUE_COMMAND, AE_PLAYER1_XA_PAUSE_COMMAND);
        uint32_t vsync_result = player1_call_hook(r, hooks, hooks != NULL ? hooks->VSync : NULL,
                                                  "VSync", 0xFFFFFFFFu, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_VSYNC_RESULT, vsync_result);
        return 1;
    }

    ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_SUBSTATE, 0);
    uint32_t status = player1_call_hook(r, hooks, hooks != NULL ? hooks->CdControl : NULL,
                                        "CdControl", command, payload, result, 0);
    if (ae_ram_failed(r) || status != 1u) return 0;

    switch (command) {
        case 2u: {
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_POSITION,
                    AE_PLAYER1_CD_POSITION_UNKNOWN);
            uint32_t position = player1_call_hook(r, hooks, hooks != NULL ? hooks->CdPosToInt : NULL,
                                                  "CdPosToInt", payload, 0, 0, 0);
            if (ae_ram_failed(r)) return 0;
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_POSITION_INT, position);
            break;
        }
        case AE_PLAYER1_CD_READ_COMMAND:
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_STATE, AE_PLAYER1_CD_PENDING_STATE);
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_SUBSTATE, AE_PLAYER1_CD_READ_SUBSTATE);
            break;
        case AE_PLAYER1_XA_PAUSE_COMMAND:
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_STATE, AE_PLAYER1_CD_BUSY_STATE);
            break;
        case AE_PLAYER1_XA_MODE_CONFIG_COMMAND:
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_LAST_STATUS,
                    (uint32_t)(int32_t)ae_rds16(r, manager + AE_PLAYER1_CD_MANAGER_READ_INDEX));
            break;
        case 0x15u:
        case 0x16u:
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_STATE, AE_PLAYER1_CD_DATA_STATE);
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_SUBSTATE, AE_PLAYER1_CD_DATA_SUBSTATE);
            break;
        case AE_PLAYER1_XA_READ_COMMAND:
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_STATE, AE_PLAYER1_CD_READING_STATE);
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_SUBSTATE, AE_PLAYER1_CD_RETRY_SUBSTATE);
            ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_RETRY_COUNT, 0);
            break;
        default:
            break;
    }

    ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_LAST_COMMAND, command);
    uint32_t done_flag = ae_rd32(r, queue_entry + AE_PLAYER1_CD_QUEUE_DONE_FLAG);
    if (done_flag != 0) ae_wr8(r, done_flag, 1);

    int32_t read_index = ae_rds16(r, manager + AE_PLAYER1_CD_MANAGER_READ_INDEX);
    ae_wr16(r, manager + AE_PLAYER1_CD_MANAGER_READ_INDEX,
            (uint32_t)player1_cd_next_index(read_index));
    uint32_t vsync_result = player1_call_hook(r, hooks, hooks != NULL ? hooks->VSync : NULL,
                                              "VSync", 0xFFFFFFFFu, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_VSYNC_RESULT, vsync_result);
    return 1;
}

int32_t ae_CdMgrQueueCommand(AeRam *r, const AePlayer1Hooks *hooks, int32_t command,
                             uint32_t parameters, uint32_t result, uint32_t done_flag) {
    uint32_t manager = AE_PLAYER1_CD_MANAGER;
    int32_t write_index = ae_rds16(r, manager + AE_PLAYER1_CD_MANAGER_WRITE_INDEX);
    int32_t next_index = player1_cd_next_index(write_index);
    int32_t read_index = ae_rds16(r, manager + AE_PLAYER1_CD_MANAGER_READ_INDEX);

    if (next_index == read_index) {
        uint32_t queue_entry = player1_cd_entry_address(next_index);
        (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->printf : NULL, "printf",
                                AE_PLAYER1_CD_QUEUE_FULL_FORMAT, (uint32_t)next_index,
                                manager, done_flag);
        if (ae_ram_failed(r)) return 0;
        while (ae_CdMgrIssueCommand(r, hooks, queue_entry) == 0) {
            if (ae_ram_failed(r)) return 0;
        }
        write_index = ae_rds16(r, manager + AE_PLAYER1_CD_MANAGER_WRITE_INDEX);
        next_index = player1_cd_next_index(write_index);
    }

    uint32_t queue_entry = player1_cd_entry_address(write_index);
    ae_wr8(r, queue_entry + AE_PLAYER1_CD_QUEUE_COMMAND, (uint32_t)command);
    if (parameters == 0) {
        ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_POINTER, 0);
    } else {
        ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_POINTER,
                queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD);
        ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD,
                ae_rd32(r, parameters));
        if (((uint32_t)command & 0xFFu) == AE_PLAYER1_CD_SPECIAL_COMMAND) {
            ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_B,
                    ae_rd32(r, parameters + AE_PLAYER1_CD_INPUT_WORD_B));
            ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_PAYLOAD_WORD_C,
                    ae_rd32(r, parameters + AE_PLAYER1_CD_INPUT_WORD_C));
        }
    }

    ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_RESULT, result);
    ae_wr32(r, queue_entry + AE_PLAYER1_CD_QUEUE_DONE_FLAG, done_flag);
    int32_t new_write_index = player1_cd_next_index(write_index);
    ae_wr16(r, manager + AE_PLAYER1_CD_MANAGER_WRITE_INDEX, (uint32_t)new_write_index);
    if (done_flag != 0) ae_wr8(r, done_flag, 0);
    if (ae_rd32(r, manager + AE_PLAYER1_CD_MANAGER_STATE) == 0) {
        ae_wr32(r, manager + AE_PLAYER1_CD_MANAGER_STATE, AE_PLAYER1_CD_BUSY_STATE);
    }
    return 1;
}

int32_t ae_XaPlay(AeRam *r, const AePlayer1Hooks *hooks, int32_t index) {
    uint32_t table_entry = AE_PLAYER1_XA_ENTRY_TABLE +
                           (uint32_t)index * AE_PLAYER1_XA_ENTRY_STRIDE;
    uint32_t flags = ae_rd8(r, table_entry + AE_PLAYER1_XA_ENTRY_FLAGS);
    uint32_t volume_mode_bits = flags & 0x0Cu;
    int32_t volume_mode = volume_mode_bits == 4u ? 0 :
                          volume_mode_bits == 8u ? 1 : -1;
    (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->SoundSetVolumeMode : NULL,
                            "SoundSetVolumeMode", (uint32_t)volume_mode, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t status = AE_PLAYER1_CD_STATUS;
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_MODE_COMMAND, 0, status, 0);

    ae_wr8(r, AE_PLAYER1_XA_STACK_PARAMS, ae_rd8(r, table_entry + AE_PLAYER1_XA_ENTRY_CHANNEL));
    ae_wr8(r, AE_PLAYER1_XA_STACK_PARAMS + 1u,
           ae_rd8(r, table_entry + AE_PLAYER1_XA_ENTRY_FILE));
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_FILTER_COMMAND,
                               AE_PLAYER1_XA_STACK_PARAMS, status, 0);

    ae_wr8(r, AE_PLAYER1_XA_STACK_MODE, AE_PLAYER1_XA_MODE_VALUE);
    if ((flags & 1u) != 0) {
        ae_wr8(r, AE_PLAYER1_XA_STACK_MODE, AE_PLAYER1_XA_LOOP_MODE_VALUE);
    }
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_MODE_CONFIG_COMMAND,
                               AE_PLAYER1_XA_STACK_MODE, status, 0);

    uint32_t sector = ae_rd32(r, table_entry + AE_PLAYER1_XA_ENTRY_SECTOR);
    (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->CdIntToPos : NULL,
                            "CdIntToPos", sector, AE_PLAYER1_XA_STACK_LOCATION, 0, 0);
    if (ae_ram_failed(r)) return 0;
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_DEMUTE_COMMAND, 0, status, 0);
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_SYNC_COMMAND, 0, status, 0);
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_READ_COMMAND,
                               AE_PLAYER1_XA_STACK_LOCATION, status, 0);
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_MODE_COMMAND, 0, status, 0);
    (void)ae_CdMgrQueueCommand(r, hooks, AE_PLAYER1_XA_PAUSE_COMMAND, 0, status, 0);

    ae_wr16(r, AE_PLAYER1_CD_MANAGER + AE_PLAYER1_CD_MANAGER_XA_INDEX, (uint32_t)index);
    ae_wr32(r, AE_PLAYER1_CD_MANAGER + AE_PLAYER1_CD_MANAGER_LAST_SECTOR, sector);
    return 0;
}

void ae_ModelDrawWithOt(AeRam *r, const AePlayer1Hooks *hooks, uint32_t model,
                        uint32_t ordering_table, uint32_t draw_data, uint32_t flags) {
    uint32_t projection_state = AE_PLAYER1_RENDER_PROJECTION_STATE;
    ae_wr16(r, projection_state + (AE_PLAYER1_RENDER_PROJECTION_ZERO -
                                   AE_PLAYER1_RENDER_PROJECTION_OFFSET), 0);
    ae_wr16(r, projection_state + (AE_PLAYER1_RENDER_PROJECTION_FLAG -
                                   AE_PLAYER1_RENDER_PROJECTION_OFFSET), flags);
    ae_wr8(r, projection_state + (AE_PLAYER1_RENDER_PROJECTION_COMMAND -
                                  AE_PLAYER1_RENDER_PROJECTION_OFFSET), 6);
    ae_wr16(r, projection_state,
            ae_rd16(r, AE_PLAYER1_RENDER_CAMERA_SETTINGS + AE_PLAYER1_RENDER_CAMERA_H));
    ae_wr16(r, projection_state + 2u,
            ae_rd16(r, AE_PLAYER1_RENDER_CAMERA_SETTINGS + AE_PLAYER1_RENDER_CAMERA_DQA));

    uint32_t draw_header = ae_rd32(r, draw_data + AE_PLAYER1_MODEL_DRAW_HEADER);
    uint32_t vertex_count = ae_rd16(r, draw_header + AE_PLAYER1_MODEL_DRAW_VERTEX_COUNT);
    uint32_t vertices = ae_rd32(r, draw_data + AE_PLAYER1_MODEL_DRAW_VERTICES);
    if (vertex_count != 0) {
        (void)player1_call_hook(r, hooks,
                                hooks != NULL ? hooks->ModelProjectVertices : NULL,
                                "ModelProjectVertices", vertices, vertex_count,
                                AE_PLAYER1_RENDER_PROJECTION_OUTPUT,
                                AE_PLAYER1_RENDER_PROJECTION_STATE);
        if (ae_ram_failed(r)) return;
    }

    uint32_t emitter_data = ae_rd32(r, draw_data + AE_PLAYER1_MODEL_DRAW_EMIT_DATA);
    (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->ModelEmitPrims : NULL,
                            "ModelEmitPrims", model, emitter_data, ordering_table, 0);
}

void ae_ModelPrepCycleColors(AeRam *r, uint32_t cycle_records, int32_t count) {
    if (count <= 0) return;

    int32_t cycle_time = ae_rds32(r, AE_PLAYER1_MODEL_COLOR_CYCLE_TIME);
    uint32_t output = AE_PLAYER1_MODEL_COLOR_CYCLE_OUTPUT;
    for (int32_t record = 0; record < count; ++record) {
        uint32_t record_address = cycle_records +
                                 (uint32_t)record * AE_PLAYER1_MODEL_CYCLE_RECORD_STRIDE;
        int32_t phase = cycle_time +
                        ae_rds16(r, record_address + AE_PLAYER1_MODEL_CYCLE_RECORD_INDEX);
        int32_t remainder = phase - (phase / AE_PLAYER1_MODEL_CYCLE_COUNT) *
                                      AE_PLAYER1_MODEL_CYCLE_COUNT;
        uint32_t color = ae_rd32(r, AE_PLAYER1_MODEL_COLOR_CYCLE_SOURCE +
                                   (uint32_t)remainder * AE_PLAYER1_MODEL_CYCLE_OUTPUT_STRIDE);
        ae_wr32(r, output + (uint32_t)record * AE_PLAYER1_MODEL_CYCLE_OUTPUT_STRIDE, color);
    }
}

void ae_ModelEmitPrimsCycleColors(AeRam *r, const AePlayer1Hooks *hooks,
                                  uint32_t model_data, uint32_t cycle_table,
                                  uint32_t primitive_info, uint32_t flags) {
    /* The native renderer owns this GTE-driven primitive emitter and its packet cursor. */
    (void)player1_call_hook(r, hooks,
                            hooks != NULL ? hooks->ModelEmitPrimsCycleColors : NULL,
                            "ModelEmitPrimsCycleColors", model_data, cycle_table,
                            primitive_info, flags);
}

void ae_DrawRibbonBetween(AeRam *r, const AePlayer1Hooks *hooks,
                          uint32_t start, uint32_t end, int32_t scale, uint32_t color) {
    uint32_t normal = AE_PLAYER1_RIBBON_NORMAL_TEMP;
    uint32_t x_delta = ae_rd16(r, end + AE_PLAYER1_POINT_X) -
                       ae_rd16(r, start + AE_PLAYER1_POINT_X);
    uint32_t y_delta = ae_rd16(r, end + AE_PLAYER1_POINT_Y) -
                       ae_rd16(r, start + AE_PLAYER1_POINT_Y);
    ae_wr16(r, normal + AE_PLAYER1_VECTOR_X, x_delta);
    ae_wr16(r, normal + AE_PLAYER1_VECTOR_Y, y_delta);
    ae_wr16(r, normal + AE_PLAYER1_VECTOR_Z, 0);
    (void)ae_sdk_VectorNormalSS(r, normal, normal);
    if (ae_ram_failed(r)) return;

    uint32_t descriptor = AE_PLAYER1_RIBBON_DESCRIPTOR;
    ae_wr32(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_START, start);
    ae_wr32(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_END, end);
    ae_wr32(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_SCALE, (uint32_t)scale);
    ae_wr32(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_COLOR, color);
    ae_wr16(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_NORMAL_X,
            ae_rds16(r, normal + AE_PLAYER1_VECTOR_X));
    ae_wr16(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_NORMAL_Y,
            ae_rds16(r, normal + AE_PLAYER1_VECTOR_Y));
    ae_wr16(r, descriptor + AE_PLAYER1_RIBBON_DESCRIPTOR_NORMAL_Z,
            ae_rds16(r, normal + AE_PLAYER1_VECTOR_Z));

    (void)player1_call_hook(r, hooks, hooks != NULL ? hooks->DrawRibbonBetween : NULL,
                            "DrawRibbonBetween", descriptor, 0, 0, 0);
}
