/* Animation-event hit bodies and dispatch for SCUS_944.23. */
#include "ae_anim_events.h"

#include "../core/ae_sdk.h"

#include <stdint.h>

/* ---- original globals ---------------------------------------------------------------------- */
#define AE_G_PLAYER                         0x800EC1E0u
#define AE_G_BONE_WORLD_MATRICES            0x800F4730u
#define AE_G_PLAYER_EVENT_FUNCTIONS         0x800B027Cu
#define AE_G_PLAYER_ACTION_FUNCTIONS         0x800B03ACu
#define AE_G_PLAYER_ANIM_EVENT_DATA          0x800F5120u
#define AE_G_PLAYER_EVENT_FIXED_POSITION    0x800F5180u
#define AE_G_PLAYER_EVENT_SKIP_MODE         0x800F44A2u

/* ---- Player fields -------------------------------------------------------------------------- */
#define AE_PLAYER_POS                       0x024u
#define AE_PLAYER_FACING                    0x060u
#define AE_PLAYER_ANIM_FRAME                0x05Cu
#define AE_PLAYER_STATE                     0x05Eu
#define AE_PLAYER_ANIM_LERP                 0x08Au
#define AE_PLAYER_HEALTH                    0x0E8u
#define AE_PLAYER_EVENT_POINT_A              0x034u
#define AE_PLAYER_EVENT_POINT_B              0x03Cu
#define AE_PLAYER_EVENT_BODY_A               0x04Cu
#define AE_PLAYER_EVENT_BODY_B               0x050u

/* ---- CollisionBody fields ------------------------------------------------------------------- */
#define AE_BODY_POS                         0x000u
#define AE_BODY_FACING                      0x018u
#define AE_BODY_RADIUS                      0x020u
#define AE_BODY_PARAM_A                     0x024u
#define AE_BODY_PARAM_B                     0x026u

/* ---- PlayerStateEntry / animation-event record fields --------------------------------------- */
#define AE_PLAYER_STATE_ENTRY_SIZE          0x08u
#define AE_PLAYER_STATE_FIRST_FRAME         0x00u
#define AE_ANIM_EVENT_STATE                 0x00u
#define AE_ANIM_EVENT_FRAME                 0x01u
#define AE_ANIM_EVENT_KIND                  0x02u
#define AE_ANIM_EVENT_ARGUMENT              0x03u
#define AE_ANIM_EVENT_PAYLOAD               0x04u
#define AE_ANIM_EVENT_HEADER_SIZE           0x04u
#define AE_ANIM_EVENT_BONE_X                0x00u
#define AE_ANIM_EVENT_BONE_Y                0x01u
#define AE_ANIM_EVENT_BONE_Z                0x02u
#define AE_ANIM_EVENT_BONE_RADIUS           0x03u
#define AE_ANIM_EVENT_ANGLE_X               0x04u
#define AE_ANIM_EVENT_ANGLE_Y               0x05u
#define AE_ANIM_EVENT_BODY_PARAM_A          0x08u
#define AE_ANIM_EVENT_BODY_PARAM_B          0x0Au
#define AE_ANIM_EVENT_LONG_RECORD_SIZE      0x10u
#define AE_ANIM_EVENT_SHORT_RECORD_SIZE     0x08u
#define AE_ANIM_EVENT_SIMPLE_RECORD_SIZE    0x04u
#define AE_ANIM_EVENT_TERMINATOR            0xFFu

/* ---- event kinds and collision-body types --------------------------------------------------- */
#define AE_ANIM_EVENT_SFX                   1u
#define AE_ANIM_EVENT_BONE_BODY              2u
#define AE_ANIM_EVENT_PLAYER_BONE_BODY       3u
#define AE_ANIM_EVENT_FUNCTION              4u
#define AE_ANIM_EVENT_ACTION                5u
#define AE_ANIM_EVENT_ADD_HIT_BODIES         6u
#define AE_ANIM_EVENT_FIXED_POSITION_BODY    7u
#define AE_ANIM_EVENT_VOICE                 8u
#define AE_ANIM_EVENT_FIRST_FRAME_MASK      0x7Fu
#define AE_ANIM_EVENT_KIND_COUNT            8u
#define AE_ANIM_EVENT_SKIP_BODY_MODE        11
#define AE_ANIM_EVENT_DEFAULT_BODY_HEIGHT   0x100u
#define AE_BODY_TYPE_ANIM_EVENT              6
#define AE_BODY_TYPE_PLAYER_EVENT            8

