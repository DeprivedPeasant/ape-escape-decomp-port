#include "native_indicator_runtime.h"
#include <string.h>

/* Trigger record offsets. */
#define T_STATE 2
#define T_COLLIDER 4
#define T_CLEAR6 6
#define T_PARAM 8
#define T_KIND 10
#define T_ANGLE 12
#define T_CLEAR16 16
#define T_INDEX 20
/* Collider record offsets. */
#define C_SIZE_A 6
#define C_SIZE_B 8
#define C_SIZE_C 10
#define C_FLAGS 24
#define C_TOUCHED 30

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static int16_t sx16(const uint8_t *p) {
    uint16_t v = get16(p);
    return (int16_t)(v >= 32768u ? (int32_t)v - 65536 : (int32_t)v);
}

int ae_indicator_runtime_init(AeIndicatorTrigger *trigger, uint8_t collider[56],
                              AeNativeIndicator slots[4], uint32_t *count, uint32_t area,
                              int time_attack, const AeCameraMathTables *math) {
    AeIndicatorTrigger t;
    uint8_t c[56];
    if (!trigger || !collider) return 0;
    memcpy(&t, trigger, sizeof(t));
    memcpy(c, collider, sizeof(c));
    put16(c + C_FLAGS, (uint16_t)((get16(c + C_FLAGS) & 0xFFF0u) | 1u));
    if (time_attack) {
        put16(t.raw + T_STATE, 2);
        put16(c + C_FLAGS, (uint16_t)(get16(c + C_FLAGS) | 0x8000u));
    } else {
        AeNativeIndicator local[AE_INDICATOR_MAX];
        uint32_t n;
        uint8_t pos8[8] = {0}; /* native policy: bytes 6..7 zero; register overwrites them */
        int idx, i;
        if (!slots || !count || !math || *count > AE_INDICATOR_MAX) return 0;
        n = *count;
        memcpy(local, slots, sizeof(local));
        put16(c + C_SIZE_C, 0x50);
        put16(c + C_SIZE_A, 0x50);
        put16(c + C_SIZE_B, 0x100);
        for (i = 0; i < 6; ++i) pos8[i] = c[i];
        idx = ae_indicator_register(local, &n, sx16(t.raw + T_KIND), pos8,
                                    (int32_t)sx16(t.raw + T_ANGLE), math);
        if (idx < 0) return 0;
        put16(t.raw + T_INDEX, (uint16_t)idx);
        put16(t.raw + T_CLEAR6, 0);
        if ((area & 0xFFFFu) - 0x5Cu < 7u) put16(t.raw + T_STATE, 0);
        else put16(t.raw + T_STATE, get16(t.raw + T_PARAM) ? 1 : 0);
        put16(t.raw + T_CLEAR16, 0);
        memcpy(slots, local, sizeof(local));
        *count = n;
    }
    memcpy(trigger, &t, sizeof(t));
    memcpy(collider, c, sizeof(c));
    return 1;
}

int ae_indicator_runtime_step(AeIndicatorTrigger *trigger, const uint8_t collider[56],
                              AeNativeIndicator slots[4], uint32_t count,
                              const int32_t player[3], uint32_t area, uint32_t exit_code,
                              int message_busy, AeIndicatorRuntimeEvents *events) {
    AeIndicatorTrigger t;
    AeIndicatorRuntimeEvents ev;
    AeNativeIndicator slot;
    int16_t center[3];
    int32_t distance, index = -1;
    uint32_t code;
    if (!trigger || !collider || !events) return 0;
    code = exit_code & 255u;
    if (code == 0x17u || code == 7u || code == 0x15u) {
        memset(events, 0, sizeof(*events));
        return 1;
    }
    if (!player) return 0;
    memset(&ev, 0, sizeof(ev));
    memcpy(&t, trigger, sizeof(t));
    center[0] = sx16(collider);
    center[1] = sx16(collider + 2);
    center[2] = sx16(collider + 4);
    distance = ae_indicator_trigger_distance(player, center);
    if (!ae_indicator_trigger_update(&t, center, sx16(collider + C_TOUCHED), distance, area,
                                     exit_code, &ev.trigger))
        return 0;
    if (ev.trigger.activate) {
        index = ev.trigger.index;
        if (!slots || count > AE_INDICATOR_MAX || index < 0 || (uint32_t)index >= count)
            return 0;
        memcpy(&slot, &slots[index], sizeof(slot));
        if (!ae_indicator_activate(&slot, ev.trigger.sector, message_busy, &ev.activation))
            return 0;
        memcpy(&slots[index], &slot, sizeof(slot));
    }
    memcpy(trigger, &t, sizeof(t));
    *events = ev;
    return 1;
}
