/* SDK matrix helpers translated from SCUS_944.23 instruction listings. */
#include "ae_sdk_matrix.h"

#include "ae_sdk.h"

#include <limits.h>
#include <stdint.h>

#define AE_SDK_SINCOS_TABLE 0x800AA5C0u
#define AE_LOOKDIR_STACK 0x801FFED0u

static int32_t signed16(uint32_t value)
{
    return value & 0x8000u ? (int32_t)(value & 0xFFFFu) - 0x10000 : (int32_t)(value & 0xFFFFu);
}

static int32_t arithmetic_shift_right(int32_t value, uint32_t amount)
{
    amount &= 31u;
    if (amount == 0) return value;
    if (value >= 0) return (int32_t)((uint32_t)value >> amount);
    return (int32_t)~((~(uint32_t)value) >> amount);
}

static int32_t negate32(int32_t value)
{
    return (int32_t)(0u - (uint32_t)value);
}

static int32_t low_product_shift12(int32_t left, int32_t right)
{
    uint32_t product = (uint32_t)left * (uint32_t)right;
    return arithmetic_shift_right((int32_t)product, 12);
}

static int32_t cross_component(int32_t a, int32_t b, int32_t c, int32_t d)
{
    uint32_t first = (uint32_t)a * (uint32_t)b;
    uint32_t second = (uint32_t)c * (uint32_t)d;
    int32_t value = (int32_t)(first - second);
    if (value < 0) value = (int32_t)((uint32_t)value + 4095u);
    return arithmetic_shift_right(value, 12);
}

static void sine_cosine(AeRam *r, int32_t angle, int32_t *sine, int32_t *cosine)
{
    uint32_t index = angle < 0 ? (uint32_t)negate32(angle) & 0xFFFu : (uint32_t)angle & 0xFFFu;
    uint32_t packed = ae_rd32(r, AE_SDK_SINCOS_TABLE + index * 4u);
    int32_t table_sine = signed16(packed);
    *sine = angle < 0 ? negate32(table_sine) : table_sine;
    *cosine = signed16(packed >> 16);
}

static int64_t floor_divide_4096(int64_t value)
{
    int64_t quotient = value / 4096;
    if (value % 4096 < 0) --quotient;
    return quotient;
}

/* Each axis mixes two rows in place. PsyQ adds/subtracts the low 32-bit
 * products before shifting, then truncates to a halfword (no saturation).
 * Translation and the unused row are preserved. */
static uint32_t rotate_rows(AeRam *r, int32_t angle, uint32_t matrix,
                            uint32_t first, uint32_t second, int reverse_sine)
{
    int32_t sine, cosine;
    sine_cosine(r, angle, &sine, &cosine);
    if (reverse_sine) sine = negate32(sine);
    for (uint32_t column = 0; column < 3; ++column) {
        uint32_t a = matrix + first * 6u + column * 2u;
        uint32_t b = matrix + second * 6u + column * 2u;
        int32_t left = ae_rds16(r, a), right = ae_rds16(r, b);
        uint32_t upper = (uint32_t)cosine * (uint32_t)left -
                         (uint32_t)sine * (uint32_t)right;
        uint32_t lower = (uint32_t)sine * (uint32_t)left +
                         (uint32_t)cosine * (uint32_t)right;
        ae_wr16(r, a, (uint32_t)arithmetic_shift_right((int32_t)upper, 12));
        ae_wr16(r, b, (uint32_t)arithmetic_shift_right((int32_t)lower, 12));
    }
    return matrix;
}

uint32_t ae_sdk_RotMatrixX(AeRam *r, int32_t angle, uint32_t matrix)
{
    return rotate_rows(r, angle, matrix, 1, 2, 0);
}

uint32_t ae_sdk_RotMatrixY(AeRam *r, int32_t angle, uint32_t matrix)
{
    return rotate_rows(r, angle, matrix, 0, 2, 1);
}

uint32_t ae_sdk_RotMatrixZ(AeRam *r, int32_t angle, uint32_t matrix)
{
    return rotate_rows(r, angle, matrix, 0, 1, 0);
}

static int16_t saturate_s16(int64_t value)
{
    if (value < INT16_MIN) return INT16_MIN;
    if (value > INT16_MAX) return INT16_MAX;
    return (int16_t)value;
}

static void write_s16_matrix_word(AeRam *r, uint32_t address, int32_t low, int32_t high)
{
    ae_wr32(r, address, ((uint32_t)low & 0xFFFFu) | (((uint32_t)high & 0xFFFFu) << 16));
}

