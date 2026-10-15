/* World indicator slot state: register (0x800716EC) and the animation slice of
 * IndicatorsUpdateDraw (0x80071A90..0x80071B68). See native_indicators.md.
 * No dependencies beyond native_camera_math.h and the C99 standard headers. */
#ifndef AE_NATIVE_INDICATORS_H
#define AE_NATIVE_INDICATORS_H

#include <stdint.h>

#include "../camera/native_camera_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_INDICATOR_MAX 4u

/* 24 byte original slot, little endian fields:
 *   +00 s16 kind   +02 s16 angle   +04 s16[3] position   +0A s16 timer
 *   +0C s16 dir sin  +0E s16 zero  +10 s16 dir cos
 *   +12 2 bytes padding (never written)  +14 u16 frame  +16 u16 state */
typedef struct AeNativeIndicator {
    uint8_t raw[24];
} AeNativeIndicator;

/* Indicator_Register. Returns the new slot's index (the previous count) and
 * increments *count. Returns -1 with nothing written if any pointer is null,
 * math is not loaded, or *count >= 4 (the original calls exit() when full).
 * kind and angle are stored as the low 16 bits; rsin/rcos get the full 32 bit
 * angle. position8 supplies the 8 bytes copied to +04..+0B; the timer at +0A
 * is then zeroed. */
int ae_indicator_register(AeNativeIndicator slots[4], uint32_t *count, int16_t kind,
                          const uint8_t position8[8], int32_t angle,
                          const AeCameraMathTables *math);

/* Animation slice only. A null slot is a no-op. Null frame pointers skip that
 * write. model_a_frame is written for kinds 0/1, model_b_frame for kind 2. */
void ae_indicator_step(AeNativeIndicator *slot, uint16_t *model_a_frame,
                       uint16_t *model_b_frame);

/* Local state and engine requests from Indicator_Activate (0x800717B4).
 * Hint requests also require the original stage/player/CD/HUD/camera protocol;
 * returning them does not execute that protocol. Pickups are three ordered
 * SpawnPickups(1, position, 3, 0) calls, followed by PlaySfx(0xF).
 * Hint requests precede PlaySfx(0xF). Zero flags mean activation was blocked.
 * Null arguments return 0 without writes; otherwise returns 1 and clears events. */
typedef struct AeIndicatorActivation {
    uint32_t hint, pickups, sound;
    int32_t sector;
    int32_t pickup_position[3];
} AeIndicatorActivation;

int ae_indicator_activate(AeNativeIndicator *slot, int32_t sector,
                          int message_busy, AeIndicatorActivation *events);

#ifdef __cplusplus
}
#endif

#endif
