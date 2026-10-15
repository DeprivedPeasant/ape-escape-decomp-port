#ifndef AE_NATIVE_INDICATOR_TRIGGER_H
#define AE_NATIVE_INDICATOR_TRIGGER_H
#include <stdint.h>
#include "native_indicators.h"
typedef struct { uint8_t raw[24]; } AeIndicatorTrigger;
typedef struct {
    uint32_t activate, effect;
    int32_t index, sector, effect_position[3];
} AeIndicatorTriggerEvents;
/* Original Gimmick_ProxTrigger_Update state slice; distance is the VecLength3
 * result supplied by the world adapter. Event calls are returned explicitly. */
int ae_indicator_trigger_update(AeIndicatorTrigger *trigger, const int16_t center[3],
    int16_t touched_by, int32_t distance, uint32_t area, uint32_t exit_code,
    AeIndicatorTriggerEvents *events);
/* VecLength3 over wrapped signed16 GTE IR components and original IntSqrt. */
int32_t ae_indicator_trigger_distance(const int32_t player[3], const int16_t center[3]);
#endif
