/* Readable RAM-image ports of player animation, draw-state, and Monkey Radar routines. */
#include "ae_animdraw.h"

#include "../core/ae_pose.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/ae_gadget.h"
#include "../player/ae_player1.h"
#include "../player/ae_player2.h"
#include "../player/native_player.h"
#include "../world/ae_items_init.h"
#include "../world/ae_zako_init.h"

#include <stdint.h>

static int32_t animdraw_sra(int32_t value, uint32_t shift);

static int32_t sign_extend_bits(uint32_t value, uint32_t shift, uint32_t width)
{
    uint32_t mask = (1u << width) - 1u;
    uint32_t component = (value >> shift) & mask;
    uint32_t sign = 1u << (width - 1u);
    return (component & sign) != 0u ? (int32_t)component - (int32_t)(1u << width) : (int32_t)component;
}

static int32_t round_shift_12(int32_t value)
{
    if (value < 0) value = (int32_t)((uint32_t)value + 4095u);
    return value >> 12;
}

static void write_vector(AeRam *r, uint32_t address, int32_t x, int32_t y, int32_t z)
{
    ae_wr16(r, address + AE_ANIMDRAW_VECTOR_X, (uint32_t)x);
    ae_wr16(r, address + AE_ANIMDRAW_VECTOR_Y, (uint32_t)y);
    ae_wr16(r, address + AE_ANIMDRAW_VECTOR_Z, (uint32_t)z);
}

static void read_vector(AeRam *r, uint32_t address, int32_t *x, int32_t *y, int32_t *z)
{
    *x = ae_rds16(r, address + AE_ANIMDRAW_VECTOR_X);
    *y = ae_rds16(r, address + AE_ANIMDRAW_VECTOR_Y);
    *z = ae_rds16(r, address + AE_ANIMDRAW_VECTOR_Z);
}

static void cross_product(AeRam *r, uint32_t out, uint32_t left, uint32_t right)
{
    int32_t lx, ly, lz;
    int32_t rx, ry, rz;
    read_vector(r, left, &lx, &ly, &lz);
    read_vector(r, right, &rx, &ry, &rz);
    write_vector(r, out,
                 round_shift_12(ly * rz - lz * ry),
                 round_shift_12(lz * rx - lx * rz),
                 round_shift_12(lx * ry - ly * rx));
}

static int32_t animdraw_call(AeRam *r, const AeAnimDrawHooks *hooks,
                             AeAnimDrawCallHook callback, const char *name,
                             uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "animdraw hook %s not provided", name);
        return 0;
    }
    int32_t result = callback(hooks->user, r, a0, a1, a2, a3);
    return ae_ram_failed(r) ? 0 : result;
}

static int32_t animdraw_call5(AeRam *r, const AeAnimDrawHooks *hooks,
                              AeAnimDrawCall5Hook callback, const char *name,
                              uint32_t a0, uint32_t a1, uint32_t a2,
                              uint32_t a3, uint32_t a4)
{
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "animdraw hook %s not provided", name);
        return 0;
    }
    int32_t result = callback(hooks->user, r, a0, a1, a2, a3, a4);
    return ae_ram_failed(r) ? 0 : result;
}

int32_t ae_StepToward(AeRam *r, int32_t current, int32_t target, int32_t step)
{
    (void)r;
    int32_t difference = (int32_t)((uint32_t)current - (uint32_t)target);
    if (difference == 0) return current;

    int32_t directed_step = difference < step ? (int32_t)(0u - (uint32_t)step) : 0;
    if (directed_step == 0) return (int32_t)((uint32_t)current - (uint32_t)step);
    if (directed_step < difference) return target;
    return (int32_t)((uint32_t)current + (uint32_t)step);
}

int32_t ae_IntSqrt(AeRam *r, int32_t value)
{
    (void)r;
    if (value <= 0) return 0;

    uint32_t remaining = (uint32_t)value;
    int32_t leading_zeroes = 0;
    for (uint32_t bit = 0x80000000u; bit != 0u && (remaining & bit) == 0u; bit >>= 1) {
        ++leading_zeroes;
    }

    int32_t digit_pairs = ((33 - leading_zeroes) >> 1) + 1;
    uint32_t partial_root = 0;
    uint32_t remainder = 0;
    for (int32_t pair = 0; pair < digit_pairs; ++pair) {
        partial_root <<= 1;
        uint32_t shift = (uint32_t)((digit_pairs - pair - 2) * 2) & 31u;
        remainder = (remainder << 2) | ((remaining >> shift) & 3u);
        uint32_t trial = (partial_root << 1) | 1u;
        if (remainder >= trial) {
            remainder -= trial;
            ++partial_root;
        }
    }

    return (int32_t)((partial_root >> 1) + (partial_root & 1u));
}

void ae_MatrixFromFrontUp(AeRam *r, uint32_t front, uint32_t up,
                          uint32_t matrix, uint32_t mode)
{
    ae_sdk_VectorNormalSS(r, front, AE_ANIMDRAW_TMP_FRONT);
    if (ae_ram_failed(r)) return;
    ae_sdk_VectorNormalSS(r, up, AE_ANIMDRAW_TMP_UP);
    if (ae_ram_failed(r)) return;

    /* The EXE builds its third basis vector as front x up, in 12-bit fixed point. */
    cross_product(r, AE_ANIMDRAW_TMP_CROSS, AE_ANIMDRAW_TMP_FRONT, AE_ANIMDRAW_TMP_UP);
    if (ae_ram_failed(r)) return;
    ae_sdk_VectorNormalSS(r, AE_ANIMDRAW_TMP_CROSS, AE_ANIMDRAW_TMP_CROSS);
    if (ae_ram_failed(r)) return;

    uint32_t second = AE_ANIMDRAW_TMP_UP;
    int32_t front_x, front_y, front_z;
    read_vector(r, AE_ANIMDRAW_TMP_FRONT, &front_x, &front_y, &front_z);
    write_vector(r, AE_ANIMDRAW_TMP_FRONT, -front_x, -front_y, -front_z);
    uint32_t third = AE_ANIMDRAW_TMP_FRONT;
    if (mode == 0u) {
        cross_product(r, AE_ANIMDRAW_TMP_SIDE, AE_ANIMDRAW_TMP_CROSS, AE_ANIMDRAW_TMP_UP);
        third = AE_ANIMDRAW_TMP_SIDE;
    } else if (mode == 1u) {
        cross_product(r, AE_ANIMDRAW_TMP_SIDE, AE_ANIMDRAW_TMP_FRONT, AE_ANIMDRAW_TMP_CROSS);
        second = AE_ANIMDRAW_TMP_SIDE;
    }
    if (ae_ram_failed(r)) return;

    int32_t x, y, z;
    read_vector(r, AE_ANIMDRAW_TMP_CROSS, &x, &y, &z);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_00, (uint32_t)x);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_10, (uint32_t)y);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_20, (uint32_t)z);
    read_vector(r, second, &x, &y, &z);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_01, (uint32_t)x);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_11, (uint32_t)y);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_21, (uint32_t)z);
    read_vector(r, third, &x, &y, &z);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_02, (uint32_t)x);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_12, (uint32_t)y);
    ae_wr16(r, matrix + AE_ANIMDRAW_MATRIX_22, (uint32_t)z);
}

uint32_t ae_AnimFramePtr(AeRam *r, uint32_t actor, uint32_t frame)
{
    uint32_t model = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_MODEL);
    uint32_t bone_count = ae_rd16(r, model + AE_ANIMDRAW_MODEL_BONE_COUNT);
    uint32_t animation = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_ANIM_SET);
    uint32_t frame_stride = (bone_count + AE_ANIMDRAW_FRAME_HEADER_WORDS) *
                            AE_ANIMDRAW_ANIM_BONE_STRIDE;
    return animation + frame * frame_stride;
}

void ae_AnimDecodeFrame(AeRam *r, uint32_t actor)
{
    uint32_t model = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_MODEL);
    uint32_t bone_count = ae_rd16(r, model + AE_ANIMDRAW_MODEL_BONE_COUNT);
    uint32_t animation = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_ANIM_SET);
    uint32_t frame_index = ae_rd16(r, actor + AE_ANIMDRAW_PLAYER_ANIM_FRAME);
    uint32_t frame = animation + frame_index *
                     ((bone_count + AE_ANIMDRAW_FRAME_HEADER_WORDS) * AE_ANIMDRAW_ANIM_BONE_STRIDE);
    uint32_t pose = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_POSE);
    if (ae_ram_failed(r)) return;

    for (uint32_t bone = 0; bone < bone_count; ++bone) {
        uint32_t packed = ae_rd32(r, frame + AE_ANIMDRAW_ANIM_BONE_WORDS +
                                  bone * AE_ANIMDRAW_ANIM_BONE_STRIDE);
        uint32_t angles = AE_ANIMDRAW_PLAYER_ANGLES + bone * AE_ANIMDRAW_ANGLE_BONE_STRIDE;
        ae_wr16(r, angles, (uint32_t)(sign_extend_bits(packed, 0, 10) * 4));
        ae_wr16(r, angles + AE_ANIMDRAW_ANGLE_COMPONENT_Y,
                (uint32_t)(sign_extend_bits(packed, 10, 12) * 4));
        ae_wr16(r, angles + AE_ANIMDRAW_ANGLE_COMPONENT_Z,
                (uint32_t)(sign_extend_bits(packed, 22, 10) * 4));
        if (ae_ram_failed(r)) return;
    }

    uint32_t root = ae_rd32(r, frame + AE_ANIMDRAW_ANIM_ROOT_WORD);
    ae_wr16(r, pose + AE_ANIMDRAW_POSE_ROOT_X, (uint32_t)sign_extend_bits(root, 0, 10));
    ae_wr16(r, pose + AE_ANIMDRAW_POSE_ROOT_Z, (uint32_t)sign_extend_bits(root, 22, 10));
    ae_wr16(r, pose + AE_ANIMDRAW_POSE_ROOT_Y, (uint32_t)sign_extend_bits(root, 10, 12));
}

