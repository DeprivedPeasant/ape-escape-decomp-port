#include "../core/ae_missa.h"
#include "../core/ae_sdk.h"
#include "../player/ae_player_spawn.h"
#include "../player/ae_player1.h"
#include "../world/ae_areaset.h"
#include "../world/ae_world.h"

#define AE_MISSA_PLAYER_POSITION              0x800EC1E0u
#define AE_MISSA_PLAYER_POSITION_X            0x0024u
#define AE_MISSA_PLAYER_POSITION_Y            0x0028u
#define AE_MISSA_PLAYER_POSITION_Z            0x002Cu
#define AE_MISSA_PLAYER_POSITION_X_S16        0x001Cu
#define AE_MISSA_PLAYER_POSITION_Y_S16        0x001Eu
#define AE_MISSA_PLAYER_POSITION_Z_S16        0x0020u
#define AE_MISSA_PLAYER_FACING                 0x0060u
#define AE_MISSA_PLAYER_GROUND_HEIGHT          0x012Cu
#define AE_MISSA_PLAYER_ENTRY_POSITION_X       0x0000u
#define AE_MISSA_PLAYER_ENTRY_POSITION_Y       0x0004u
#define AE_MISSA_PLAYER_ENTRY_POSITION_Z       0x0008u
#define AE_MISSA_PLAYER_ENTRY_FACING_X         0x0018u
#define AE_MISSA_PLAYER_ENTRY_FACING_Y         0x001Cu
#define AE_MISSA_PLAYER_ENTRY_FACING_Z         0x0020u
#define AE_MISSA_PLAYER_ENTRY_TEMP_OFFSET      0x0010u
#define AE_MISSA_PLAYER_ENTRY_FRAME_SIZE       0x0050u
#define AE_MISSA_AREA_ENTRY_ZERO_AREA          61
#define AE_MISSA_PLAYER_STATE_SETS              0x800F4DE0u
#define AE_MISSA_PLAYER_SETUP_FLAG              0x800F4DC4u
#define AE_MISSA_PLAYER_PARAMS                  0x800EC010u
#define AE_MISSA_PLAYER_AUX_MODEL_BASE          0x800E07D0u
#define AE_MISSA_PLAYER_STATIC_MODELS_A          0x800EC060u
#define AE_MISSA_PLAYER_STATIC_MODELS_B          0x800F4C30u
#define AE_MISSA_PLAYER_STATIC_MODELS_C          0x800F4630u
#define AE_MISSA_PLAYER_STAGE_MODELS_A           0x01FCu
#define AE_MISSA_PLAYER_STAGE_MODELS_B           0x0200u
#define AE_MISSA_PLAYER_STAGE_MODELS_C           0x0204u
#define AE_MISSA_PLAYER_STAGE_MODEL_AUX_COUNT    0x023Cu
#define AE_MISSA_PLAYER_STAGE_FIRST_MODEL        0x019Cu
#define AE_MISSA_PLAYER_STAGE_IDLE_MODEL         0x01A0u
#define AE_MISSA_PLAYER_STATE_SET_STRIDE         0x0010u
#define AE_MISSA_PLAYER_MODEL_INSTANCE_STRIDE    0x0010u
#define AE_MISSA_PLAYER_MODEL_HEADER_OFFSET      0x0004u
#define AE_MISSA_PLAYER_PARAMS_WORD_COUNT        18u
#define AE_MISSA_TRAIL_RING_CURSOR               0x800B8AB0u
#define AE_MISSA_TRAIL_RING_RECORD_STRIDE        0x0014u
#define AE_MISSA_TRAIL_RING_POSITION_SOURCE      0x800F5154u
#define AE_MISSA_TRAIL_RING_POSITION_WORDS       4u
#define AE_MISSA_TRAIL_RING_POSITION_STRIDE      4u
#define AE_MISSA_TRAIL_RING_POSITION_OFFSET      0x0004u
#define AE_MISSA_TRAIL_RING_LIFE_OFFSET          0x0014u
#define AE_MISSA_TRAIL_RING_CAPACITY             12