/* These temporaries occupy the same excluded top-of-RAM stack window as the original function. */
#define AE_ANIM_EVENT_STACK_VECTOR           0x801FFF80u

static int32_t add32(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

static int32_t arithmetic_shift_right(int32_t value, uint32_t amount)
{
    amount &= 31u;
    if (amount == 0u) return value;
    if (value >= 0) return (int32_t)((uint32_t)value >> amount);
    return (int32_t)~((~(uint32_t)value) >> amount);
}

static int32_t signed_byte_times_eight(AeRam *r, uint32_t address)
{
    int32_t value = ae_rds8(r, address);
    return (int32_t)((uint32_t)value << 3);
}

static int32_t round_q12_toward_zero(int32_t value)
{
    if (value < 0) value = add32(value, 4095);
    return arithmetic_shift_right(value, 12u);
}

static int missing_event_hook(AeRam *r, const char *name)
{
    ae_ram_fail(r, "AeAnimEventHooks.%s not provided", name);
    return 0;
}

static void copy_player_facing(AeRam *r, uint32_t body)
{
    for (uint32_t offset = 0; offset < 8u; offset += 4u) {
        uint32_t facing_word = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_FACING + offset);
        ae_wr32(r, body + AE_BODY_FACING + offset, facing_word);
        if (ae_ram_failed(r)) return;
    }
}

static int body_register(AeRam *r, const AeBodyHooks *hooks, int32_t type, uint32_t body)
{
    (void)ae_CollisionBodyRegister(r, hooks, type, body, AE_G_PLAYER);
    return !ae_ram_failed(r);
}

static int interpolate_hit_body_position(AeRam *r, int32_t position[3], uint32_t point_a_weight,
                                         uint32_t point_b_weight)
{
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        int32_t point_a = ae_rds16(r, AE_G_PLAYER_ANIM_EVENT_DATA + AE_PLAYER_EVENT_POINT_A + axis * 2u);
        int32_t point_b = ae_rds16(r, AE_G_PLAYER_ANIM_EVENT_DATA + AE_PLAYER_EVENT_POINT_B + axis * 2u);
        int32_t weighted_a = (int32_t)((uint32_t)point_a * point_a_weight);
        int32_t weighted_b = (int32_t)((uint32_t)point_b * point_b_weight);
        position[axis] = arithmetic_shift_right(add32(weighted_a, weighted_b), 3u);
        if (ae_ram_failed(r)) return 0;
    }
    return 1;
}

static int set_bone_offset(AeRam *r, uint32_t record)
{
    ae_wr16(r, AE_ANIM_EVENT_STACK_VECTOR + 0u,
            (uint32_t)signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_X));
    ae_wr16(r, AE_ANIM_EVENT_STACK_VECTOR + 2u,
            (uint32_t)signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_Y));
    ae_wr16(r, AE_ANIM_EVENT_STACK_VECTOR + 4u,
            (uint32_t)signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_Z));
    return !ae_ram_failed(r);
}

static int transform_bone_offset(AeRam *r)
{
    (void)ae_sdk_ApplyMatrixSV(r, AE_G_BONE_WORLD_MATRICES,
                              AE_ANIM_EVENT_STACK_VECTOR, AE_ANIM_EVENT_STACK_VECTOR);
    return !ae_ram_failed(r);
}

static int write_player_relative_body_position(AeRam *r, uint32_t body)
{
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        int32_t player_position = ae_rds32(r, AE_G_PLAYER + AE_PLAYER_POS + axis * 4u);
        int32_t bone_offset = ae_rds16(r, AE_ANIM_EVENT_STACK_VECTOR + axis * 2u);
        ae_wr32(r, body + AE_BODY_POS + axis * 4u, (uint32_t)add32(player_position, bone_offset));
        if (ae_ram_failed(r)) return 0;
    }
    return 1;
}