void ae_AnimDecodeLerpPtr(AeRam *r, uint32_t actor, uint32_t frame_a,
                          uint32_t frame_b, uint32_t fraction)
{
    uint32_t model = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_MODEL);
    uint32_t bone_count = ae_rd16(r, model + AE_ANIMDRAW_MODEL_BONE_COUNT);
    uint32_t pose = ae_rd32(r, actor + AE_ANIMDRAW_PLAYER_POSE);
    uint32_t complement = 4096u - fraction;
    if (ae_ram_failed(r)) return;

    for (uint32_t bone = 0; bone < bone_count; ++bone) {
        uint32_t first_word = ae_rd32(r, frame_a + AE_ANIMDRAW_ANIM_BONE_WORDS +
                                      bone * AE_ANIMDRAW_ANIM_BONE_STRIDE);
        uint32_t second_word = ae_rd32(r, frame_b + AE_ANIMDRAW_ANIM_BONE_WORDS +
                                       bone * AE_ANIMDRAW_ANIM_BONE_STRIDE);
        int32_t first[3] = {
            sign_extend_bits(first_word, 0, 10) * 4,
            sign_extend_bits(first_word, 10, 12) * 4,
            sign_extend_bits(first_word, 22, 10) * 4
        };
        int32_t second[3] = {
            sign_extend_bits(second_word, 0, 10) * 4,
            sign_extend_bits(second_word, 10, 12) * 4,
            sign_extend_bits(second_word, 22, 10) * 4
        };
        uint32_t angles = AE_ANIMDRAW_PLAYER_ANGLES + bone * AE_ANIMDRAW_ANGLE_BONE_STRIDE;

        for (uint32_t axis = 0; axis < 3u; ++axis) {
            int32_t difference = second[axis] - first[axis];
            if (difference >= 2049) first[axis] += 4096;
            else if (difference <= -2049) second[axis] += 4096;

            uint32_t weighted_bits = (uint32_t)first[axis] * complement +
                                     (uint32_t)second[axis] * fraction;
            int32_t weighted = (int32_t)weighted_bits;
            if (weighted < 0) weighted = (int32_t)((uint32_t)weighted + 4095u);
            ae_wr16(r, angles + axis * 2u, (uint32_t)(weighted >> 12));
        }
        if (ae_ram_failed(r)) return;
    }

    uint32_t first_root = ae_rd32(r, frame_a + AE_ANIMDRAW_ANIM_ROOT_WORD);
    uint32_t second_root = ae_rd32(r, frame_b + AE_ANIMDRAW_ANIM_ROOT_WORD);
    const uint32_t shifts[3] = {0u, 22u, 10u};
    const uint32_t widths[3] = {10u, 10u, 12u};
    const uint32_t stores[3] = {4u, 8u, 6u};
    for (uint32_t component = 0; component < 3u; ++component) {
        int32_t first = sign_extend_bits(first_root, shifts[component], widths[component]);
        int32_t second = sign_extend_bits(second_root, shifts[component], widths[component]);
        uint32_t weighted_bits = (uint32_t)first * complement + (uint32_t)second * fraction;
        int32_t weighted = (int32_t)weighted_bits;
        if (weighted < 0) weighted = (int32_t)((uint32_t)weighted + 4095u);
        ae_wr16(r, pose + stores[component], (uint32_t)(weighted >> 12));
    }
}

int32_t ae_PlayerCanActivateGadget(AeRam *r, uint32_t gadget)
{
    (void)gadget;
    uint32_t player = AE_ANIMDRAW_PLAYER;
    uint32_t area_state = AE_ANIMDRAW_AREA_STATE;
    int32_t health = ae_rds16(r, player + AE_ANIMDRAW_PLAYER_HEALTH);
    if (health <= 0) return 0;

    uint32_t gadget_state = ae_rd16(r, area_state + AE_ANIMDRAW_AREA_LEVEL_FIELD - 2u);
    if (gadget_state == 5u || gadget_state == 6u) return 0;
    if (ae_rd16(r, area_state + AE_ANIMDRAW_AREA_LEVEL_FIELD) == 11u) return 0;

    uint32_t state = ae_rd16(r, player + AE_ANIMDRAW_PLAYER_STATE);
    if (state == 14u || state == 15u || state == 47u || state == 48u ||
        state == 66u || state == 67u) return 0;
    return state < 27u || state > 33u;
}

int32_t ae_AreaGetMonkeyRecord(AeRam *r, const AeAreaSetHooks *area_hooks,
                               uint32_t area, uint32_t out_record)
{
    int32_t level = ae_AreaGetLevel(r, area_hooks, (int32_t)area);
    if (level == 32) return 0;
    level = ae_AreaGetLevel(r, area_hooks, (int32_t)area);
    if (level == 31) return 0;
    level = ae_AreaGetLevel(r, area_hooks, (int32_t)area);
    if (level == 24) return 0;
    level = ae_AreaGetLevel(r, area_hooks, (int32_t)area);
    int32_t first_area = ae_GetLevelFirstArea(r, level);
    uint32_t record_index = area - (uint32_t)first_area;
    uint32_t table_entry = AE_ANIMDRAW_G_AREA_INDICES +
                           record_index * AE_ANIMDRAW_LEVEL_ENTRY_STRIDE;
    uint32_t record = AE_ANIMDRAW_G_AREA_RECORDS +
                      record_index * AE_ANIMDRAW_LEVEL_RECORD_STRIDE;
    ae_wr32(r, out_record, ae_rd16(r, table_entry));
    return (int32_t)record;
}

int32_t ae_MonkeyFlag248Set(AeRam *r, uint32_t monkey)
{
    if (ae_rd8(r, monkey + AE_ANIMDRAW_MONKEY_KIND) != AE_ANIMDRAW_MONKEY_SPECIAL_KIND)
        return 0;
    return (int32_t)((ae_rd32(r, monkey + AE_ANIMDRAW_MONKEY_FLAGS) >>
                      AE_ANIMDRAW_MONKEY_SPECIAL_BIT) & 1u);
}

void ae_PlayerSwimSfxUpdate(AeRam *r, const AeAnimDrawHooks *hooks)
{
    uint32_t player = AE_ANIMDRAW_PLAYER;
    int32_t sound_id = 0;
    if (ae_rd16(r, player + AE_ANIMDRAW_PLAYER_STATE) == AE_ANIMDRAW_SWIM_STATE) {
        if (ae_rd16(r, AE_ANIMDRAW_AREA_STATE + AE_ANIMDRAW_AREA_LEVEL_FIELD) !=
            AE_ANIMDRAW_SWIM_AREA_EXEMPT) {
            sound_id = (ae_rd32(r, AE_ANIMDRAW_G_SWIM_FLAGS) & 0xAu) != 0u ?
                       AE_ANIMDRAW_SOUND_ID_DIVE : AE_ANIMDRAW_SOUND_ID_SWIM;
        }
    } else {
        int32_t x = ae_rds32(r, player + AE_ANIMDRAW_PLAYER_VELOCITY);
        int32_t y = ae_rds32(r, player + AE_ANIMDRAW_PLAYER_VELOCITY_Y);
        int32_t z = ae_rds32(r, player + AE_ANIMDRAW_PLAYER_VELOCITY_Z);
        uint32_t speed = (x < 0 ? 0u - (uint32_t)x : (uint32_t)x) +
                         (y < 0 ? 0u - (uint32_t)y : (uint32_t)y) +
                         (z < 0 ? 0u - (uint32_t)z : (uint32_t)z);
        if ((int32_t)speed >= (int32_t)AE_ANIMDRAW_SWIM_SPEED_LIMIT)
            sound_id = AE_ANIMDRAW_SOUND_ID_SWIM;
    }

    uint32_t loop = AE_ANIMDRAW_G_SOUND_LOOP_DATA;
    int32_t left_voice = ae_rds16(r, loop + AE_ANIMDRAW_SOUND_LOOP_LEFT);
    int32_t right_voice = ae_rds16(r, loop + AE_ANIMDRAW_SOUND_LOOP_RIGHT);
    if (sound_id == 0) {
        if (left_voice != -1) {
            (void)animdraw_call(r, hooks, hooks ? hooks->SoundLoopStop : NULL,
                                "SoundLoopStop", (uint32_t)right_voice,
                                (uint32_t)left_voice, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, loop + AE_ANIMDRAW_SOUND_LOOP_LEFT, AE_ANIMDRAW_SOUND_LOOP_OFF);
            ae_wr16(r, loop + AE_ANIMDRAW_SOUND_LOOP_RIGHT, AE_ANIMDRAW_SOUND_LOOP_OFF);
        }
        return;
    }

    if (right_voice == sound_id) return;
    if (left_voice != -1) {
        (void)animdraw_call(r, hooks, hooks ? hooks->SoundLoopStop : NULL,
                            "SoundLoopStop", (uint32_t)right_voice,
                            (uint32_t)left_voice, 0, 0);
        if (ae_ram_failed(r)) return;
    }
    int32_t new_voice = animdraw_call(r, hooks,
                                      hooks ? hooks->SoundLoopStart : NULL,
                                      "SoundLoopStart", 0u,
                                      (uint32_t)sound_id, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, loop + AE_ANIMDRAW_SOUND_LOOP_LEFT, (uint32_t)new_voice);
    if ((int16_t)new_voice != -1)
        ae_wr16(r, loop + AE_ANIMDRAW_SOUND_LOOP_RIGHT, (uint32_t)sound_id);
}