uint32_t ae_sdk_RotMatrixZYX(AeRam *r, uint32_t angles, uint32_t matrix)
{
    int32_t sine_x, cosine_x, sine_y, cosine_y, sine_z, cosine_z;
    int32_t angle_x = ae_rds16(r, angles);
    int32_t angle_y = ae_rds16(r, angles + 2u);
    int32_t angle_z = ae_rds16(r, angles + 4u);
    int32_t rotation[3][3];
    if (ae_ram_failed(r)) return matrix;

    sine_cosine(r, angle_x, &sine_x, &cosine_x);
    sine_cosine(r, angle_y, &sine_y, &cosine_y);
    sine_cosine(r, angle_z, &sine_z, &cosine_z);

    /* Each nested product is shifted separately, as in the original MULTU/MFLO/SRA sequence. */
    rotation[0][0] = low_product_shift12(cosine_y, cosine_z);
    rotation[0][1] = (int32_t)((uint32_t)low_product_shift12(low_product_shift12(sine_x, sine_y), cosine_z) -
                                (uint32_t)low_product_shift12(sine_z, cosine_x));
    rotation[0][2] = (int32_t)((uint32_t)low_product_shift12(low_product_shift12(sine_y, cosine_x), cosine_z) +
                                (uint32_t)low_product_shift12(sine_x, sine_z));
    rotation[1][0] = low_product_shift12(sine_z, cosine_y);
    rotation[1][1] = (int32_t)((uint32_t)low_product_shift12(cosine_x, cosine_z) +
                                (uint32_t)low_product_shift12(low_product_shift12(sine_x, sine_y), sine_z));
    rotation[1][2] = (int32_t)((uint32_t)low_product_shift12(low_product_shift12(sine_y, cosine_x), sine_z) -
                                (uint32_t)low_product_shift12(sine_x, cosine_z));
    rotation[2][0] = negate32(sine_y);
    rotation[2][1] = low_product_shift12(sine_x, cosine_y);
    rotation[2][2] = low_product_shift12(cosine_x, cosine_y);
    if (ae_ram_failed(r)) return matrix;

    /* The SDK stores in this order. Only the 18 rotation bytes are touched. */
    ae_wr16(r, matrix + 12u, (uint32_t)rotation[2][0]);
    ae_wr16(r, matrix + 14u, (uint32_t)rotation[2][1]);
    ae_wr16(r, matrix + 16u, (uint32_t)rotation[2][2]);
    ae_wr16(r, matrix, (uint32_t)rotation[0][0]);
    ae_wr16(r, matrix + 6u, (uint32_t)rotation[1][0]);
    ae_wr16(r, matrix + 2u, (uint32_t)rotation[0][1]);
    ae_wr16(r, matrix + 8u, (uint32_t)rotation[1][1]);
    ae_wr16(r, matrix + 4u, (uint32_t)rotation[0][2]);
    ae_wr16(r, matrix + 10u, (uint32_t)rotation[1][2]);
    return matrix;
}

