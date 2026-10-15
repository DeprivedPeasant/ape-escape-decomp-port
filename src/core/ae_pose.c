/* Fixed-frame actor animation and channel-based model posing for SCUS_944.23. */
#include "ae_pose.h"

#include "ae_sdk.h"
#include "ae_sdk_matrix.h"

#include <limits.h>
#include <stdint.h>

#define AE_POSE_GLOBAL_ANGLES 0x800F4B30u
#define AE_POSE_GLOBAL_BONES 0x800F4730u
#define AE_POSE_VIEW_MATRIX 0x800E78C0u
#define AE_POSE_STACK 0x801FFEC0u

static int32_t signed_bits(uint32_t value, uint32_t shift, uint32_t bits)
{
    uint32_t mask = (1u << bits) - 1u;
    uint32_t component = (value >> shift) & mask;
    uint32_t sign = 1u << (bits - 1u);
    return component & sign ? (int32_t)component - (int32_t)(1u << bits) : (int32_t)component;
}

static int32_t arithmetic_shift_right(int32_t value, uint32_t amount)
{
    amount &= 31u;
    if (amount == 0) return value;
    if (value >= 0) return (int32_t)((uint32_t)value >> amount);
    return (int32_t)~((~(uint32_t)value) >> amount);
}

static int32_t interpolate_component(int32_t first, int32_t second, uint32_t fraction)
{
    uint32_t complement = 4096u - fraction;
    uint32_t total = (uint32_t)first * complement + (uint32_t)second * fraction;
    int32_t weighted = (int32_t)total;
    if (weighted < 0) weighted = (int32_t)((uint32_t)weighted + 4095u);
    return arithmetic_shift_right(weighted, 12);
}

static int32_t divide_or_fail(AeRam *r, int32_t numerator, int32_t denominator, const char *context)
{
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(r, "%s: signed divide trap (the original executes BREAK)", context);
        return 0;
    }
    return numerator / denominator;
}

static void copy_matrix(AeRam *r, uint32_t source, uint32_t destination)
{
    for (uint32_t offset = 0; offset < 32u; offset += 4u) {
        uint32_t word = ae_rd32(r, source + offset);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, destination + offset, word);
        if (ae_ram_failed(r)) return;
    }
}

static void set_actor_world_position(AeRam *r, uint32_t actor, uint32_t root_matrix, uint32_t additive)
{
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t matrix_translation = ae_rd32(r, root_matrix + 20u + axis * 4u);
        uint32_t actor_position = ae_rd32(r, actor + 0x24u + axis * 4u);
        uint32_t value = additive ? matrix_translation + actor_position : actor_position;
        ae_wr32(r, root_matrix + 20u + axis * 4u, value);
        if (ae_ram_failed(r)) return;
    }
}

static void copy_first_world_bone_position(AeRam *r, uint32_t actor)
{
    ae_wr16(r, actor + 0x1Cu, ae_rd16(r, AE_POSE_GLOBAL_BONES + 0x34u));
    ae_wr16(r, actor + 0x1Eu, ae_rd16(r, AE_POSE_GLOBAL_BONES + 0x38u));
    ae_wr16(r, actor + 0x20u, ae_rd16(r, AE_POSE_GLOBAL_BONES + 0x3Cu));
}

static void set_local_bone_translation(AeRam *r, uint32_t matrix, uint32_t bone_record)
{
    ae_wr32(r, matrix + 20u, (uint32_t)ae_rds16(r, bone_record + 4u));
    ae_wr32(r, matrix + 24u, (uint32_t)ae_rds16(r, bone_record + 6u));
    ae_wr32(r, matrix + 28u, (uint32_t)ae_rds16(r, bone_record + 8u));
}