void ae_PlayerRumbleSlam(AeRam *r, const AeAnimDrawHooks *hooks)
{
    uint32_t positions = AE_ANIMDRAW_GADGET_WORK_DATA;
    uint32_t player = AE_ANIMDRAW_PLAYER;
    uint32_t rumble = AE_ANIMDRAW_G_SOUND_LOOP_DATA;
    uint32_t x_sum = ae_rd32(r, positions + AE_ANIMDRAW_RUMBLE_POS_A_X) +
                     ae_rd32(r, positions + AE_ANIMDRAW_RUMBLE_POS_B_X);
    uint32_t y_sum = ae_rd32(r, positions + AE_ANIMDRAW_RUMBLE_POS_A_Y) +
                     ae_rd32(r, positions + AE_ANIMDRAW_RUMBLE_POS_B_Y);
    uint32_t z_sum = ae_rd32(r, positions + AE_ANIMDRAW_RUMBLE_POS_A_Z) +
                     ae_rd32(r, positions + AE_ANIMDRAW_RUMBLE_POS_B_Z);

    ae_wr16(r, rumble + AE_ANIMDRAW_RUMBLE_X,
            (uint32_t)animdraw_sra((int32_t)x_sum, AE_ANIMDRAW_RUMBLE_AVERAGE_SHIFT));
    ae_wr16(r, rumble + AE_ANIMDRAW_RUMBLE_Y,
            (uint32_t)animdraw_sra((int32_t)y_sum, AE_ANIMDRAW_RUMBLE_AVERAGE_SHIFT));
    ae_wr16(r, rumble + AE_ANIMDRAW_RUMBLE_Z,
            (uint32_t)animdraw_sra((int32_t)z_sum, AE_ANIMDRAW_RUMBLE_AVERAGE_SHIFT));
    ae_wr16(r, rumble + AE_ANIMDRAW_RUMBLE_STRENGTH, 512u);
    ae_wr32(r, rumble + AE_ANIMDRAW_RUMBLE_DATA,
            ae_rd32(r, player + AE_ANIMDRAW_PLAYER_RUMBLE_DATA));
    ae_wr32(r, rumble + AE_ANIMDRAW_RUMBLE_DATA + AE_ANIMDRAW_RUMBLE_DATA_WORD_B,
            ae_rd32(r, player + AE_ANIMDRAW_PLAYER_RUMBLE_DATA + AE_ANIMDRAW_RUMBLE_DATA_WORD_B));
    ae_wr32(r, rumble + AE_ANIMDRAW_RUMBLE_MODE, 1u);
    (void)animdraw_call(r, hooks, hooks ? hooks->PadRumble : NULL, "PadRumble",
                        1u, 255u, 6u, 0u);
    if (ae_ram_failed(r)) return;
    (void)animdraw_call(r, hooks, hooks ? hooks->PlaySfx : NULL, "PlaySfx",
                        AE_ANIMDRAW_SOUND_ID_RUMBLE, 0, 0, 0);
}

void ae_RcCarDraw(AeRam *r, const AeAnimDrawHooks *hooks)
{
    uint32_t car = AE_ANIMDRAW_G_CAR;
    if (ae_rd32(r, car + AE_ANIMDRAW_CAR_DRAW_ENABLED) == 0u) return;

    uint32_t matrix = AE_ANIMDRAW_CAR_MATRIX_TEMP;
    uint32_t car_model = AE_ANIMDRAW_G_CAR_MODEL;
    ae_MatrixFromFrontUp(r, car + AE_ANIMDRAW_CAR_FRONT,
                         car + AE_ANIMDRAW_CAR_UP, matrix, 0u);
    if (ae_ram_failed(r)) return;

    ae_wr32(r, AE_ANIMDRAW_CAR_POSITION_TEMP + AE_ANIMDRAW_CAR_POSITION_X,
            ae_rd32(r, car + AE_ANIMDRAW_CAR_POSITION));
    ae_wr32(r, AE_ANIMDRAW_CAR_POSITION_TEMP + AE_ANIMDRAW_CAR_POSITION_Y,
            ae_rd32(r, car + AE_ANIMDRAW_CAR_POSITION + AE_ANIMDRAW_CAR_POSITION_Y_FROM_BASE));
    ae_wr32(r, AE_ANIMDRAW_CAR_POSITION_TEMP + AE_ANIMDRAW_CAR_POSITION_Z,
            ae_rd32(r, car + AE_ANIMDRAW_CAR_POSITION + AE_ANIMDRAW_CAR_POSITION_Z_FROM_BASE));
    (void)ae_sdk_CompMatrix(r, AE_ANIMDRAW_G_VIEW_MATRIX, matrix, matrix);
    if (ae_ram_failed(r)) return;

    (void)animdraw_call(r, hooks,
                        hooks ? hooks->ModelTransformRigid : NULL,
                        "ModelTransformRigid", car_model, matrix,
                        AE_ANIMDRAW_G_CAR_MODEL_MATRIX, 0u);
    if (ae_ram_failed(r)) return;

    uint32_t model_instance = AE_ANIMDRAW_G_CAR_MODEL_MATRIX;
    uint32_t model_header = ae_rd32(r, car_model + AE_ANIMDRAW_MODEL_INSTANCE_HEADER);
    uint32_t color_cycle_count = ae_rd32(r, AE_ANIMDRAW_G_CAR_CYCLE_INDEX);
    int32_t cycle_count = ae_rds16(r, car + AE_ANIMDRAW_CAR_CYCLE_COUNT);
    uint32_t cycle_table = AE_ANIMDRAW_G_CAR_CYCLE_RECORDS;
    if (cycle_count > 0) {
        ae_ModelPrepCycleColors(r, model_instance,
                                ae_rd16(r, model_header + AE_ANIMDRAW_CAR_CYCLE_COUNT_FIELD));
        if (ae_ram_failed(r)) return;
        uint32_t scale = color_cycle_count * AE_ANIMDRAW_CAR_CYCLE_STRIDE_FACTOR;
        cycle_table += scale << AE_ANIMDRAW_CAR_CYCLE_SCALE_SHIFT;
        (void)animdraw_call(r, hooks,
                            hooks ? hooks->ModelEmitPrimsCycleColors : NULL,
                            "ModelEmitPrimsCycleColors", cycle_table,
                            model_instance, car_model,
                            AE_PLAYER1_MODEL_COLOR_CYCLE_OUTPUT +
                            ((uint32_t)ae_rd16(r, model_header +
                                AE_ANIMDRAW_CAR_CYCLE_COUNT_FIELD) * 4u));
    } else {
        uint32_t draw_data = AE_ANIMDRAW_G_CAR_CYCLE_RECORDS +
                             (color_cycle_count * AE_ANIMDRAW_CAR_CYCLE_STRIDE_FACTOR <<
                              AE_ANIMDRAW_CAR_CYCLE_SCALE_SHIFT);
        (void)animdraw_call(r, hooks, hooks ? hooks->ModelDraw : NULL,
                            "ModelDraw", draw_data, model_instance, car_model, 0u);
    }
    if (ae_ram_failed(r)) return;

    if (ae_rd8(r, AE_ANIMDRAW_G_CAR_STAGE) != AE_ANIMDRAW_CAR_STAGE_ACTIVE) {
        uint32_t next = ae_rd32(r, car + AE_ANIMDRAW_CAR_STAGE_COUNTER) +
                        AE_ANIMDRAW_CAR_STAGE_STEP;
        ae_wr32(r, car + AE_ANIMDRAW_CAR_STAGE_TEMP_A, 0u);
        ae_wr32(r, car + AE_ANIMDRAW_CAR_STAGE_TEMP_B, 0u);
        ae_wr32(r, car + AE_ANIMDRAW_CAR_STAGE_COUNTER, next);
    }
    if (hooks == NULL) {
        ae_Gadget7TrailUpdate(r, NULL);
    } else {
        ae_Gadget7TrailUpdate(r, hooks->gadget_hooks);
    }
}

static int32_t animdraw_clamp_rotation(int32_t target, int32_t current, int32_t bias)
{
    int32_t difference = (int32_t)((uint32_t)target - (uint32_t)current);
    int32_t scaled = (int32_t)((uint32_t)difference << AE_ANIMDRAW_HEAD_ROTATION_SHIFT);
    scaled = (int32_t)((uint32_t)scaled + (uint32_t)bias);
    if (scaled < -AE_ANIMDRAW_HEAD_ROTATION_CLAMP)
        return -AE_ANIMDRAW_HEAD_ROTATION_CLAMP;
    if (scaled > AE_ANIMDRAW_HEAD_ROTATION_CLAMP)
        return AE_ANIMDRAW_HEAD_ROTATION_CLAMP;
    return scaled;
}

static int32_t animdraw_sra1(int32_t value)
{
    uint32_t bits = (uint32_t)value;
    return (int32_t)((bits >> 1) | (bits & 0x80000000u));
}