#define AE_MISSA_PLAYER_STATE_MODEL_A            0x019Cu
#define AE_MISSA_PLAYER_STATE_MODEL_B            0x01FCu
#define AE_MISSA_PLAYER_STATE_MODEL_C            0x0200u
#define AE_MISSA_PLAYER_STATE_MODEL_D            0x0204u
#define AE_MISSA_PLAYER_STATE_MODEL_IDLE         0x01A0u

static const uint16_t missa_player_state_header_fields[5] = {
    0x0004u, 0x0008u, 0x000Cu, 0x0010u, 0x0014u
};
static const uint16_t missa_player_state_set_fields[4] = {
    0x0014u, 0x0018u, 0x001Cu, 0x0020u
};
static const uint32_t missa_player_parameter_values[AE_MISSA_PLAYER_PARAMS_WORD_COUNT] = {
    0u, 173u, (uint32_t)-1650, (uint32_t)-670, (uint32_t)-1680, 1280u,
    1u, 200u, 1920u, 1010u, 100u, 80u, 60u,
    (uint32_t)-1900, (uint32_t)-5550, (uint32_t)-4440, (uint32_t)-120, 2650u
};

static int32_t missa_player_hook(AeRam *r, const AeMissaHooks *hooks,
                                AeMissaHookFn fn, const char *missing,
                                uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (hooks == NULL || fn == NULL) {
        ae_ram_fail(r, "%s", missing);
        return 0;
    }
    return fn(hooks->user, r, a0, a1, a2, a3);
}

static int32_t missa_player_s16(int32_t value)
{
    return (int16_t)(uint16_t)value;
}

int32_t ae_PlayerPlaceAtEntry(AeRam *r, const AeMissaHooks *hooks,
                              int32_t speed, int32_t area, int32_t entry)
{
    int32_t area_index = missa_player_s16(area);
    int32_t entry_index = missa_player_s16(entry);
    uint32_t entry_sp = hooks != NULL && hooks->stack_pointer != 0u ?
                        hooks->stack_pointer : 0x801FFF00u;
    uint32_t entry_record = entry_sp - AE_MISSA_PLAYER_ENTRY_FRAME_SIZE +
                            AE_MISSA_PLAYER_ENTRY_TEMP_OFFSET;

    if (area_index == AE_MISSA_AREA_ENTRY_ZERO_AREA) entry_index = 0;
    (void)ae_AreaGetEntryPoint(r, area_index, entry_index, entry_record);
    if (ae_ram_failed(r)) return 0;

    uint32_t player = AE_MISSA_PLAYER_POSITION;
    int32_t x = ae_rds32(r, entry_record + AE_MISSA_PLAYER_ENTRY_POSITION_X);
    int32_t y = ae_rds32(r, entry_record + AE_MISSA_PLAYER_ENTRY_POSITION_Y);
    int32_t z = ae_rds32(r, entry_record + AE_MISSA_PLAYER_ENTRY_POSITION_Z);
    ae_wr32(r, player + AE_MISSA_PLAYER_POSITION_X, (uint32_t)x);
    ae_wr16(r, player + AE_MISSA_PLAYER_POSITION_X_S16, (uint32_t)x);
    ae_wr32(r, player + AE_MISSA_PLAYER_POSITION_Z, (uint32_t)z);
    ae_wr16(r, player + AE_MISSA_PLAYER_POSITION_Z_S16, (uint32_t)z);

    int32_t ground = ae_GetGroundHeight(r, hooks != NULL ? hooks->world : NULL,
                                        x, y, z);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, player + AE_MISSA_PLAYER_POSITION_Y, (uint32_t)ground);
    ae_wr16(r, player + AE_MISSA_PLAYER_POSITION_Y_S16, (uint32_t)ground);
    ae_wr32(r, player + AE_MISSA_PLAYER_GROUND_HEIGHT, (uint32_t)ground);

    int32_t facing_speed = missa_player_s16(speed);
    uint32_t direction = entry_sp - 24u;
    int32_t facing_x = ae_rds32(r, entry_record + AE_MISSA_PLAYER_ENTRY_FACING_X);
    int32_t facing_y = ae_rds32(r, entry_record + AE_MISSA_PLAYER_ENTRY_FACING_Y);
    int32_t facing_z = ae_rds32(r, entry_record + AE_MISSA_PLAYER_ENTRY_FACING_Z);
    ae_wr16(r, direction + 0u, (uint32_t)facing_speed * (uint32_t)facing_x);
    ae_wr16(r, direction + 2u, (uint32_t)facing_speed * (uint32_t)facing_y);
    ae_wr16(r, direction + 4u, (uint32_t)facing_speed * (uint32_t)facing_z);
    (void)ae_sdk_VectorNormalSS(r, direction, direction);
    if (ae_ram_failed(r)) return 0;

    ae_wr16(r, player + AE_MISSA_PLAYER_FACING + 0u, ae_rd16(r, direction + 0u));
    ae_wr16(r, player + AE_MISSA_PLAYER_FACING + 2u, ae_rd16(r, direction + 2u));
    ae_wr16(r, player + AE_MISSA_PLAYER_FACING + 4u, ae_rd16(r, direction + 4u));
    return 0;
}