uint32_t ae_AnimDecodeLerp(AeRam *r, uint32_t actor, uint32_t frame_a, uint32_t frame_b, uint32_t fraction)
{
    uint32_t model = ae_rd32(r, actor);
    uint32_t bone_count = ae_rd16(r, model + 4u);
    uint32_t animation = ae_rd32(r, actor + 0x18u);
    uint32_t frame_stride = (bone_count + 2u) * 4u;
    uint32_t first_frame = animation + frame_a * frame_stride;
    uint32_t second_frame = animation + frame_b * frame_stride;
    uint32_t bones = ae_rd32(r, actor + 0x0Cu);
    if (ae_ram_failed(r)) return 0;

    for (uint32_t bone = 0; bone < bone_count; ++bone) {
        uint32_t first_word = ae_rd32(r, first_frame + 8u + bone * 4u);
        uint32_t second_word = ae_rd32(r, second_frame + 8u + bone * 4u);
        int32_t first_x = signed_bits(first_word, 0, 10) * 4;
        int32_t first_y = signed_bits(first_word, 10, 12) * 4;
        int32_t first_z = signed_bits(first_word, 22, 10) * 4;
        int32_t second_x = signed_bits(second_word, 0, 10) * 4;
        int32_t second_y = signed_bits(second_word, 10, 12) * 4;
        int32_t second_z = signed_bits(second_word, 22, 10) * 4;
        if (ae_ram_failed(r)) return 0;

        int32_t difference = second_x - first_x;
        if (difference >= 2049) first_x += 4096;
        else if (difference <= -2049) second_x += 4096;
        difference = second_y - first_y;
        if (difference >= 2049) first_y += 4096;
        else if (difference <= -2049) second_y += 4096;
        difference = second_z - first_z;
        if (difference >= 2049) first_z += 4096;
        else if (difference <= -2049) second_z += 4096;

        uint32_t out = AE_POSE_GLOBAL_ANGLES + bone * 8u;
        ae_wr16(r, out, (uint32_t)interpolate_component(first_x, second_x, fraction));
        ae_wr16(r, out + 2u, (uint32_t)interpolate_component(first_y, second_y, fraction));
        ae_wr16(r, out + 4u, (uint32_t)interpolate_component(first_z, second_z, fraction));
        if (ae_ram_failed(r)) return 0;
    }

    uint32_t first_root = ae_rd32(r, first_frame + 4u);
    uint32_t second_root = ae_rd32(r, second_frame + 4u);
    int32_t root_x = interpolate_component(signed_bits(first_root, 0, 10),
                                           signed_bits(second_root, 0, 10), fraction);
    int32_t root_z = interpolate_component(signed_bits(first_root, 22, 10),
                                           signed_bits(second_root, 22, 10), fraction);
    int32_t root_y = interpolate_component(signed_bits(first_root, 10, 12),
                                           signed_bits(second_root, 10, 12), fraction);
    ae_wr16(r, bones + 4u, (uint32_t)root_x);
    ae_wr16(r, bones + 8u, (uint32_t)root_z);
    ae_wr16(r, bones + 6u, (uint32_t)root_y);
    return (uint32_t)root_y;
}

uint32_t ae_ActorAnimDecode(AeRam *r, uint32_t actor)
{
    uint32_t animation_index = ae_rd16(r, actor + 0x5Eu);
    uint32_t current_frame = ae_rd16(r, actor + 0x5Cu);
    uint32_t table = ae_rd32(r, actor + 0x16Cu);
    uint32_t entry = table + animation_index * 8u;
    uint32_t first = ae_rd16(r, entry);
    uint32_t interval = ae_rd16(r, entry + 4u);
    uint32_t first_key = ae_rd16(r, entry + 6u);
    if (ae_ram_failed(r)) return 0;

    int32_t frame_delta = (int32_t)current_frame - (int32_t)first;
    int32_t scaled_delta = (int32_t)((uint32_t)frame_delta << 12);
    int32_t fraction_units = divide_or_fail(r, scaled_delta, (int32_t)interval, "ActorAnimDecode weight");
    if (ae_ram_failed(r)) return 0;
    int32_t key_delta = divide_or_fail(r, frame_delta, (int32_t)interval, "ActorAnimDecode key");
    if (ae_ram_failed(r)) return 0;

    uint32_t frame_a = first_key + (uint32_t)key_delta;
    uint32_t frame_b = frame_a + 1u;
    int32_t whole_numerator = fraction_units;
    if (whole_numerator < 0) whole_numerator += 4095;
    uint32_t whole_fraction = (uint32_t)arithmetic_shift_right(whole_numerator, 12) << 12;
    uint32_t fraction = (uint32_t)fraction_units - whole_fraction;
    return ae_AnimDecodeLerp(r, actor, frame_a, frame_b, fraction);
}