void ae_PlayerHeadLookUpdate(AeRam *r, const AeAnimDrawHooks *hooks)
{
    uint32_t vector_a = AE_ANIMDRAW_HEAD_MATRIX_TEMP_A;
    uint32_t vector_b = AE_ANIMDRAW_HEAD_MATRIX_TEMP_B;
    uint32_t vector_c = AE_ANIMDRAW_HEAD_MATRIX_TEMP_C;
    uint32_t actor = AE_ANIMDRAW_G_HEAD_ACTOR;
    uint32_t actor_matrices = ae_rd32(r, actor + AE_ANIMDRAW_HEAD_ACTOR_BONE_MATRIX);
    uint32_t root_matrix = actor_matrices + AE_ANIMDRAW_HEAD_ACTOR_MATRIX;

    ae_wr32(r, vector_a, ae_rd32(r, AE_ANIMDRAW_G_HEAD_LOOK_SOURCE));
    ae_wr32(r, vector_a + AE_ANIMDRAW_HEAD_VECTOR_SECOND_WORD,
            ae_rd32(r, AE_ANIMDRAW_G_HEAD_LOOK_SOURCE + AE_ANIMDRAW_HEAD_SOURCE_VECTOR_WORD1));
    ae_wr32(r, vector_b,
            ae_rd32(r, AE_ANIMDRAW_G_HEAD_LOOK_SOURCE + AE_ANIMDRAW_HEAD_SOURCE_VECTOR_WORD2));
    ae_wr32(r, vector_b + AE_ANIMDRAW_HEAD_VECTOR_SECOND_WORD,
            ae_rd32(r, AE_ANIMDRAW_G_HEAD_LOOK_SOURCE + AE_ANIMDRAW_HEAD_SOURCE_VECTOR_WORD3));
    (void)animdraw_call(r, hooks, hooks ? hooks->SetRotMatrix : NULL,
                        "SetRotMatrix", AE_ANIMDRAW_G_STATIC_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    (void)animdraw_call(r, hooks, hooks ? hooks->SetTransMatrix : NULL,
                        "SetTransMatrix", AE_ANIMDRAW_G_STATIC_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    (void)animdraw_call(r, hooks, hooks ? hooks->SDK_80031230 : NULL,
                        "SDK_80031230", vector_a, vector_a, vector_c, 0);
    if (ae_ram_failed(r)) return;

    int32_t angle_x = ae_rds16(r, AE_ANIMDRAW_G_HEAD_LOOK_ANGLES);
    int32_t angle_y = ae_rds16(r, AE_ANIMDRAW_G_HEAD_LOOK_ANGLES + AE_ANIMDRAW_HEAD_LOOK_Y);
    int32_t angle_z = ae_rds16(r, AE_ANIMDRAW_G_HEAD_LOOK_ANGLES + AE_ANIMDRAW_HEAD_LOOK_Z);
    int32_t vector_x = ae_rds16(r, vector_a);
    int32_t vector_y = ae_rds16(r, vector_a + AE_ANIMDRAW_HEAD_LOOK_Y);
    int32_t vector_z = ae_rds16(r, vector_a + AE_ANIMDRAW_HEAD_LOOK_Z);
    ae_wr16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA,
            (uint32_t)animdraw_clamp_rotation(angle_x, vector_x, 0));
    ae_wr16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA + AE_ANIMDRAW_HEAD_LOOK_Y,
            (uint32_t)animdraw_clamp_rotation(angle_y, vector_y,
                                               AE_ANIMDRAW_HEAD_ROTATION_BIAS_Y));
    ae_wr16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA + AE_ANIMDRAW_HEAD_LOOK_Z,
            (uint32_t)animdraw_clamp_rotation(angle_z, vector_z, 0));
    ae_wr16(r, AE_ANIMDRAW_G_HEAD_LOOK_ANGLES, (uint32_t)vector_x);
    ae_wr16(r, AE_ANIMDRAW_G_HEAD_LOOK_ANGLES + AE_ANIMDRAW_HEAD_LOOK_Y, (uint32_t)vector_y);
    ae_wr16(r, AE_ANIMDRAW_G_HEAD_LOOK_ANGLES + AE_ANIMDRAW_HEAD_LOOK_Z, (uint32_t)vector_z);
    if (ae_ram_failed(r)) return;

    for (uint32_t word = 0; word < AE_ANIMDRAW_HEAD_MATRIX_COPY_BYTES / 4u; ++word) {
        ae_wr32(r, actor_matrices + AE_ANIMDRAW_HEAD_ACTOR_MATRIX_OFFSET + word * 4u,
                ae_rd32(r, AE_ANIMDRAW_G_HEAD_MATRIX_COPY + word * 4u));
        if (ae_ram_failed(r)) return;
    }

    (void)ae_sdk_RotMatrixZYX(r, vector_b, root_matrix);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, actor_matrices + AE_ANIMDRAW_HEAD_MATRIX_TRANSLATION_X,
            24u);
    ae_wr32(r, actor_matrices + AE_ANIMDRAW_HEAD_MATRIX_TRANSLATION_Y,
            AE_ANIMDRAW_HEAD_ROOT_Y_POSITION);
    ae_wr32(r, actor_matrices + AE_ANIMDRAW_HEAD_MATRIX_TRANSLATION_Z, 0u);
    (void)ae_sdk_CompMatrix(r, AE_ANIMDRAW_G_STATIC_MATRIX,
                            root_matrix, root_matrix);
    if (ae_ram_failed(r)) return;

    ae_wr16(r, actor_matrices + AE_ANIMDRAW_HEAD_BONE_AXIS_X,
            ae_rd16(r, actor_matrices + AE_ANIMDRAW_HEAD_BONE_AXIS_X) +
            ae_rd16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA));
    ae_wr16(r, actor_matrices + AE_ANIMDRAW_HEAD_BONE_AXIS_Y,
            ae_rd16(r, actor_matrices + AE_ANIMDRAW_HEAD_BONE_AXIS_Y) +
            ae_rd16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA + AE_ANIMDRAW_HEAD_LOOK_Y));
    ae_wr16(r, actor_matrices + AE_ANIMDRAW_HEAD_BONE_AXIS_Z,
            ae_rd16(r, actor_matrices + AE_ANIMDRAW_HEAD_BONE_AXIS_Z) +
            ae_rd16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA + AE_ANIMDRAW_HEAD_LOOK_Z));
    (void)animdraw_call(r, hooks, hooks ? hooks->SDK_800302D0 : NULL,
                        "SDK_800302D0", root_matrix, root_matrix, 0, 0);
    if (ae_ram_failed(r)) return;

    uint32_t translation_matrix = actor_matrices + AE_ANIMDRAW_HEAD_ACTOR_TRANSLATION;
    (void)ae_sdk_MatrixIdentity(r, translation_matrix);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, translation_matrix + AE_ANIMDRAW_MATRIX_TRANSLATION_X, 96u);
    ae_wr32(r, translation_matrix + AE_ANIMDRAW_MATRIX_TRANSLATION_Y, 0u);
    ae_wr32(r, translation_matrix + AE_ANIMDRAW_MATRIX_TRANSLATION_Z, 0u);
    (void)ae_sdk_CompMatrix(r, root_matrix, translation_matrix, translation_matrix);
    if (ae_ram_failed(r)) return;

    ae_wr16(r, translation_matrix + AE_ANIMDRAW_HEAD_MATRIX_DELTA_X,
            ae_rd16(r, translation_matrix + AE_ANIMDRAW_HEAD_MATRIX_DELTA_X) +
            animdraw_sra1(ae_rds16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA)));
    ae_wr16(r, translation_matrix + AE_ANIMDRAW_HEAD_MATRIX_DELTA_Y,
            ae_rd16(r, translation_matrix + AE_ANIMDRAW_HEAD_MATRIX_DELTA_Y) +
            animdraw_sra1(ae_rds16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA + AE_ANIMDRAW_HEAD_LOOK_Y)));
    ae_wr16(r, translation_matrix + AE_ANIMDRAW_HEAD_MATRIX_DELTA_Z,
            ae_rd16(r, translation_matrix + AE_ANIMDRAW_HEAD_MATRIX_DELTA_Z) +
            animdraw_sra1(ae_rds16(r, AE_ANIMDRAW_G_HEAD_LOOK_DELTA + AE_ANIMDRAW_HEAD_LOOK_Z)));
    (void)animdraw_call(r, hooks, hooks ? hooks->SDK_800302D0 : NULL,
                        "SDK_800302D0", translation_matrix, translation_matrix, 0, 0);
    if (ae_ram_failed(r)) return;

    (void)ae_sdk_CompMatrix(r, AE_ANIMDRAW_G_CAMERA_MATRIX,
                            root_matrix, root_matrix);
    (void)ae_sdk_CompMatrix(r, AE_ANIMDRAW_G_CAMERA_MATRIX,
                            translation_matrix, translation_matrix);
    if (ae_ram_failed(r)) return;
    (void)animdraw_call(r, hooks,
                        hooks ? hooks->ModelTransformSkinned : NULL,
                        "ModelTransformSkinned", actor, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    (void)animdraw_call(r, hooks, hooks ? hooks->ModelDraw : NULL,
                        "ModelDraw", ae_rd32(r, AE_ANIMDRAW_G_CURRENT_OT),
                        AE_ANIMDRAW_G_CAR_MODEL_MATRIX, actor, 0);
}

static int32_t animdraw_sra(int32_t value, uint32_t shift)
{
    uint32_t bits = (uint32_t)value;
    uint32_t result = bits >> shift;
    if ((bits & 0x80000000u) != 0u) result |= 0xFFFFFFFFu << (32u - shift);
    return (int32_t)result;
}

static int32_t animdraw_radar_score(AeRam *r, uint32_t vector, int32_t *distance)
{
    int32_t squares = ae_sdk_VectorNormalSS(r, vector, vector);
    if (ae_ram_failed(r)) return 0;
    *distance = ae_IntSqrt(r, squares);
    if (ae_ram_failed(r)) return 0;

    int32_t x = ae_rds16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_X);
    int32_t z = ae_rds16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_Z);
    int32_t forward_x = ae_rds16(r, AE_ANIMDRAW_G_RADAR_SIGNAL + AE_ANIMDRAW_RADAR_X);
    int32_t forward_z = ae_rds16(r, AE_ANIMDRAW_G_RADAR_SIGNAL + AE_ANIMDRAW_RADAR_Z);
    uint32_t dot_bits = (uint32_t)(x * forward_x) + (uint32_t)(z * forward_z);
    return animdraw_sra((int32_t)dot_bits, 12u);
}

static int32_t animdraw_radar_contribution(AeRam *r, int32_t distance, int32_t score)
{
    int32_t numerator = (int32_t)(((uint32_t)(score - 3547)) << 12);
    int32_t distance_denominator;
    if (distance < AE_ANIMDRAW_RADAR_CLOSE_RANGE) {
        distance_denominator = distance / 4 + 1;
        return AE_ANIMDRAW_RADAR_EMITTER_BASE / distance_denominator + 2;
    }
    uint32_t polynomial = (uint32_t)distance * 549u;
    distance_denominator = (int32_t)(polynomial >> 2) + 1;
    if (distance_denominator == 0 ||
        (distance_denominator == -1 && numerator == INT32_MIN)) {
        ae_ram_fail(r, "MonkeyRadarSignal: signed divide trap");
        return 0;
    }
    return numerator / distance_denominator + 2;
}

