#ifndef AE_NATIVE_INDICATOR_RUNTIME_H
#define AE_NATIVE_INDICATOR_RUNTIME_H
#include <stdint.h>
#include "../camera/native_camera_math.h"
#include "native_indicators.h"
#include "native_indicator_trigger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gimmick_ProxTrigger_Init state composition. The caller has already resolved
 * the trigger's collider (24-byte trigger record, 56-byte collider record).
 * Returns 1 on success, 0 on failure with every output untouched.
 * time_attack != 0: only trigger and collider are used and modified; no slot is
 * registered, so slots, count and math may be NULL.
 * Otherwise slots, count (<= 4, a full bank fails) and loaded math are required. */
int ae_indicator_runtime_init(AeIndicatorTrigger *trigger, uint8_t collider[56],
                              AeNativeIndicator slots[4], uint32_t *count, uint32_t area,
                              int time_attack, const AeCameraMathTables *math);

/* Ordered requests from one update: activation requests (Indicator_Activate
 * results) come first, then the trigger's effect0x112 notification. */
typedef struct {
    AeIndicatorTriggerEvents trigger;
    AeIndicatorActivation activation;
} AeIndicatorRuntimeEvents;

/* Gimmick_ProxTrigger_Update composition: distance from player to collider
 * position, touchedBy from the collider, trigger update, then slot activation.
 * Returns 1 on success, 0 on failure with trigger, slots and events untouched.
 * Stage exit codes 0x17/7/0x15 succeed with cleared events and need neither
 * player nor slots. player is required otherwise; slots and count only when an
 * activation is requested. */
int ae_indicator_runtime_step(AeIndicatorTrigger *trigger, const uint8_t collider[56],
                              AeNativeIndicator slots[4], uint32_t count,
                              const int32_t player[3], uint32_t area, uint32_t exit_code,
                              int message_busy, AeIndicatorRuntimeEvents *events);

#ifdef __cplusplus
}
#endif
#endif