static uint32_t missa_player_model_header(AeRam *r, uint32_t model_table,
                                         uint32_t index)
{
    uint32_t record = model_table + index * 4u;
    uint32_t word_offset = ae_rd16(r, record + AE_MISSA_PLAYER_MODEL_HEADER_OFFSET);
    return model_table + word_offset * 4u;
}

static void missa_player_static_model(AeRam *r, const AeMissaHooks *hooks,
                                      uint32_t instance, uint32_t header)
{
    (void)hooks;
    ae_StaticModelInit(r, NULL, instance, header);
}

void ae_PlayerInit(AeRam *r, const AeMissaHooks *hooks)
{
    const uint32_t member = 0x80103000u;
    uint32_t stage = 0x800F4350u;

    ae_wr32(r, stage + AE_MISSA_PLAYER_STAGE_FIRST_MODEL,
            member + (uint32_t)ae_rd16(r, member + missa_player_state_header_fields[0]) * 4u);
    ae_wr32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_A,
            member + (uint32_t)ae_rd16(r, member + missa_player_state_header_fields[1]) * 4u);
    ae_wr32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_B,
            member + (uint32_t)ae_rd16(r, member + missa_player_state_header_fields[2]) * 4u);
    ae_wr32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_C,
            member + (uint32_t)ae_rd16(r, member + missa_player_state_header_fields[3]) * 4u);
    ae_wr32(r, stage + AE_MISSA_PLAYER_STAGE_IDLE_MODEL,
            member + (uint32_t)ae_rd16(r, member + missa_player_state_header_fields[4]) * 4u);

    uint32_t set_table = member;
    for (uint32_t set = 0; set < 10u; ++set) {
        uint32_t set_record = set_table + set * AE_MISSA_PLAYER_STATE_SET_STRIDE;
        uint32_t output = AE_MISSA_PLAYER_STATE_SETS + set * AE_MISSA_PLAYER_STATE_SET_STRIDE;
        for (uint32_t field = 0; field < 4u; ++field) {
            uint32_t word_offset = ae_rd16(r, set_record + missa_player_state_set_fields[field]);
            ae_wr32(r, output + field * 4u, member + word_offset * 4u);
        }
    }
    ae_PlayerIdleModelSetup(r, 0u);

    uint32_t player = AE_MISSA_PLAYER_POSITION;
    uint32_t first_model = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_FIRST_MODEL);
    ae_wr16(r, stage + 0x023Eu, 6u);
    ae_wr32(r, player + 0x0018u,
            ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_IDLE_MODEL));
    ae_ModelInitNormals(r, player, first_model);
    ae_wr16(r, player + 0x0066u, 1u);
    ae_wr16(r, player + 0x0046u, 256u);
    ae_wr16(r, player + 0x00E8u, 5u);
    ae_wr16(r, AE_MISSA_PLAYER_SETUP_FLAG, 0u);
    ae_wr16(r, player + 0x008Eu, 4096u);

    static const uint16_t cleared_word_fields[] = {
        0x0030u, 0x0034u, 0x0038u, 0x0120u, 0x0124u, 0x0128u,
        0x0114u, 0x0118u, 0x011Cu, 0x0050u, 0x00E0u, 0x012Cu, 0x0138u, 0x0070u
    };
    for (uint32_t i = 0; i < sizeof(cleared_word_fields) / sizeof(cleared_word_fields[0]); ++i)
        ae_wr32(r, player + cleared_word_fields[i], 0u);
    ae_wr16(r, player + 0x00ECu, 0u);
    ae_wr16(r, player + 0x0148u, 0u);
    ae_wr16(r, player + 0x00FEu, 0u);
    ae_wr32(r, player + 0x00F4u, 0x00FFFFFFu);
    ae_wr32(r, player + 0x00F8u, 0x00FFFFFFu);
    for (uint32_t i = 0; i < AE_MISSA_PLAYER_PARAMS_WORD_COUNT; ++i)
        ae_wr32(r, AE_MISSA_PLAYER_PARAMS + i * 4u, missa_player_parameter_values[i]);

    uint32_t model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_A);
    uint32_t first_model_count =
        (ae_rd16(r, model_table) - 1u) & 0xFFFFu;
    ae_wr16(r, stage + AE_MISSA_PLAYER_STAGE_MODEL_AUX_COUNT, first_model_count);
    uint32_t index = 0;
    /* 800329B8 reloads the halfword after StaticModelInit. Model instances
     * can overlap this field when the supplied member table is empty. */
    for (; index < ae_rd16(r, stage + AE_MISSA_PLAYER_STAGE_MODEL_AUX_COUNT); ++index) {
        model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_A);
        missa_player_static_model(r, hooks,
            AE_MISSA_PLAYER_STATIC_MODELS_A + index * AE_MISSA_PLAYER_MODEL_INSTANCE_STRIDE,
            missa_player_model_header(r, model_table, index));
    }
    model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_A);
    uint32_t auxiliary_model = missa_player_model_header(r, model_table, index);
    (void)missa_player_hook(r, hooks,
        hooks != NULL ? hooks->PlayerAuxModelInit : NULL,
        "PlayerInit: PlayerAuxModelInit hook missing", auxiliary_model, 0u, 0u, 0u);
    if (ae_ram_failed(r)) return;

    model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_B);
    int32_t model_count = ae_rds32(r, model_table);
    for (int32_t index = 0; index < model_count; ++index) {
        model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_B);
        missa_player_static_model(r, hooks,
            AE_MISSA_PLAYER_STATIC_MODELS_B + (uint32_t)index * AE_MISSA_PLAYER_MODEL_INSTANCE_STRIDE,
            missa_player_model_header(r, model_table, (uint32_t)index));
    }

    model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_C);
    model_count = ae_rds32(r, model_table);
    for (int32_t index = 0; index < model_count; ++index) {
        model_table = ae_rd32(r, stage + AE_MISSA_PLAYER_STAGE_MODELS_C);
        missa_player_static_model(r, hooks,
            AE_MISSA_PLAYER_STATIC_MODELS_C + (uint32_t)index * AE_MISSA_PLAYER_MODEL_INSTANCE_STRIDE,
            missa_player_model_header(r, model_table, (uint32_t)index));
    }
}

