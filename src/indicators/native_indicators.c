#include "native_indicators.h"

#include <stddef.h>
#include <string.h>

#define OFF_KIND 0x00u
#define OFF_ANGLE 0x02u
#define OFF_POS 0x04u
#define OFF_TIMER 0x0Au
#define OFF_DIR_SIN 0x0Cu
#define OFF_DIR_ZERO 0x0Eu
#define OFF_DIR_COS 0x10u
#define OFF_FRAME 0x14u
#define OFF_STATE 0x16u

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static int16_t to_s16(uint16_t v)
{
    return (int16_t)(v < 0x8000u ? (int32_t)v : (int32_t)v - 65536);
}

int ae_indicator_register(AeNativeIndicator slots[4], uint32_t *count, int16_t kind,
                          const uint8_t position8[8], int32_t angle,
                          const AeCameraMathTables *math)
{
    uint32_t index;
    uint8_t *s;
    uint16_t sin_v, cos_v;
    int i;

    if (!slots || !count || !position8 || !math || !math->loaded || *count >= AE_INDICATOR_MAX)
        return -1;

    index = *count;
    s = slots[index].raw;

    /* rsin/rcos results are stored as their low 16 bits (sh). */
    sin_v = (uint16_t)((uint32_t)ae_camera_rsin(math, angle) & 0xFFFFu);
    cos_v = (uint16_t)((uint32_t)ae_camera_rcos(math, angle) & 0xFFFFu);

    put_u16(s + OFF_KIND, (uint16_t)kind);
    put_u16(s + OFF_ANGLE, (uint16_t)((uint32_t)angle & 0xFFFFu));
    for (i = 0; i < 8; i++)
        s[OFF_POS + (unsigned)i] = position8[i];
    put_u16(s + OFF_FRAME, 0);
    put_u16(s + OFF_STATE, 0);
    put_u16(s + OFF_DIR_SIN, sin_v);
    put_u16(s + OFF_DIR_ZERO, 0);
    put_u16(s + OFF_DIR_COS, cos_v);
    put_u16(s + OFF_TIMER, 0);

    *count = index + 1u;
    return (int)index;
}

void ae_indicator_step(AeNativeIndicator *slot, uint16_t *model_a_frame,
                       uint16_t *model_b_frame)
{
    uint8_t *s;
    int16_t kind;
    int32_t frame;
    uint16_t state;

    if (!slot)
        return;
    s = slot->raw;
    kind = to_s16(get_u16(s + OFF_KIND));
    frame = (int32_t)get_u16(s + OFF_FRAME) + 1;

    if (kind >= 0 && kind <= 2) {
        state = get_u16(s + OFF_STATE);
        if (kind < 2) {
            if (state == 0) {
                if (frame > 28)
                    frame = 0;
            } else if (state == 1) {
                if (frame > 88) {
                    frame = 0;
                    put_u16(s + OFF_STATE, 0);
                }
            }
            if (model_a_frame)
                *model_a_frame = (uint16_t)((uint32_t)frame & 0xFFFFu);
        } else {
            int16_t timer;

            if (state == 0) {
                if (frame > 57)
                    frame = 0;
            } else if (state == 1) {
                if (frame > 87) {
                    frame = 0;
                    put_u16(s + OFF_STATE, 0);
                }
            }
            if (model_b_frame)
                *model_b_frame = (uint16_t)((uint32_t)frame & 0xFFFFu);
            timer = to_s16(get_u16(s + OFF_TIMER));
            if (timer > 0)
                put_u16(s + OFF_TIMER, (uint16_t)(timer - 1));
        }
    }
    put_u16(s + OFF_FRAME, (uint16_t)((uint32_t)frame & 0xFFFFu));
}

int ae_indicator_activate(AeNativeIndicator *slot, int32_t sector,
                          int message_busy, AeIndicatorActivation *events)
{
    uint8_t *s;
    int16_t kind;
    uint16_t state;
    if (!slot || !events) return 0;
    memset(events, 0, sizeof(*events));
    s = slot->raw;
    kind = to_s16(get_u16(s + OFF_KIND));
    state = get_u16(s + OFF_STATE);
    if (kind == 0 || kind == 1) {
        if (state == 1 || message_busy) return 1;
        put_u16(s + OFF_STATE, 1);
        put_u16(s + OFF_FRAME, 29);
        events->hint = 1;
        events->sector = sector;
    } else if (kind == 2) {
        if (state != 0 || to_s16(get_u16(s + OFF_TIMER)) > 0) return 1;
        put_u16(s + OFF_STATE, 1);
        put_u16(s + OFF_FRAME, 58);
        put_u16(s + OFF_TIMER, 90);
        events->pickups = 3;
        events->pickup_position[0] = to_s16(get_u16(s + OFF_POS));
        events->pickup_position[1] = to_s16((uint16_t)(get_u16(s + OFF_POS + 2) - 0x140u));
        events->pickup_position[2] = to_s16(get_u16(s + OFF_POS + 4));
    } else return 1;
    events->sound = 0xFu;
    return 1;
}