int32_t ae_MonkeyRadarSignal(AeRam *r, const AeAnimDrawHooks *hooks)
{
    (void)hooks;
    uint32_t player = AE_ANIMDRAW_PLAYER;
    uint32_t radar = AE_ANIMDRAW_G_RADAR_SIGNAL;
    uint32_t vector = AE_ANIMDRAW_RADAR_VECTOR_TEMP;
    int32_t score_total = 0;
    int32_t best_score = 0;
    int32_t valid_monkeys = 0;
    int32_t area_signal = 0;

    for (uint32_t slot = 0; slot < AE_ANIMDRAW_MONKEY_SLOTS; ++slot) {
        uint32_t monkey = AE_ANIMDRAW_MONKEYS + slot * AE_ANIMDRAW_MONKEY_STRIDE;
        uint32_t kind = ae_rd8(r, monkey + AE_ANIMDRAW_MONKEY_KIND);
        if (kind == 0u || kind == 2u || kind == 3u) continue;

        int32_t heading_x = animdraw_sra((int32_t)((uint32_t)ae_rds16(
                                    r, player + AE_ANIMDRAW_PLAYER_LOOK_X) << 16), 21u);
        int32_t heading_z = animdraw_sra((int32_t)((uint32_t)ae_rds16(
                                    r, player + AE_ANIMDRAW_PLAYER_LOOK_Z) << 16), 21u);
        int32_t dx = (int32_t)((uint32_t)ae_rd16(r, monkey + AE_ANIMDRAW_MONKEY_POS_X) -
                               ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_X) +
                               (uint32_t)heading_x);
        int32_t dz = (int32_t)((uint32_t)ae_rd16(r, monkey + AE_ANIMDRAW_MONKEY_POS_Z) -
                               ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_Z) +
                               (uint32_t)heading_z);
        ae_wr16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_X, (uint32_t)dx);
        ae_wr16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_Y, 0u);
        ae_wr16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_Z, (uint32_t)dz);
        int32_t distance;
        int32_t score = animdraw_radar_score(r, vector, &distance);
        if (ae_ram_failed(r)) return 0;
        ++valid_monkeys;
        if (score < AE_ANIMDRAW_RADAR_MIN_SCORE) continue;

        int32_t contribution = animdraw_radar_contribution(r, distance, score);
        if (ae_ram_failed(r)) return 0;
        score_total = (int32_t)((uint32_t)score_total + (uint32_t)contribution);
        if (best_score < contribution) {
            best_score = contribution;
            ae_wr32(r, radar + AE_ANIMDRAW_RADAR_STATE_FOUND, 1u);
            ae_wr32(r, radar + AE_ANIMDRAW_RADAR_STATE_TARGET, monkey);
        }
    }

    if (valid_monkeys == 0) {
        int32_t area_count = ae_rds32(r, AE_ANIMDRAW_G_AREA_MONKEY_COUNT);
        uint32_t area_table = ae_rd32(r, AE_ANIMDRAW_G_AREA_MONKEYS);
        if (area_count > 0) {
            for (int32_t entry_index = 0; entry_index < area_count; ++entry_index) {
                uint32_t entry = area_table + (uint32_t)entry_index *
                                 AE_ANIMDRAW_AREA_MONKEY_STRIDE;
                uint32_t area = ae_rd32(r, entry + AE_ANIMDRAW_AREA_MONKEY_TARGET);
                int32_t record = ae_AreaGetMonkeyRecord(r,
                                                        hooks ? hooks->area_hooks : NULL,
                                                        area, AE_ANIMDRAW_RADAR_COUNT_TEMP);
                if (ae_ram_failed(r)) return 0;
                if (record == 0) continue;
                uint32_t monkey_kind_count = 0;
                for (uint32_t index = 0; index < AE_ANIMDRAW_AREA_MONKEY_KINDS; ++index) {
                    uint32_t kind = ae_rd8(r, (uint32_t)record + index);
                    if (kind == 2u || kind == 3u) ++monkey_kind_count;
                }
                if (monkey_kind_count >= ae_rd32(r, AE_ANIMDRAW_RADAR_COUNT_TEMP)) continue;

                int32_t dx = (int32_t)((uint32_t)ae_rd16(r, entry + AE_ANIMDRAW_AREA_MONKEY_X) -
                                       ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_X));
                int32_t dz = (int32_t)((uint32_t)ae_rd16(r, entry + AE_ANIMDRAW_AREA_MONKEY_Z) -
                                       ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_Z));
                ae_wr16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_X, (uint32_t)dx);
                ae_wr16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_Y, 0u);
                ae_wr16(r, vector + AE_ANIMDRAW_RADAR_VECTOR_Z, (uint32_t)dz);
                int32_t distance;
                int32_t score = animdraw_radar_score(r, vector, &distance);
                if (ae_ram_failed(r)) return 0;
                if (score >= AE_ANIMDRAW_RADAR_MIN_SCORE) area_signal = 1;
            }
        }
        if (area_signal != 0) score_total = 1;
    }

    if (score_total >= AE_ANIMDRAW_RADAR_AREA_LIMIT)
        score_total = AE_ANIMDRAW_RADAR_RESULT_LIMIT;
    return score_total;
}

static uint32_t animdraw_radar_cycle_table(AeRam *r)
{
    uint32_t cycle_index = ae_rd32(r, AE_ANIMDRAW_G_RADAR_STATE + AE_ANIMDRAW_RADAR_STATE_CYCLE_INDEX);
    return AE_ANIMDRAW_G_RADAR_CYCLE_BASE +
           ((cycle_index * AE_ANIMDRAW_CAR_CYCLE_STRIDE_FACTOR) <<
            AE_ANIMDRAW_CAR_CYCLE_SCALE_SHIFT);
}

static void animdraw_radar_fill_and_emit(AeRam *r, const AeAnimDrawHooks *hooks,
                                         uint32_t fill_color, uint32_t extra_material)
{
    uint32_t fill = AE_ANIMDRAW_G_RADAR_FILL;
    uint32_t material = (ae_rd32(r, AE_ANIMDRAW_G_RADAR_STATE) & 0x1Fu) << 8;
    material |= extra_material;
    (void)animdraw_call5(r, hooks,
                         hooks ? hooks->ModelFillVertexColors : NULL,
                         "ModelFillVertexColors", fill, material, 0u, 6u,
                         fill_color);
    if (ae_ram_failed(r)) return;

    uint32_t cursor = ae_rd32(r, fill + AE_ANIMDRAW_RADAR_FILL_CURSOR);
    uint32_t cycle_table = animdraw_radar_cycle_table(r);
    (void)animdraw_call(r, hooks, hooks ? hooks->ModelEmitPrims : NULL,
                        "ModelEmitPrims", cycle_table, cursor,
                        AE_ANIMDRAW_G_RADAR_MODEL, 6u);
}

static void animdraw_radar_draw_model(AeRam *r, const AeAnimDrawHooks *hooks,
                                      uint32_t signal, int emit_packet)
{
    uint32_t base_color;
    if (signal < AE_ANIMDRAW_RADAR_NEAR_LIMIT)
        base_color = (uint32_t)signal * 8u + AE_ANIMDRAW_RADAR_COLOR_RAMP;
    else
        base_color = AE_ANIMDRAW_RADAR_COLOR_BASE;
    uint32_t half = base_color >> 1;
    uint32_t packed_color = base_color | (half << 16) | (half << 8);
    animdraw_radar_fill_and_emit(r, hooks, packed_color, 0u);
    if (ae_ram_failed(r)) return;

    uint32_t cursor = ae_rd32(r, AE_ANIMDRAW_G_RADAR_FILL + AE_ANIMDRAW_RADAR_FILL_CURSOR);
    uint32_t cycle_table = animdraw_radar_cycle_table(r);
    if (emit_packet) {
        uint32_t denominator = AE_ANIMDRAW_RADAR_MAX_DISTANCE - signal;
        if (denominator == 0u) {
            ae_ram_fail(r, "RadarUpdateDraw: unsigned divide by zero");
            return;
        }
        uint32_t remainder = ae_rd32(r, AE_ANIMDRAW_G_RADAR_STATE) % denominator;
        if (remainder < AE_ANIMDRAW_RADAR_PACKET_THRESHOLD) {
            (void)animdraw_call(r, hooks,
                                hooks ? hooks->RendererPrimitiveBlock : NULL,
                                "RendererPrimitiveBlock", cursor,
                                cycle_table + AE_ANIMDRAW_RADAR_PRIMITIVE_CYCLE_OFFSET,
                                AE_ANIMDRAW_G_RADAR_MODEL,
                                1u);
            if (ae_ram_failed(r)) return;
            cursor += AE_ANIMDRAW_RADAR_PACKET_BYTES;
        }
        ae_wr32(r, AE_ANIMDRAW_G_RADAR_OT, cursor);
    }
}

static void animdraw_radar_stop_voice(AeRam *r, const AeAnimDrawHooks *hooks,
                                      uint32_t voice_address, uint32_t sound_id)
{
    int32_t voice = ae_rds16(r, voice_address);
    if (voice == -1) return;
    (void)animdraw_call(r, hooks, hooks ? hooks->SoundLoopStop : NULL,
                        "SoundLoopStop", sound_id, (uint32_t)voice, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, voice_address, AE_ANIMDRAW_SOUND_LOOP_OFF);
}

static void animdraw_radar_loop_update(AeRam *r, const AeAnimDrawHooks *hooks,
                                       int32_t signal)
{
    uint32_t voice_a = AE_ANIMDRAW_G_RADAR_VOICES + AE_ANIMDRAW_RADAR_VOICE_A;
    uint32_t voice_b = AE_ANIMDRAW_G_RADAR_VOICES + AE_ANIMDRAW_RADAR_VOICE_B;
    if (signal > 0) {
        animdraw_radar_stop_voice(r, hooks, voice_a, AE_ANIMDRAW_RADAR_LOOP_A);
        if (ae_ram_failed(r)) return;
        int32_t current_b = ae_rds16(r, voice_b);
        if (current_b == -1) {
            int32_t voice = animdraw_call(r, hooks,
                                          hooks ? hooks->SoundLoopStart : NULL,
                                          "SoundLoopStart", 0u,
                                          AE_ANIMDRAW_RADAR_LOOP_B, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, voice_b, (uint32_t)voice);
        } else {
            uint32_t emitter_pitch = AE_ANIMDRAW_RADAR_EMITTER_BASE +
                                    (uint32_t)signal * AE_ANIMDRAW_RADAR_EMITTER_SCALE;
            uint32_t rumble_strength = AE_ANIMDRAW_RADAR_RUMBLE_BASE +
                                       (uint32_t)signal * AE_ANIMDRAW_RADAR_RUMBLE_STEP;
            (void)animdraw_call(r, hooks,
                                hooks ? hooks->SoundEmitterPitch : NULL,
                                "SoundEmitterPitch", AE_ANIMDRAW_RADAR_LOOP_B,
                                (uint32_t)current_b,
                                emitter_pitch, 0u);
            if (ae_ram_failed(r)) return;
            (void)animdraw_call(r, hooks, hooks ? hooks->PadRumble : NULL,
                                "PadRumble", 1u, rumble_strength, 2u, 0u);
        }
    } else {
        animdraw_radar_stop_voice(r, hooks, voice_b, AE_ANIMDRAW_RADAR_LOOP_B);
        if (ae_ram_failed(r)) return;
        if (ae_rds16(r, voice_a) == -1) {
            int32_t voice = animdraw_call(r, hooks,
                                          hooks ? hooks->SoundLoopStart : NULL,
                                          "SoundLoopStart", 0u,
                                          AE_ANIMDRAW_RADAR_LOOP_A, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, voice_a, (uint32_t)voice);
        }
    }
}