/* Event kind 2: orient a type-6 body from a bone-local position and two packed angles. */
static void add_oriented_event_body(AeRam *r, const AeBodyHooks *body_hooks, uint32_t record)
{
    uint32_t payload = record + AE_ANIM_EVENT_PAYLOAD;
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0u || ae_ram_failed(r)) return;

    if (!set_bone_offset(r, record) || !transform_bone_offset(r) ||
        !write_player_relative_body_position(r, body)) return;

    ae_wr16(r, body + AE_BODY_RADIUS,
            (uint32_t)signed_byte_times_eight(r, payload + AE_ANIM_EVENT_BONE_RADIUS));
    if (ae_ram_failed(r)) return;

    int32_t angle_x = (int32_t)(ae_rd8(r, payload + AE_ANIM_EVENT_ANGLE_X) << 4);
    int32_t angle_y = (int32_t)(ae_rd8(r, payload + AE_ANIM_EVENT_ANGLE_Y) << 4);
    int32_t facing_x = round_q12_toward_zero((int32_t)((uint32_t)ae_sdk_rsin(r, angle_x) *
                                                        (uint32_t)ae_sdk_rcos(r, angle_y)));
    int32_t facing_y = ae_sdk_rsin(r, angle_y);
    int32_t facing_z = round_q12_toward_zero((int32_t)(0u -
                        (uint32_t)((uint32_t)ae_sdk_rcos(r, angle_x) * (uint32_t)ae_sdk_rcos(r, angle_y))));
    ae_wr16(r, AE_ANIM_EVENT_STACK_VECTOR, (uint32_t)facing_x);
    ae_wr16(r, AE_ANIM_EVENT_STACK_VECTOR + 2u, (uint32_t)facing_y);
    ae_wr16(r, AE_ANIM_EVENT_STACK_VECTOR + 4u, (uint32_t)facing_z);
    if (ae_ram_failed(r)) return;
    (void)ae_sdk_ApplyMatrixSV(r, AE_G_BONE_WORLD_MATRICES,
                              AE_ANIM_EVENT_STACK_VECTOR, body + AE_BODY_FACING);
    if (ae_ram_failed(r)) return;

    ae_wr16(r, body + AE_BODY_PARAM_A, ae_rd16(r, payload + AE_ANIM_EVENT_BODY_PARAM_A));
    ae_wr16(r, body + AE_BODY_PARAM_B, ae_rd16(r, payload + AE_ANIM_EVENT_BODY_PARAM_B));
    if (ae_ram_failed(r)) return;
    (void)body_register(r, body_hooks, AE_BODY_TYPE_ANIM_EVENT, body);
}

/* Event kind 3: create the Monkey Net catch body at the selected bone offset. */
static void add_player_bone_body(AeRam *r, const AeBodyHooks *body_hooks, uint32_t record)
{
    if ((int32_t)ae_rd16(r, AE_G_PLAYER_EVENT_SKIP_MODE) == AE_ANIM_EVENT_SKIP_BODY_MODE ||
        ae_rds16(r, AE_G_PLAYER + AE_PLAYER_HEALTH) <= 0 || ae_ram_failed(r)) return;

    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0u || ae_ram_failed(r)) return;
    if (!set_bone_offset(r, record) || !transform_bone_offset(r) ||
        !write_player_relative_body_position(r, body)) return;

    copy_player_facing(r, body);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, body + AE_BODY_RADIUS,
            (uint32_t)signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_RADIUS));
    if (ae_ram_failed(r)) return;

    (void)body_register(r, body_hooks, AE_BODY_TYPE_PLAYER_EVENT, body);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, body + AE_BODY_PARAM_A, AE_ANIM_EVENT_DEFAULT_BODY_HEIGHT);
    ae_wr16(r, body + AE_BODY_PARAM_B, 0u);
}