void ae_TrailRingPush(AeRam *r)
{
    uint32_t next_value = ae_rd32(r, AE_MISSA_TRAIL_RING_CURSOR) + 1u;
    int32_t signed_next = (int32_t)next_value;
    int32_t ring_index = signed_next % AE_MISSA_TRAIL_RING_CAPACITY;
    uint32_t unsigned_index = (uint32_t)ring_index;
    uint32_t record = AE_MISSA_TRAIL_RING_CURSOR +
                      unsigned_index * AE_MISSA_TRAIL_RING_RECORD_STRIDE;

    for (uint32_t word = 0; word < AE_MISSA_TRAIL_RING_POSITION_WORDS; ++word) {
        ae_wr32(r, record + AE_MISSA_TRAIL_RING_POSITION_OFFSET +
                    word * AE_MISSA_TRAIL_RING_POSITION_STRIDE,
                ae_rd32(r, AE_MISSA_TRAIL_RING_POSITION_SOURCE +
                            word * AE_MISSA_TRAIL_RING_POSITION_STRIDE));
    }
    ae_wr32(r, record + AE_MISSA_TRAIL_RING_LIFE_OFFSET, 0xFFu);
    ae_wr32(r, AE_MISSA_TRAIL_RING_CURSOR, unsigned_index);
}