void ae_ModelBuildBoneMatrices(AeRam *r, uint32_t actor, uint32_t flags)
{
    uint32_t model = ae_rd32(r, actor);
    uint32_t bones = ae_rd32(r, actor + 0x0Cu);
    uint32_t matrices = ae_rd32(r, actor + 0x14u);
    uint32_t bone_count = ae_rd16(r, model + 4u);
    if (ae_ram_failed(r)) return;

    set_actor_world_position(r, actor, matrices, (flags & 4u) != 0);
    if (ae_ram_failed(r)) return;

    if (flags & 2u) {
        copy_matrix(r, matrices, AE_POSE_GLOBAL_BONES);
        if (ae_ram_failed(r)) return;

        for (uint32_t bone = 0; bone < bone_count; ++bone) {
            uint32_t local_matrix = AE_POSE_GLOBAL_BONES + (bone + 1u) * 32u;
            uint32_t bone_record = bones + bone * 12u;
            ae_sdk_RotMatrixZYX(r, AE_POSE_GLOBAL_ANGLES + bone * 8u, local_matrix);
            if (ae_ram_failed(r)) return;
            set_local_bone_translation(r, local_matrix, bone_record);
            if (ae_ram_failed(r)) return;

            uint32_t parent = ae_rd16(r, bone_record + 10u);
            if (ae_ram_failed(r)) return;
            ae_sdk_CompMatrix(r, AE_POSE_GLOBAL_BONES + parent * 32u, local_matrix, local_matrix);
            if (ae_ram_failed(r)) return;
        }
        copy_first_world_bone_position(r, actor);
        if (ae_ram_failed(r)) return;
    }

    if (flags & 1u) {
        ae_sdk_CompMatrix(r, AE_POSE_VIEW_MATRIX, matrices, matrices);
        if (ae_ram_failed(r)) return;
        for (uint32_t bone = 0; bone < bone_count; ++bone) {
            uint32_t local_matrix = matrices + (bone + 1u) * 32u;
            uint32_t bone_record = bones + bone * 12u;
            ae_sdk_RotMatrixZYX(r, AE_POSE_GLOBAL_ANGLES + bone * 8u, local_matrix);
            if (ae_ram_failed(r)) return;
            set_local_bone_translation(r, local_matrix, bone_record);
            if (ae_ram_failed(r)) return;

            uint32_t parent = ae_rd16(r, bone_record + 10u);
            if (ae_ram_failed(r)) return;
            ae_sdk_CompMatrix(r, matrices + parent * 32u, local_matrix, local_matrix);
            if (ae_ram_failed(r)) return;
        }
    }
}

static int descriptor_matches(AeRam *r, uint32_t descriptor, uint32_t bone_number, uint32_t kind)
{
    return ae_rd8(r, descriptor) == bone_number && ae_rd8(r, descriptor + 2u) == kind;
}

static void decode_rotation_word(AeRam *r, uint32_t packed, uint32_t output)
{
    ae_wr16(r, output, (uint32_t)(signed_bits(packed, 0, 10) * 4));
    ae_wr16(r, output + 2u, (uint32_t)(signed_bits(packed, 10, 12) * 4));
    ae_wr16(r, output + 4u, (uint32_t)(signed_bits(packed, 22, 10) * 4));
}

static void decode_translation_word(AeRam *r, uint32_t matrix, uint32_t packed)
{
    ae_wr32(r, matrix + 20u, (uint32_t)signed_bits(packed, 0, 10));
    ae_wr32(r, matrix + 24u, (uint32_t)signed_bits(packed, 10, 12));
    ae_wr32(r, matrix + 28u, (uint32_t)signed_bits(packed, 22, 10));
}