static void animdraw_radar_finish(AeRam *r)
{
    uint32_t signal_record = AE_ANIMDRAW_G_RADAR_SIGNAL;
    if (ae_rd32(r, signal_record + AE_ANIMDRAW_RADAR_STATE_FOUND) != 0u) {
        uint32_t target = ae_rd32(r, signal_record + AE_ANIMDRAW_RADAR_STATE_TARGET);
        if (ae_MonkeyFlag248Set(r, target) == 0)
            ae_wr32(r, signal_record + AE_ANIMDRAW_RADAR_STATE_FOUND, 0u);
    }
}

void ae_RadarUpdateDraw(AeRam *r, const AeAnimDrawHooks *hooks)
{
    uint32_t signal_record = AE_ANIMDRAW_G_RADAR_SIGNAL;
    uint32_t state_argument = (ae_rd32(r, AE_ANIMDRAW_G_RADAR_STATE) <<
                               AE_ANIMDRAW_RADAR_GATE_SHIFT);
    if ((ae_rd32(r, AE_ANIMDRAW_AREA_STATE + AE_ANIMDRAW_RADAR_FLAG) & 1u) != 0u) {
        ae_GadgetDrawWithStateCheck(r, hooks ? hooks->gadget_hooks : NULL,
                                    AE_ANIMDRAW_G_RADAR_TRANSFORM,
                                    AE_ANIMDRAW_G_STATIC_MATRIX,
                                    signal_record + 4u, state_argument);
        animdraw_radar_finish(r);
        return;
    }

    if (ae_PlayerCanActivateGadget(r, 2u) == 0 ||
        ae_rd16(r, AE_ANIMDRAW_G_RADAR_GATE + AE_ANIMDRAW_RADAR_GATE_ENABLE) == 0u ||
        ae_rds16(r, AE_ANIMDRAW_G_RADAR_GATE_STATE) <= 0) {
        (void)animdraw_call(r, hooks, hooks ? hooks->TurnTowardsY : NULL,
                            "TurnTowardsY", AE_ANIMDRAW_G_RADAR_TURN_BACK,
                            signal_record + 4u, 341u, 0u);
        if (ae_ram_failed(r)) return;
        ae_GadgetDrawWithStateCheck(r, hooks ? hooks->gadget_hooks : NULL,
                                    AE_ANIMDRAW_G_RADAR_TRANSFORM,
                                    AE_ANIMDRAW_G_STATIC_MATRIX,
                                    signal_record + 4u, 0u);
        if (ae_ram_failed(r)) return;
        animdraw_radar_stop_voice(r, hooks,
                                  AE_ANIMDRAW_G_RADAR_VOICES + AE_ANIMDRAW_RADAR_VOICE_A,
                                  AE_ANIMDRAW_RADAR_LOOP_A);
        if (ae_ram_failed(r)) return;
        animdraw_radar_stop_voice(r, hooks,
                                  AE_ANIMDRAW_G_RADAR_VOICES + AE_ANIMDRAW_RADAR_VOICE_B,
                                  AE_ANIMDRAW_RADAR_LOOP_B);
        if (ae_ram_failed(r)) return;
        animdraw_radar_finish(r);
        return;
    }

    (void)animdraw_call(r, hooks, hooks ? hooks->TurnTowardsY : NULL,
                        "TurnTowardsY", AE_ANIMDRAW_G_RADAR_TURN_FROM,
                        signal_record + 4u, 341u, 0u);
    if (ae_ram_failed(r)) return;
    int32_t signal = ae_MonkeyRadarSignal(r, hooks);
    if (ae_ram_failed(r)) return;
    state_argument = ae_rd32(r, AE_ANIMDRAW_G_RADAR_STATE) <<
                     AE_ANIMDRAW_RADAR_GATE_SHIFT;
    ae_GadgetDrawWithStateCheck(r, hooks ? hooks->gadget_hooks : NULL,
                                AE_ANIMDRAW_G_RADAR_TRANSFORM,
                                AE_ANIMDRAW_G_STATIC_MATRIX,
                                signal_record + 4u, state_argument);
    if (ae_ram_failed(r)) return;

    if (signal > 0) {
        animdraw_radar_draw_model(r, hooks, (uint32_t)signal, 1);
        if (ae_ram_failed(r)) return;
    } else {
        animdraw_radar_fill_and_emit(r, hooks, 0x00202020u, 0x20u);
        if (ae_ram_failed(r)) return;
    }
    animdraw_radar_loop_update(r, hooks, signal);
    if (ae_ram_failed(r)) return;

    animdraw_radar_finish(r);
}

static int animdraw_random_range(AeRam *r, const AeAnimDrawHooks *hooks,
                                 uint32_t output, uint32_t limit)
{
    (void)animdraw_call(r, hooks, hooks ? hooks->RandomRange : NULL,
                        "RandomRange", output, 0u, limit, 0u);
    return !ae_ram_failed(r);
}

static void animdraw_draw_nop(AeRam *r, const AeAnimDrawHooks *hooks)
{
    (void)animdraw_call(r, hooks, hooks ? hooks->Draw_Nop : NULL,
                        "Draw_Nop", 1u, AE_ANIMDRAW_PLAYER_DRAW_NOP_FLAGS, 0u, 0u);
}

static int animdraw_player_stop_gadget_sounds(AeRam *r,
                                              const AeAnimDrawHooks *hooks,
                                              uint32_t gadget)
{
    (void)animdraw_call(r, hooks,
                        hooks ? hooks->PlayerStopGadgetSounds : NULL,
                        "PlayerStopGadgetSounds", gadget, 0u, 0u, 0u);
    return !ae_ram_failed(r);
}

static int32_t animdraw_player_scale(AeRam *r, int32_t size)
{
    int32_t sine_angle = (int32_t)((uint32_t)size * 300u);
    int32_t sine = ae_sdk_rsin(r, sine_angle);
    int32_t sine_product = (int32_t)((uint32_t)sine * (uint32_t)size);
    int32_t sine_divisor = sine_product / 240;
    int32_t size_product = (int32_t)((uint32_t)size << 12);
    int32_t size_divisor = size_product / 80;
    return (int32_t)(AE_ANIMDRAW_PLAYER_SCALE_ONE - sine_divisor - size_divisor);
}

static int animdraw_player_call_gadget(AeRam *r, const AeAnimDrawHooks *hooks,
                                       uint32_t gadget_state)
{
    const AeGadgetHooks *gh = hooks ? hooks->gadget_hooks : NULL;
    switch (gadget_state) {
    case 0u:
        ae_GadgetSwingUpdate(r, gh);
        break;
    case 1u:
        ae_PlayerHeadLookUpdate(r, hooks);
        break;
    case 2u:
        ae_RadarUpdateDraw(r, hooks);
        break;
    case 3u:
        ae_Gadget3UpdateDraw(r, gh);
        break;
    case 4u:
        ae_Gadget4UpdateDraw(r, gh);
        break;
    case 5u:
        ae_Gadget4PlayerUpdate(r, gh);
        break;
    case 6u:
        ae_Gadget6UpdateSound(r, gh);
        break;
    case 7u:
        ae_GadgetDrawStaticModel(r, gh);
        break;
    case 8u: {
        uint32_t state = ae_rds8(r, AE_ANIMDRAW_G_PLAYER_GADGET_MODE);
        uint32_t instance = AE_ANIMDRAW_PLAYER_MODEL_INSTANCE_BASE +
                            state * AE_STATIC_MODEL_SIZE;
        uint32_t model_header = ae_rd32(r, instance + AE_STATIC_MODEL_MODEL);
        uint32_t matrix = AE_ANIMDRAW_PLAYER_MODEL_MATRIX_POOL +
                          ((uint32_t)ae_rd16(r, model_header + AE_ANIMDRAW_MODEL_BONE_COUNT) << 5);
        (void)animdraw_call(r, hooks,
                            hooks ? hooks->ModelTransformRigid : NULL,
                            "ModelTransformRigid", instance, matrix,
                            AE_ANIMDRAW_PLAYER_MODEL_STATIC, 0u);
        if (ae_ram_failed(r)) return 0;
        (void)animdraw_call(r, hooks, hooks ? hooks->ModelDraw : NULL,
                            "ModelDraw", animdraw_radar_cycle_table(r),
                            AE_ANIMDRAW_PLAYER_MODEL_STATIC, instance, 0u);
        break;
    }
    default:
        break;
    }
    return !ae_ram_failed(r);
}

