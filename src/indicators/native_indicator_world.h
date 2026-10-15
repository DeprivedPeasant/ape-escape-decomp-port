/* Optional native world indicator presentation: prepared AEIND1/AEIND2 pack, original
 * slot registration/animation state (native_indicators.h) and actor drawing.
 * See native_indicator_world.md. Needs native_indicators, native_camera_math and
 * native_actor. No game data is embedded. */
#ifndef AE_NATIVE_INDICATOR_WORLD_H
#define AE_NATIVE_INDICATOR_WORLD_H

#include <stddef.h>
#include <stdint.h>
#include "native_indicators.h"
#include "native_indicator_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AeIndicatorWorld AeIndicatorWorld;

/* Loads an AEIND1 or AEIND2 pack for `area`. The caller passes a zero initialized pointer
 * (*out must be NULL). Actor files are read from the pack's directory. exe is
 * the original PSX-EXE image (used for the sine tables only; it is not kept).
 * Returns 1 and sets *out, or returns 0 with *out still NULL and everything
 * released (see ae_indicator_world_error). No OpenGL context is needed. */
int ae_indicator_world_load(AeIndicatorWorld **out, const char *pack_path,
                            const uint8_t *exe, size_t exe_bytes, uint32_t area);

/* Uploads actor textures. Requires a current OpenGL context. */
int ae_indicator_world_upload(AeIndicatorWorld *world);

/* Frees the world; call before the OpenGL context is destroyed. NULL is a no-op. */
void ae_indicator_world_free(AeIndicatorWorld *world);

/* One original 30 Hz game tick: ae_indicator_step once per slot. Never starts an
 * activation. NULL is a no-op. */
void ae_indicator_world_tick(AeIndicatorWorld *world);

/* Draws every indicator with the current OpenGL matrix as the PSX-to-GL world
 * (x, -y, z) / 256. Does not advance state. Returns 0 (message in
 * ae_indicator_world_error) if a raw frame has no clip frame or posing fails. */
int ae_indicator_world_draw(AeIndicatorWorld *world, int textured);
/* Draw the model-frame outputs cached by the preceding world tick. The original
 * indicator draw precedes trigger activation, so later slot writes must not
 * change the pose drawn during that tick. */
int ae_indicator_world_draw_stepped(AeIndicatorWorld *world, int textured);
uint32_t ae_indicator_world_model_frame(const AeIndicatorWorld *world, uint32_t index);

/* Accessors for tests and logs. Index >= count returns 0 / -1. */
uint32_t ae_indicator_world_count(const AeIndicatorWorld *world);
uint32_t ae_indicator_world_ticks(const AeIndicatorWorld *world);
uint32_t ae_indicator_world_frame(const AeIndicatorWorld *world, uint32_t index);  /* raw slot frame */
int ae_indicator_world_kind(const AeIndicatorWorld *world, uint32_t index);        /* -1 if invalid */
uint32_t ae_indicator_world_state(const AeIndicatorWorld *world, uint32_t index);  /* raw slot state */
int ae_indicator_world_hint_sector(const AeIndicatorWorld *world, uint32_t index);
/* Execute local activation state and return unexecuted engine requests.
 * Invalid world/index/events return 0 without writes. No implicit UI or pickups. */
int ae_indicator_world_activate(AeIndicatorWorld *world, uint32_t index,
                                int message_busy, AeIndicatorActivation *events);
/* Clip selection used by draw: *clip 0 = loop, 1 = activate. Returns 0 outside bounds. */
int ae_indicator_world_clip_frame(const AeIndicatorWorld *world, uint32_t index,
                                  uint32_t *clip, uint32_t *frame);
/* Yaw in degrees and position in viewer units for slot index. Returns 0 if invalid. */
int ae_indicator_world_placement(const AeIndicatorWorld *world, uint32_t index,
                                 float position[3], float *yaw);

/* 1 only for a world loaded from an AEIND2 pack; 0 for AEIND1 or NULL. */
int ae_indicator_world_has_runtime(const AeIndicatorWorld *world);
/* Copies the stored trigger, collider and the shared slot for `index`. Requires a
 * runtime world, index < count and all three outputs; no writes on failure. */
int ae_indicator_world_runtime_record(const AeIndicatorWorld *world, uint32_t index,
                                      AeIndicatorTrigger *trigger, uint8_t collider[56],
                                      AeNativeIndicator *slot);
/* One Gimmick_ProxTrigger_Update per entry in registration order, transactional
 * (events/missing mask/world unchanged on failure). Stage exit codes 0x17/7/0x15
 * (low 8 bits) need no player and zero events and the mask. Otherwise player is
 * required; touch_known_mask may only name existing entries and touched_by is
 * required only if it is nonzero. Unknown touch is fed as -1 (the stored disk
 * touched field is never live contact). Bit i of *missing_touch_mask is set when
 * entry i is unknown and its state-1 (distance < radius) or state-3
 * (distance <= radius) touch branch is reachable. message_busy is one host input
 * for the whole batch. No animation tick (use ae_indicator_world_tick). Unused
 * events[] are zeroed. Returned requests are not executed. */
int ae_indicator_world_proximity_step(AeIndicatorWorld *world, const int32_t player[3],
                                      uint32_t exit_code, int message_busy,
                                      const int16_t touched_by[4], uint32_t touch_known_mask,
                                      AeIndicatorRuntimeEvents events[4], uint32_t *missing_touch_mask);

const char *ae_indicator_world_error(void);

#ifdef __cplusplus
}
#endif

#endif