/* Event kind 7: create a type-8 body at the position prepared by the player animation code. */
static void add_fixed_position_body(AeRam *r, const AeBodyHooks *body_hooks, uint32_t record)
{
    if ((int32_t)ae_rd16(r, AE_G_PLAYER_EVENT_SKIP_MODE) == AE_ANIM_EVENT_SKIP_BODY_MODE ||
        ae_rds16(r, AE_G_PLAYER + AE_PLAYER_HEALTH) <= 0 || ae_ram_failed(r)) return;

    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0u || ae_ram_failed(r)) return;
    copy_player_facing(r, body);
    if (ae_ram_failed(r)) return;

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        int32_t position = ae_rds16(r, AE_G_PLAYER_EVENT_FIXED_POSITION + axis * 2u);
        ae_wr32(r, body + AE_BODY_POS + axis * 4u, (uint32_t)position);
        if (ae_ram_failed(r)) return;
    }
    ae_wr16(r, body + AE_BODY_RADIUS,
            (uint32_t)signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_RADIUS));
    if (ae_ram_failed(r)) return;

    (void)body_register(r, body_hooks, AE_BODY_TYPE_PLAYER_EVENT, body);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, body + AE_BODY_PARAM_A, AE_ANIM_EVENT_DEFAULT_BODY_HEIGHT);
    ae_wr16(r, body + AE_BODY_PARAM_B, 0u);
}

static void call_event_hook(AeRam *r, const AeAnimEventHooks *hooks, uint32_t table,
                            uint32_t index, const char *hook_name)
{
    uint32_t function = ae_rd32(r, table + index * 4u);
    if (ae_ram_failed(r)) return;
    if (hooks == NULL || hooks->event_callback == NULL) {
        (void)missing_event_hook(r, hook_name);
        return;
    }
    hooks->event_callback(hooks->user, r, function);
}

void ae_PlayerRunAnimEvents(AeRam *r, const AeBodyHooks *body_hooks, const AeAnimEventHooks *event_hooks,
                            uint32_t state_table, uint32_t events)
{
    uint32_t record = events;
    while (!ae_ram_failed(r)) {
        uint32_t state = ae_rd8(r, record + AE_ANIM_EVENT_STATE);
        if (ae_ram_failed(r)) return;
        if (state == AE_ANIM_EVENT_TERMINATOR) return;

        uint32_t player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
        if (ae_ram_failed(r)) return;
        if (state == player_state) {
            uint32_t relative_frame = ae_rd8(r, record + AE_ANIM_EVENT_FRAME);
            uint32_t state_index = state & AE_ANIM_EVENT_FIRST_FRAME_MASK;
            uint32_t state_entry = state_table + state_index * AE_PLAYER_STATE_ENTRY_SIZE;
            uint32_t first_frame = ae_rd16(r, state_entry + AE_PLAYER_STATE_FIRST_FRAME);
            uint32_t player_frame = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_ANIM_FRAME);
            uint32_t anim_lerp = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_ANIM_LERP);
            if (ae_ram_failed(r)) return;

            if (relative_frame + first_frame == player_frame && anim_lerp == 0u) {
                uint32_t kind = ae_rd8(r, record + AE_ANIM_EVENT_KIND);
                uint32_t argument = ae_rd8(r, record + AE_ANIM_EVENT_ARGUMENT);
                if (ae_ram_failed(r)) return;
                switch (kind) {
                case AE_ANIM_EVENT_SFX:
                    if (event_hooks == NULL || event_hooks->play_sfx == NULL) {
                        (void)missing_event_hook(r, "play_sfx");
                        return;
                    }
                    event_hooks->play_sfx(event_hooks->user, r, (int32_t)argument);
                    break;
                case AE_ANIM_EVENT_BONE_BODY:
                    add_oriented_event_body(r, body_hooks, record);
                    break;
                case AE_ANIM_EVENT_PLAYER_BONE_BODY:
                    add_player_bone_body(r, body_hooks, record);
                    break;
                case AE_ANIM_EVENT_FUNCTION:
                    call_event_hook(r, event_hooks, AE_G_PLAYER_EVENT_FUNCTIONS, argument, "event_callback");
                    break;
                case AE_ANIM_EVENT_ACTION:
                    call_event_hook(r, event_hooks, AE_G_PLAYER_ACTION_FUNCTIONS, argument, "event_callback");
                    break;
                case AE_ANIM_EVENT_ADD_HIT_BODIES: {
                    int32_t radius = signed_byte_times_eight(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BONE_RADIUS);
                    uint32_t param_a = ae_rd16(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BODY_PARAM_A);
                    uint32_t param_b = ae_rd16(r, record + AE_ANIM_EVENT_PAYLOAD + AE_ANIM_EVENT_BODY_PARAM_B);
                    if (ae_ram_failed(r)) return;
                    ae_PlayerAnimEventAddHitBodies(r, body_hooks, 0u, radius, param_a, param_b);
                    break;
                }
                case AE_ANIM_EVENT_FIXED_POSITION_BODY:
                    add_fixed_position_body(r, body_hooks, record);
                    break;
                case AE_ANIM_EVENT_VOICE:
                    if (event_hooks == NULL || event_hooks->play_player_voice == NULL) {
                        (void)missing_event_hook(r, "play_player_voice");
                        return;
                    }
                    event_hooks->play_player_voice(event_hooks->user, r, (int32_t)argument);
                    break;
                default:
                    break;
                }
                if (ae_ram_failed(r)) return;
            }
        }

        uint32_t kind = ae_rd8(r, record + AE_ANIM_EVENT_KIND);
        if (ae_ram_failed(r)) return;
        if (kind == AE_ANIM_EVENT_BONE_BODY || kind == AE_ANIM_EVENT_ADD_HIT_BODIES)
            record = (uint32_t)(record + AE_ANIM_EVENT_LONG_RECORD_SIZE);
        else if (kind == AE_ANIM_EVENT_PLAYER_BONE_BODY || kind == AE_ANIM_EVENT_FIXED_POSITION_BODY)
            record = (uint32_t)(record + AE_ANIM_EVENT_SHORT_RECORD_SIZE);
        else
            record = (uint32_t)(record + AE_ANIM_EVENT_SIMPLE_RECORD_SIZE);
    }
}