static void decode_scale_word(AeRam *r, uint32_t output, uint32_t packed)
{
    ae_wr32(r, output, (uint32_t)(signed_bits(packed, 0, 10) * 16));
    ae_wr32(r, output + 4u, (uint32_t)(signed_bits(packed, 10, 12) * 16));
    ae_wr32(r, output + 8u, (uint32_t)(signed_bits(packed, 22, 10) * 16));
}

void ae_ModelPoseBones(AeRam *r, uint32_t actor, uint32_t flags)
{
    uint32_t animation = ae_rd32(r, actor + 0x18u);
    uint32_t frame = ae_rd16(r, actor + 0x5Cu);
    uint32_t model = ae_rd32(r, actor);
    uint32_t bones = ae_rd32(r, actor + 0x0Cu);
    uint32_t matrices = ae_rd32(r, actor + 0x14u);
    uint32_t channel_count = ae_rd32(r, animation);
    uint32_t bone_count = ae_rd16(r, model + 4u);
    uint32_t frame_index = channel_count * (frame + 1u);
    uint32_t descriptor = animation + 4u;
    uint32_t sample = descriptor + frame_index * 4u;
    if (ae_ram_failed(r)) return;

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr32(r, matrices + 20u + axis * 4u, ae_rd32(r, actor + 0x24u + axis * 4u));
        if (ae_ram_failed(r)) return;
    }
    copy_matrix(r, matrices, AE_POSE_GLOBAL_BONES);
    if (ae_ram_failed(r)) return;
    ae_sdk_CompMatrix(r, AE_POSE_VIEW_MATRIX, matrices, matrices);
    if (ae_ram_failed(r)) return;

    for (uint32_t bone = 0; bone < bone_count; ++bone) {
        uint32_t bone_number = bone + 1u;
        uint32_t bone_record = bones + bone * 12u;
        uint32_t local_matrix = matrices + bone_number * 32u;

        if (descriptor_matches(r, descriptor, bone_number, 1u)) {
            uint32_t packed = ae_rd32(r, sample);
            decode_rotation_word(r, packed, AE_POSE_STACK + 16u);
            if (ae_ram_failed(r)) return;
            descriptor += 4u;
            sample += 4u;
            ae_sdk_RotMatrixZYX(r, AE_POSE_STACK + 16u, local_matrix);
        } else {
            ae_sdk_MatrixIdentity(r, local_matrix);
        }
        if (ae_ram_failed(r)) return;

        if (descriptor_matches(r, descriptor, bone_number, 2u)) {
            uint32_t packed = ae_rd32(r, sample);
            decode_translation_word(r, local_matrix, packed);
            if (ae_ram_failed(r)) return;
            descriptor += 4u;
            sample += 4u;
        } else {
            set_local_bone_translation(r, local_matrix, bone_record);
            if (ae_ram_failed(r)) return;
        }

        if (descriptor_matches(r, descriptor, bone_number, 3u)) {
            uint32_t packed = ae_rd32(r, sample);
            decode_scale_word(r, AE_POSE_STACK + 24u, packed);
            if (ae_ram_failed(r)) return;
            descriptor += 4u;
            sample += 4u;
            ae_sdk_ScaleMatrix(r, local_matrix, AE_POSE_STACK + 24u);
            if (ae_ram_failed(r)) return;
        }

        uint32_t parent = ae_rd16(r, bone_record + 10u);
        if (ae_ram_failed(r)) return;
        if (flags & 2u) {
            ae_sdk_CompMatrix(r, AE_POSE_GLOBAL_BONES + parent * 32u, local_matrix,
                              AE_POSE_GLOBAL_BONES + bone_number * 32u);
            if (ae_ram_failed(r)) return;
        }
        ae_sdk_CompMatrix(r, matrices + parent * 32u, local_matrix, local_matrix);
        if (ae_ram_failed(r)) return;
    }
    copy_first_world_bone_position(r, actor);
}
