#include "native_sdk_sqrt.h"

#define TABLE_FILE_OFFSET \
    ((size_t)(AE_SDK_SQRT_TABLE_VADDR - 0x80010000u + 0x800u))
#define TABLE_BYTES ((size_t)AE_SDK_SQRT_TABLE_COUNT * 2u)

#define TABLE_MIN 4096
#define TABLE_MAX 8192
#define TABLE_FIRST 4096
#define TABLE_LAST 8175

/* Equivalent of the MIPS LZCR read for a 32-bit word. */
static unsigned count_leading_zeros32(uint32_t v)
{
    unsigned n = 0;
    uint32_t bit = 0x80000000u;
    while (n < 32u && (v & bit) == 0u) {
        n++;
        bit >>= 1;
    }
    return n;
}

int ae_sdk_sqrt_load(AeSdkSqrtTables *tables, const uint8_t *exe, size_t bytes)
{
    int16_t staged[AE_SDK_SQRT_TABLE_COUNT];
    const uint8_t *p;
    size_t i;

    if (tables != NULL) {
        tables->loaded = 0;
    }
    if (tables == NULL || exe == NULL) {
        return 0;
    }
    if (bytes < TABLE_FILE_OFFSET || bytes - TABLE_FILE_OFFSET < TABLE_BYTES) {
        return 0;
    }

    p = exe + TABLE_FILE_OFFSET;
    for (i = 0; i < AE_SDK_SQRT_TABLE_COUNT; i++) {
        uint16_t raw = (uint16_t)((uint16_t)p[i * 2u] |
                                  (uint16_t)((uint16_t)p[i * 2u + 1u] << 8));
        /* Raw signed value; valid entries are positive, so no wrap is needed. */
        int value = (raw & 0x8000u) ? (int)raw - 0x10000 : (int)raw;
        if (value < TABLE_MIN || value > TABLE_MAX) {
            return 0;
        }
        staged[i] = (int16_t)value;
    }
    if (staged[0] != TABLE_FIRST ||
        staged[AE_SDK_SQRT_TABLE_COUNT - 1u] != TABLE_LAST) {
        return 0;
    }

    for (i = 0; i < AE_SDK_SQRT_TABLE_COUNT; i++) {
        tables->values[i] = staged[i];
    }
    tables->loaded = 1;
    return 1;
}

int ae_sdk_sqrt0(const AeSdkSqrtTables *tables, uint32_t value, uint32_t *out)
{
    unsigned lz, even, exponent;
    uint32_t normalized, index, factor;

    if (tables == NULL || out == NULL || !tables->loaded ||
        value > 0x7FFFFFFFu) {
        return 0;
    }
    if (value == 0u) {
        *out = 0u;
        return 1;
    }

    lz = count_leading_zeros32(value);   /* 1..31 here */
    even = lz & ~1u;                     /* 0..30 */
    exponent = (31u - even) >> 1;        /* 0..15 */
    normalized = (even >= 24u) ? (value << (even - 24u))
                               : (value >> (24u - even));
    if (normalized < 64u) {
        return 0;
    }
    index = normalized - 64u;
    if (index >= AE_SDK_SQRT_TABLE_COUNT) {
        return 0;
    }
    if (tables->values[index] <= 0) {
        return 0;
    }
    factor = (uint32_t)tables->values[index];
    *out = (factor << exponent) >> 12;
    return 1;
}