void ae_PlayerAnimateAndDraw(AeRam *r, const AeAnimDrawHooks *hooks)
{
    uint32_t player = AE_ANIMDRAW_PLAYER;
    uint32_t area_state = AE_ANIMDRAW_AREA_STATE;
    uint32_t scratch = AE_ANIMDRAW_SCRATCHPAD_BASE;
    uint32_t bone_matrices = ae_rd32(r, player + AE_ANIMDRAW_PLAYER_BONE_MATS);
    uint16_t player_state = ae_rd16(r, player + AE_ANIMDRAW_PLAYER_STATE);
    uint16_t blend_mode = ae_rd16(r, player + AE_ANIMDRAW_PLAYER_FRAME_MODE);
    uint32_t player_flags = ae_rd32(r, player + AE_ANIMDRAW_PLAYER_FLAGS);

    if (ae_rd16(r, area_state + AE_ANIMDRAW_AREA_LEVEL_FIELD - 2u) ==
        AE_ANIMDRAW_PLAYER_SPECIAL_STATE) return;

    if ((player_flags & 0xF0u) == 0x20u ||
        blend_mode == AE_ANIMDRAW_PLAYER_BLEND_NEXT) {
        uint32_t frame = ae_rd16(r, player + AE_ANIMDRAW_PLAYER_ANIM_FRAME);
        uint32_t next_frame = (frame + 1u) & 0xFFFFu;
        ae_wr16(r, player + AE_ANIMDRAW_PLAYER_FRAME_NEXT, next_frame);
        ae_AnimDecodeLerp(r, player, frame, next_frame,
                          ae_rd16(r, player + AE_ANIMDRAW_PLAYER_FRAME_BLEND));
    } else if (blend_mode == AE_ANIMDRAW_PLAYER_BLEND_FRAME) {
        ae_AnimDecodeLerp(r, player,
                          ae_rd16(r, player + AE_ANIMDRAW_PLAYER_ANIM_FRAME),
                          ae_rd16(r, player + AE_ANIMDRAW_PLAYER_FRAME_NEXT),
                          ae_rd16(r, player + AE_ANIMDRAW_PLAYER_FRAME_BLEND));
    } else if (blend_mode == AE_ANIMDRAW_PLAYER_BLEND_DISABLED) {
        ae_AnimDecodeFrame(r, player);
    } else {
        uint32_t frame_a = ae_AnimFramePtr(r, player,
                                           ae_rd16(r, player + AE_ANIMDRAW_PLAYER_ANIM_FRAME));
        uint32_t frame_b = ae_rd32(r, player + AE_ANIMDRAW_PLAYER_FRAME_PTR);
        uint32_t fraction = (uint32_t)blend_mode * AE_ANIMDRAW_PLAYER_ANIM_BLEND_SCALE;
        ae_AnimDecodeLerpPtr(r, player, frame_a, frame_b, fraction);
    }
    if (ae_ram_failed(r)) return;

    if ((ae_rd32(r, player + AE_ANIMDRAW_PLAYER_MIRROR_FLAGS) & 1u) != 0u) {
        ae_MatrixCopy8(r, bone_matrices, player + AE_ANIMDRAW_PLAYER_MIRROR_MATRIX);
    } else if ((player_state >= AE_ANIMDRAW_PLAYER_STATE_70 &&
                player_state < AE_ANIMDRAW_PLAYER_STATE_72) ||
               player_state == AE_ANIMDRAW_PLAYER_STATE_76) {
        ae_MatrixFromFrontUp(r, player + AE_ANIMDRAW_PLAYER_FRONT,
                             player + AE_ANIMDRAW_PLAYER_UP,
                             bone_matrices, 1u);
    } else {
        uint32_t front = AE_ANIMDRAW_G_PLAYER_FACING_VECTOR;
        ae_MatrixFromFrontUp(r, front, front + AE_ANIMDRAW_VECTOR_WORD_Z,
                             bone_matrices, 0u);
    }
    if (ae_ram_failed(r)) return;

    int32_t squash = ae_rds16(r, player + AE_ANIMDRAW_PLAYER_MODEL_SIZE);
    if (squash > 0) {
        if (!animdraw_random_range(r, hooks, AE_ANIMDRAW_PLAYER_EFFECT_TEMP, 16u)) return;
        int32_t y_scale = animdraw_player_scale(r, squash);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, AE_ANIMDRAW_PLAYER_SCALE_TEMP + AE_ANIMDRAW_PLAYER_SCALE_X,
                AE_ANIMDRAW_PLAYER_SCALE_ONE);
        ae_wr32(r, AE_ANIMDRAW_PLAYER_SCALE_TEMP + AE_ANIMDRAW_PLAYER_SCALE_Y,
                (uint32_t)y_scale);
        ae_wr32(r, AE_ANIMDRAW_PLAYER_SCALE_TEMP + AE_ANIMDRAW_PLAYER_SCALE_Z,
                AE_ANIMDRAW_PLAYER_SCALE_ONE);
        ae_wr32(r, AE_ANIMDRAW_PLAYER_SCALE_TEMP + AE_ANIMDRAW_PLAYER_SCALE_W,
                ae_rd32(r, AE_ANIMDRAW_PLAYER_EFFECT_TEMP + AE_ANIMDRAW_PLAYER_EFFECT_WORD_D));
        (void)ae_sdk_ScaleMatrix(r, bone_matrices, AE_ANIMDRAW_PLAYER_SCALE_TEMP);
        if (ae_ram_failed(r)) return;
    }

    if (ae_rd8(r, AE_ANIMDRAW_G_PLAYER_SPECIAL_FLAG) == 1u) {
        uint32_t angles = AE_ANIMDRAW_PLAYER_ANGLES;
        int32_t adjust_y = ae_rds8(r, AE_ANIMDRAW_G_PLAYER_SPECIAL_ADJUST) * 4;
        int32_t adjust_z = ae_rds8(r, AE_ANIMDRAW_G_PLAYER_SPECIAL_ANGLE_ADJUST) * 4;
        ae_wr16(r, angles + AE_ANIMDRAW_PLAYER_BONE_ANGLE_Y,
                ae_rd16(r, angles + AE_ANIMDRAW_PLAYER_BONE_ANGLE_Y) - (uint32_t)adjust_y);
        ae_wr16(r, angles + AE_ANIMDRAW_PLAYER_BONE_ANGLE_Z,
                ae_rd16(r, angles + AE_ANIMDRAW_PLAYER_BONE_ANGLE_Z) - (uint32_t)adjust_z);
    }
    if (ae_ram_failed(r)) return;

    uint32_t pose_flags = ae_rd16(r, area_state + AE_ANIMDRAW_AREA_LEVEL_FIELD - 2u) == 6u ?
                          7u : 3u;
    ae_ModelBuildBoneMatrices(r, player, pose_flags);
    if (ae_ram_failed(r)) return;
    (void)animdraw_call(r, hooks, hooks ? hooks->ModelTransformSkinned : NULL,
                        "ModelTransformSkinned", player, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    if ((ae_rd16(r, player + AE_ANIMDRAW_PLAYER_WOBBLE_MODE) >> 12) == 1u &&
        (ae_rd32(r, area_state + AE_ANIMDRAW_AREA_WOBBLE_GATE) >> 12) != 1u) {
        ae_wr16(r, AE_ANIMDRAW_PLAYER_WOBBLE_GTE_VECTOR,
                ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_X));
        ae_wr16(r, AE_ANIMDRAW_PLAYER_WOBBLE_GTE_VECTOR + AE_ANIMDRAW_VECTOR_Y,
                ae_rd16(r, player + AE_ANIMDRAW_PLAYER_WOBBLE_SCALE));
        ae_wr16(r, AE_ANIMDRAW_PLAYER_WOBBLE_GTE_VECTOR + AE_ANIMDRAW_VECTOR_Z,
                ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_Z));
        (void)animdraw_call(r, hooks, hooks ? hooks->SetRotMatrix : NULL,
                            "SetRotMatrix", AE_ANIMDRAW_G_CAMERA_MATRIX, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        (void)animdraw_call(r, hooks, hooks ? hooks->SetTransMatrix : NULL,
                            "SetTransMatrix", AE_ANIMDRAW_G_CAMERA_MATRIX, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        int32_t gte_result = animdraw_call(r, hooks,
                                           hooks ? hooks->RendererWobbleGte : NULL,
                                           "RendererWobbleGte",
                                           AE_ANIMDRAW_G_CAMERA_MATRIX,
                                           AE_ANIMDRAW_PLAYER_WOBBLE_GTE_VECTOR,
                                           player, 0u);
        if (ae_ram_failed(r)) return;
        uint32_t model = ae_rd32(r, player + AE_ANIMDRAW_PLAYER_MODEL);
        (void)animdraw_call(r, hooks, hooks ? hooks->VertexWobble : NULL,
                            "VertexWobble", AE_ANIMDRAW_PLAYER_MODEL_STATIC,
                            ae_rd16(r, model + AE_ANIMDRAW_CAR_CYCLE_COUNT_FIELD),
                            (uint32_t)animdraw_sra(gte_result, 16u), 0u);
        if (ae_ram_failed(r)) return;
    }

    if (ae_rds16(r, player + AE_ANIMDRAW_PLAYER_ENV_MAP) != 0) {
        (void)animdraw_call(r, hooks, hooks ? hooks->ModelCalcEnvUVs : NULL,
                            "ModelCalcEnvUVs", player, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        uint32_t cycle_table = animdraw_radar_cycle_table(r);
        ae_wr8(r, scratch + AE_ANIMDRAW_SCRATCH_RENDER_FLAGS, 2u);
        (void)animdraw_call(r, hooks,
                            hooks ? hooks->ModelDrawEnvMap : NULL,
                            "ModelDrawEnvMap", cycle_table,
                            AE_ANIMDRAW_PLAYER_MODEL_STATIC, player,
                            AE_ANIMDRAW_G_PLAYER_MODEL_DRAW_DATA);
        if (ae_ram_failed(r)) return;

        if (!animdraw_random_range(r, hooks, AE_ANIMDRAW_PLAYER_RING_TEMP,
                                   AE_ANIMDRAW_PLAYER_RING_RANDOM_RANGE)) return;
        ae_wr16(r, AE_ANIMDRAW_PLAYER_RING_TEMP + AE_ANIMDRAW_PLAYER_RING_POSITION_X,
                ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_X));
        ae_wr16(r, AE_ANIMDRAW_PLAYER_RING_TEMP + AE_ANIMDRAW_PLAYER_RING_POSITION_Y,
                ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_Y) - AE_ANIMDRAW_PLAYER_RING_Y_OFFSET);
        ae_wr16(r, AE_ANIMDRAW_PLAYER_RING_TEMP + AE_ANIMDRAW_PLAYER_RING_POSITION_Z,
                ae_rd16(r, player + AE_ANIMDRAW_PLAYER_POS_Z));
        ae_wr32(r, AE_ANIMDRAW_PLAYER_EFFECT_TEMP,
                ae_rd32(r, AE_ANIMDRAW_PLAYER_RING_TEMP));
        ae_wr32(r, AE_ANIMDRAW_PLAYER_EFFECT_TEMP + AE_ANIMDRAW_PLAYER_EFFECT_WORD_B,
                ae_rd32(r, AE_ANIMDRAW_PLAYER_RING_TEMP + AE_ANIMDRAW_PLAYER_RING_EFFECT_WORD));
        (void)animdraw_call(r, hooks, hooks ? hooks->EffectRingDraw : NULL,
                            "EffectRingDraw", AE_ANIMDRAW_PLAYER_EFFECT_TEMP,
                            AE_ANIMDRAW_PLAYER_RING_SIZE,
                            AE_ANIMDRAW_PLAYER_RING_KIND,
                            AE_ANIMDRAW_PLAYER_RING_FLAGS);
        if (ae_ram_failed(r)) return;
    }

    uint32_t stage = AE_ANIMDRAW_G_PLAYER_STAGE;
    uint32_t stage_mode = ae_rd8(r, stage + AE_ANIMDRAW_STAGE_MODE);
    if (ae_rd16(r, player + AE_ANIMDRAW_PLAYER_ENV_MAP) == 0u &&
        stage_mode != 1u && ae_rd8(r, stage + AE_ANIMDRAW_STAGE_AIM_MODE) != 1u &&
        !(ae_rds16(r, AE_ANIMDRAW_G_PLAYER_STAGE_GLOBAL) > 0 &&
          (ae_rd16(r, AE_ANIMDRAW_G_PLAYER_STAGE_GLOBAL) & 1u) != 0u)) {
        uint32_t red = (uint32_t)ae_rd8(r, player + AE_ANIMDRAW_PLAYER_COLOR) << 4;
        uint32_t green = (uint32_t)ae_rd8(r, player + AE_ANIMDRAW_PLAYER_COLOR +
                                         AE_ANIMDRAW_PLAYER_COLOR_GREEN) << 4;
        uint32_t blue = (uint32_t)ae_rd8(r, player + AE_ANIMDRAW_PLAYER_COLOR +
                                        AE_ANIMDRAW_PLAYER_COLOR_BLUE) << 4;
        ae_SetColorMatrixMode(r, hooks ? hooks->items_hooks : NULL,
                              AE_ANIMDRAW_PLAYER_COLOR_MATRIX_MODE,
                              red, green, blue);
        if (ae_ram_failed(r)) return;
        if (ae_rd32(r, player + AE_ANIMDRAW_PLAYER_COLOR) !=
            ae_rd32(r, player + AE_ANIMDRAW_PLAYER_COLOR_TARGET)) {
            for (uint32_t channel = 0; channel < 3u; ++channel) {
                uint32_t offset = AE_ANIMDRAW_PLAYER_COLOR + channel;
                int32_t value = ae_StepToward(r, ae_rd8(r, player + offset),
                                              ae_rd8(r, player +
                                                  AE_ANIMDRAW_PLAYER_COLOR_TARGET + channel),
                                              4);
                ae_wr8(r, player + offset, (uint32_t)value);
            }
        }
        if (ae_ram_failed(r)) return;
        (void)animdraw_call(r, hooks, hooks ? hooks->ModelDraw : NULL,
                            "ModelDraw", animdraw_radar_cycle_table(r),
                            AE_ANIMDRAW_PLAYER_MODEL_STATIC, player, 0u);
        if (ae_ram_failed(r)) return;
        ae_SetColorMatrixMode(r, hooks ? hooks->items_hooks : NULL,
                              AE_ANIMDRAW_PLAYER_COLOR_MATRIX_RESET, 0u, 0u, 0u);
        if (ae_ram_failed(r)) return;
    }

    if (stage_mode == 1u) {
        if ((int8_t)ae_rd8(r, AE_ANIMDRAW_G_PLAYER_GADGET_MODE) == 4 &&
            ae_rds16(r, AE_ANIMDRAW_G_RADAR_SIGNAL + AE_ANIMDRAW_RADAR_SIGNAL_AREA_FLAG) == 1) {
            ae_wr16(r, AE_ANIMDRAW_G_RADAR_SIGNAL + AE_ANIMDRAW_RADAR_SIGNAL_AREA_FLAG, 0u);
            ae_wr32(r, AE_ANIMDRAW_G_RADAR_SIGNAL + AE_ANIMDRAW_RADAR_SIGNAL_TARGET, 0u);
            if (!animdraw_player_stop_gadget_sounds(r, hooks, 4u)) return;
        }
    } else if (ae_player_is_swimming(player_state)) {
        (void)animdraw_call(r, hooks,
                            hooks ? hooks->ModelTransformRigid : NULL,
                            "ModelTransformRigid", AE_ANIMDRAW_G_PLAYER_SWIM_MODEL,
                            AE_ANIMDRAW_G_PLAYER_SWIM_MODEL_MATRIX,
                            AE_ANIMDRAW_PLAYER_MODEL_STATIC, 0u);
        if (ae_ram_failed(r)) return;
        (void)animdraw_call(r, hooks, hooks ? hooks->ModelDraw : NULL,
                            "ModelDraw", animdraw_radar_cycle_table(r),
                            AE_ANIMDRAW_PLAYER_MODEL_STATIC,
                            AE_ANIMDRAW_G_PLAYER_SWIM_MODEL, 0u);
        if (ae_ram_failed(r)) return;

        if (stage_mode != 1u &&
            (ae_rd32(r, AE_ANIMDRAW_G_PLAYER_SWIM_GATE + AE_ANIMDRAW_PLAYER_SWIM_GATE_OFFSET) &
             ae_rd32(r, AE_ANIMDRAW_G_PLAYER_SWIM_MASK) & 0x400u) != 0u &&
            ae_rds16(r, AE_ANIMDRAW_G_RADAR_GATE_STATE) > 0 &&
            ae_rd32(r, AE_ANIMDRAW_G_PLAYER_SWIM_SOUND_ACTIVE) == 0u) {
            ae_PlayerRumbleSlam(r, hooks);
            if (ae_ram_failed(r)) return;
        }
        if (ae_rd32(r, AE_ANIMDRAW_G_PLAYER_SWIM_SOUND_ACTIVE) > 0u) {
            ae_GadgetSelectFxDraw(r, hooks ? hooks->gadget_hooks : NULL);
            if (ae_ram_failed(r)) return;
        }
        ae_PlayerSwimSfxUpdate(r, hooks);
        if (ae_ram_failed(r)) return;
    } else {
        uint32_t voices = AE_ANIMDRAW_G_PLAYER_SWIM_VOICES;
        int32_t left = ae_rds16(r, voices + AE_ANIMDRAW_SOUND_LOOP_LEFT);
        if (left != -1) {
            int32_t right = ae_rds16(r, voices + AE_ANIMDRAW_SOUND_LOOP_RIGHT);
            (void)animdraw_call(r, hooks,
                                hooks ? hooks->SoundLoopStop : NULL,
                                "SoundLoopStop", (uint32_t)right,
                                (uint32_t)left, 0u, 0u);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, voices + AE_ANIMDRAW_SOUND_LOOP_LEFT,
                    AE_ANIMDRAW_SOUND_LOOP_OFF);
            ae_wr16(r, voices + AE_ANIMDRAW_SOUND_LOOP_RIGHT,
                    AE_ANIMDRAW_SOUND_LOOP_OFF);
        }

        uint32_t gadget_state = ae_rd8(r, AE_ANIMDRAW_G_PLAYER_GADGET_MODE);
        if (gadget_state < 10u) {
            (void)animdraw_player_call_gadget(r, hooks, gadget_state);
            if (ae_ram_failed(r)) return;
        } else {
            int32_t signed_state = ae_rds8(r, AE_ANIMDRAW_G_PLAYER_GADGET_MODE);
            uint32_t instance = AE_ANIMDRAW_G_PLAYER_DYNAMIC_MODELS +
                                (uint32_t)signed_state * AE_STATIC_MODEL_SIZE;
            uint32_t model_header = ae_rd32(r, instance + AE_STATIC_MODEL_MODEL);
            uint32_t matrix = AE_ANIMDRAW_G_PLAYER_MATRIX_BASE +
                              ((uint32_t)ae_rd16(r, model_header + AE_ANIMDRAW_MODEL_BONE_COUNT) << 5);
            (void)animdraw_call(r, hooks,
                                hooks ? hooks->ModelTransformRigid : NULL,
                                "ModelTransformRigid", instance, matrix,
                                AE_ANIMDRAW_PLAYER_MODEL_STATIC, 0u);
            if (ae_ram_failed(r)) return;
            (void)animdraw_call(r, hooks, hooks ? hooks->ModelDraw : NULL,
                                "ModelDraw", animdraw_radar_cycle_table(r),
                                AE_ANIMDRAW_PLAYER_MODEL_STATIC, instance, 0u);
            if (ae_ram_failed(r)) return;
        }
    }

    animdraw_draw_nop(r, hooks);
    if (ae_ram_failed(r)) return;

    int32_t shadow_x = ae_rds16(r, player + AE_ANIMDRAW_PLAYER_SHADOW_POS);
    int32_t shadow_y = ae_rds16(r, player + AE_ANIMDRAW_PLAYER_SHADOW_POS +
                               AE_ANIMDRAW_PLAYER_SHADOW_VECTOR_Y);
    int32_t shadow_z = ae_rds16(r, player + AE_ANIMDRAW_PLAYER_SHADOW_POS +
                               AE_ANIMDRAW_PLAYER_SHADOW_VECTOR_Z);
    ae_wr32(r, scratch + AE_ANIMDRAW_STACK_SHADOW_X, (uint32_t)shadow_x);
    ae_wr32(r, scratch + AE_ANIMDRAW_STACK_SHADOW_Y, (uint32_t)shadow_y);
    ae_wr32(r, scratch + AE_ANIMDRAW_STACK_SHADOW_Z, (uint32_t)shadow_z);
    if (player_state == AE_ANIMDRAW_PLAYER_STATE_14 ||
        player_state == AE_ANIMDRAW_PLAYER_STATE_15) {
        int32_t facing_x = animdraw_sra((int32_t)((uint32_t)ae_rd16(
                                     r, player + AE_ANIMDRAW_PLAYER_LOOK_X) << 16), 24u);
        int32_t facing_z = animdraw_sra((int32_t)((uint32_t)ae_rd16(
                                     r, player + AE_ANIMDRAW_PLAYER_LOOK_Z) << 16), 24u);
        ae_wr32(r, scratch + AE_ANIMDRAW_STACK_SHADOW_X, (uint32_t)((int32_t)((uint32_t)shadow_x -
                                                          (uint32_t)facing_x)));
        ae_wr32(r, scratch + AE_ANIMDRAW_STACK_SHADOW_Z, (uint32_t)((int32_t)((uint32_t)shadow_z -
                                                          (uint32_t)facing_z)));
    }
    if (ae_rd8(r, stage + AE_ANIMDRAW_STAGE_MODE) != 1u &&
        ae_rd8(r, stage + AE_ANIMDRAW_STAGE_AIM_MODE) != 1u &&
        ae_rd16(r, player + AE_ANIMDRAW_PLAYER_DRAW_ENABLED) != 0u) {
        (void)animdraw_call(r, hooks, hooks ? hooks->DrawShadow : NULL,
                            "DrawShadow", scratch + AE_ANIMDRAW_STACK_SHADOW_X,
                            ae_rd32(r, player + AE_ANIMDRAW_PLAYER_SHADOW_FLAGS),
                            AE_ANIMDRAW_PLAYER_SHADOW_RADIUS,
                            ae_rd32(r, player + AE_ANIMDRAW_PLAYER_SHADOW_TARGET));
    }
}