void ae_PlayerAnimEventAddHitBodies(AeRam *r, const AeBodyHooks *body_hooks, uint32_t unused,
                                    int32_t radius, uint32_t param_a, uint32_t param_b)
{
    (void)unused;

    uint32_t first_body = ae_CollisionBodyAlloc(r);
    if (first_body != 0u && !ae_ram_failed(r)) {
        int32_t first[3];
        if (!interpolate_hit_body_position(r, first, 1u, 7u)) return;
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            ae_wr32(r, first_body + AE_BODY_POS + axis * 4u, (uint32_t)first[axis]);
            if (ae_ram_failed(r)) return;
        }
        ae_wr16(r, first_body + AE_BODY_RADIUS, (uint32_t)add32(radius, 0x20));
        copy_player_facing(r, first_body);
        ae_wr16(r, first_body + AE_BODY_PARAM_A, param_a);
        ae_wr16(r, first_body + AE_BODY_PARAM_B, param_b);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, AE_G_PLAYER_ANIM_EVENT_DATA + AE_PLAYER_EVENT_BODY_A, first_body);
        if (ae_ram_failed(r)) return;
        (void)body_register(r, body_hooks, AE_BODY_TYPE_ANIM_EVENT, first_body);
        if (ae_ram_failed(r)) return;

        uint32_t second_body = ae_CollisionBodyAlloc(r);
        if (second_body == 0u || ae_ram_failed(r)) return;
        int32_t second[3];
        if (!interpolate_hit_body_position(r, second, 5u, 3u)) return;
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            ae_wr32(r, second_body + AE_BODY_POS + axis * 4u, (uint32_t)second[axis]);
            if (ae_ram_failed(r)) return;
        }
        ae_wr16(r, second_body + AE_BODY_RADIUS, (uint32_t)radius);
        copy_player_facing(r, second_body);
        ae_wr16(r, second_body + AE_BODY_PARAM_A, param_a);
        ae_wr16(r, second_body + AE_BODY_PARAM_B, param_b);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, AE_G_PLAYER_ANIM_EVENT_DATA + AE_PLAYER_EVENT_BODY_B, second_body);
        (void)body_register(r, body_hooks, AE_BODY_TYPE_ANIM_EVENT, second_body);
    }
}
