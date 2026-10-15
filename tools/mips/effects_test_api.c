/* Checker-only model of the already verified PlaySfx dependency. */
#include "../../src/core/ae_ram.h"
#include "../../src/sound/ae_sound.h"

static int32_t g_effects_test_sfx_ids[4];
static uint32_t g_effects_test_sfx_count;
static int32_t g_effects_test_spawn_types[64];
static uint32_t g_effects_test_spawn_positions[64];
static uint32_t g_effects_test_spawn_count;

void effects_test_reset(void) {
    g_effects_test_sfx_count = 0;
    g_effects_test_spawn_count = 0;
}

void effects_test_record_spawn_particle(int32_t type, uint32_t position) {
    if (g_effects_test_spawn_count < 64u) {
        uint32_t index = g_effects_test_spawn_count++;
        g_effects_test_spawn_types[index] = type;
        g_effects_test_spawn_positions[index] = position;
    }
}

uint32_t effects_test_spawn_count(void) {
    return g_effects_test_spawn_count;
}

int32_t effects_test_spawn_type(uint32_t index) {
    return index < g_effects_test_spawn_count ? g_effects_test_spawn_types[index] : -1;
}

uint32_t effects_test_spawn_position(uint32_t index) {
    return index < g_effects_test_spawn_count ? g_effects_test_spawn_positions[index] : 0;
}

uint32_t effects_test_sfx_count(void) {
    return g_effects_test_sfx_count;
}

int32_t effects_test_sfx_id(uint32_t index) {
    return index < g_effects_test_sfx_count ? g_effects_test_sfx_ids[index] : -1;
}

int32_t effects_test_play_sfx(AeRam *r, const AeSoundHw *hw, int32_t id) {
    (void)r;
    (void)hw;
    if (g_effects_test_sfx_count < 4u) {
        g_effects_test_sfx_ids[g_effects_test_sfx_count++] = id;
    }
    return -1;
}