uint32_t ae_sdk_CompMatrix(AeRam *r, uint32_t left, uint32_t right, uint32_t out)
{
    int32_t left_rotation[3][3];
    int32_t right_rotation[3][3];
    int32_t left_translation[3];
    int32_t right_translation[3];
    int16_t result_rotation[3][3];
    int32_t result_translation[3];
    int64_t last_ir3 = 0;

    /* The MIPS routine starts by loading all five aligned words of the left rotation matrix. */
    uint32_t left_words[5];
    uint32_t right_words[5];
    for (uint32_t i = 0; i < 5u; ++i) {
        left_words[i] = ae_rd32(r, left + i * 4u);
        if (ae_ram_failed(r)) return out;
    }
    for (uint32_t i = 0; i < 5u; ++i) {
        right_words[i] = ae_rd32(r, right + i * 4u);
        if (ae_ram_failed(r)) return out;
    }

    /* Decode the nine s16 values from the saved words; the final word has an unused upper half. */
    for (uint32_t row = 0; row < 3u; ++row) {
        for (uint32_t col = 0; col < 3u; ++col) {
            uint32_t index = row * 3u + col;
            left_rotation[row][col] = signed16(left_words[index / 2u] >> ((index & 1u) * 16u));
            right_rotation[row][col] = signed16(right_words[index / 2u] >> ((index & 1u) * 16u));
        }
    }
    for (uint32_t row = 0; row < 3u; ++row) {
        left_translation[row] = ae_rds32(r, left + 20u + row * 4u);
        if (ae_ram_failed(r)) return out;
        uint32_t raw = ae_rd32(r, right + 20u + row * 4u);
        right_translation[row] = signed16(raw);
        if (ae_ram_failed(r)) return out;
    }

    for (uint32_t row = 0; row < 3u; ++row) {
        for (uint32_t col = 0; col < 3u; ++col) {
            int64_t mac = (int64_t)left_rotation[row][0] * right_rotation[0][col] +
                          (int64_t)left_rotation[row][1] * right_rotation[1][col] +
                          (int64_t)left_rotation[row][2] * right_rotation[2][col];
            result_rotation[row][col] = saturate_s16(floor_divide_4096(mac));
            if (row == 2u && col == 2u) last_ir3 = result_rotation[row][col];
        }
    }

    /* MVMVA writes the packed rotation in column order, then stores IR3 as a 32-bit word. */
    write_s16_matrix_word(r, out, result_rotation[0][0], result_rotation[0][1]);
    write_s16_matrix_word(r, out + 12u, result_rotation[2][0], result_rotation[2][1]);
    write_s16_matrix_word(r, out + 4u, result_rotation[0][2], result_rotation[1][0]);
    write_s16_matrix_word(r, out + 8u, result_rotation[1][1], result_rotation[1][2]);
    ae_wr32(r, out + 16u, (uint32_t)(int32_t)last_ir3);
    if (ae_ram_failed(r)) return out;

    for (uint32_t row = 0; row < 3u; ++row) {
        int64_t mac = (int64_t)left_rotation[row][0] * right_translation[0] +
                      (int64_t)left_rotation[row][1] * right_translation[1] +
                      (int64_t)left_rotation[row][2] * right_translation[2];
        int64_t translated = floor_divide_4096(mac) + left_translation[row];
        if (translated < INT32_MIN || translated > INT32_MAX) {
            ae_ram_fail(r, "CompMatrix: translation ADD overflow (the original traps)");
            return out;
        }
        result_translation[row] = (int32_t)translated;
    }
    for (uint32_t row = 0; row < 3u; ++row) {
        ae_wr32(r, out + 20u + row * 4u, (uint32_t)result_translation[row]);
    }
    return out;
}

uint32_t ae_sdk_ScaleMatrix(AeRam *r, uint32_t matrix, uint32_t scale)
{
    uint32_t scale_x = ae_rd32(r, scale);
    uint32_t scale_y = ae_rd32(r, scale + 4u);
    uint32_t scale_z = ae_rd32(r, scale + 8u);
    if (ae_ram_failed(r)) return matrix;

    const uint32_t offsets[] = {0u, 4u, 8u, 12u};
    const uint32_t scales_low[] = {scale_x, scale_z, scale_y, scale_x};
    const uint32_t scales_high[] = {scale_y, scale_x, scale_z, scale_y};
    for (uint32_t i = 0; i < 4u; ++i) {
        uint32_t address = matrix + offsets[i];
        uint32_t word = ae_rd32(r, address);
        int32_t first = signed16(word);
        int32_t second = signed16(word >> 16);
        uint32_t low = (uint32_t)arithmetic_shift_right((int32_t)((uint32_t)first * scales_low[i]), 12) & 0xFFFFu;
        uint32_t high = (uint32_t)arithmetic_shift_right((int32_t)((uint32_t)second * scales_high[i]), 12) << 16;
        ae_wr32(r, address, low | high);
        if (ae_ram_failed(r)) return matrix;
    }
    {
        uint32_t word = ae_rd32(r, matrix + 16u);
        int32_t last = signed16(word);
        ae_wr32(r, matrix + 16u,
                (uint32_t)arithmetic_shift_right((int32_t)((uint32_t)last * scale_z), 12));
    }
    return matrix;
}

uint32_t ae_sdk_MatrixIdentity(AeRam *r, uint32_t matrix)
{
    ae_wr32(r, matrix + 20u, 0);
    ae_wr32(r, matrix + 24u, 0);
    ae_wr32(r, matrix + 28u, 0);
    ae_wr32(r, matrix + 4u, 0);
    ae_wr32(r, matrix + 12u, 0);
    ae_wr32(r, matrix, 4096u);
    ae_wr32(r, matrix + 8u, 4096u);
    ae_wr32(r, matrix + 16u, 4096u);
    return 4096u;
}

uint32_t ae_sdk_MatrixFromDirXZ(AeRam *r, uint32_t direction, uint32_t matrix)
{
    uint32_t value = ae_rd16(r, direction + 4u);
    value = 0u - value;
    ae_wr16(r, matrix + 16u, value);
    ae_wr16(r, matrix, value);

    value = ae_rd16(r, direction);
    value = 0u - value;
    ae_wr16(r, matrix + 4u, value);
    uint32_t return_value = 4096u;
    ae_wr16(r, matrix + 8u, return_value);
    ae_wr16(r, matrix + 14u, 0);
    ae_wr16(r, matrix + 10u, 0);
    ae_wr16(r, matrix + 6u, 0);
    ae_wr16(r, matrix + 2u, 0);
    value = ae_rd16(r, direction);
    ae_wr16(r, matrix + 12u, value);
    return return_value;
}

uint32_t ae_sdk_MatrixLookDir(AeRam *r, uint32_t direction, uint32_t matrix)
{
    uint32_t normalized = AE_LOOKDIR_STACK + 32u;
    (void)ae_sdk_VectorNormalSS(r, direction, normalized);
    if (ae_ram_failed(r)) return 0;

    int32_t dir_x = signed16((uint32_t)negate32(ae_rds16(r, normalized)));
    int32_t dir_y = signed16((uint32_t)negate32(ae_rds16(r, normalized + 2u)));
    int32_t dir_z = signed16((uint32_t)negate32(ae_rds16(r, normalized + 4u)));
    ae_wr16(r, normalized, (uint32_t)dir_x);
    ae_wr16(r, normalized + 2u, (uint32_t)dir_y);
    ae_wr16(r, normalized + 4u, (uint32_t)dir_z);

    int32_t basis_x = 0;
    int32_t basis_y = 0;
    int32_t basis_z = 0;
    if (dir_x != 0 || dir_z != 0) {
        basis_y = 4096;
    } else {
        basis_z = 4096;
    }
    ae_wr16(r, AE_LOOKDIR_STACK + 24u, (uint32_t)basis_x);
    ae_wr16(r, AE_LOOKDIR_STACK + 26u, (uint32_t)basis_y);
    ae_wr16(r, AE_LOOKDIR_STACK + 28u, (uint32_t)basis_z);

    int32_t cross_x = cross_component(basis_y, dir_z, basis_z, dir_y);
    int32_t cross_y = cross_component(basis_z, dir_x, basis_x, dir_z);
    int32_t cross_z = cross_component(basis_x, dir_y, basis_y, dir_x);
    ae_wr16(r, AE_LOOKDIR_STACK + 16u, (uint32_t)cross_x);
    ae_wr16(r, AE_LOOKDIR_STACK + 18u, (uint32_t)cross_y);
    ae_wr16(r, AE_LOOKDIR_STACK + 20u, (uint32_t)cross_z);
    uint32_t normalize_result = ae_sdk_VectorNormalSS(r, AE_LOOKDIR_STACK + 16u, AE_LOOKDIR_STACK + 16u);
    if (ae_ram_failed(r)) return 0;

    int32_t normal_x = ae_rds16(r, AE_LOOKDIR_STACK + 16u);
    int32_t normal_y = ae_rds16(r, AE_LOOKDIR_STACK + 18u);
    int32_t normal_z = ae_rds16(r, AE_LOOKDIR_STACK + 20u);
    cross_x = cross_component(dir_y, normal_z, dir_z, normal_y);
    cross_y = cross_component(dir_z, normal_x, dir_x, normal_z);
    cross_z = cross_component(dir_x, normal_y, dir_y, normal_x);
    ae_wr16(r, AE_LOOKDIR_STACK + 24u, (uint32_t)cross_x);
    ae_wr16(r, AE_LOOKDIR_STACK + 26u, (uint32_t)cross_y);
    ae_wr16(r, AE_LOOKDIR_STACK + 28u, (uint32_t)cross_z);

    ae_wr16(r, matrix, (uint32_t)normal_x);
    ae_wr16(r, matrix + 6u, (uint32_t)normal_y);
    ae_wr16(r, matrix + 12u, (uint32_t)normal_z);
    ae_wr16(r, matrix + 2u, (uint32_t)cross_x);
    ae_wr16(r, matrix + 8u, (uint32_t)cross_y);
    ae_wr16(r, matrix + 14u, (uint32_t)cross_z);
    ae_wr16(r, matrix + 4u, (uint32_t)dir_x);
    ae_wr16(r, matrix + 10u, (uint32_t)dir_y);
    ae_wr16(r, matrix + 16u, (uint32_t)dir_z);
    (void)normalize_result;
    return (uint32_t)ae_rd16(r, normalized + 4u);
}
