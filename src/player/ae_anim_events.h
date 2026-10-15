#ifndef AE_ANIM_EVENTS_H
#define AE_ANIM_EVENTS_H

#include "../core/ae_ram.h"
#include "../bodies/ae_bodies.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host services called by PlayerRunAnimEvents. The function-address callback covers the two
   original callback tables (g_PlayerEventFns and g_PlayerActionFns); it receives the resolved
   EXE address so the host can route the event to its own implementation. */
typedef struct AeAnimEventHooks {
    void *user;
    void (*play_sfx)(void *user, AeRam *r, int32_t id);              /* PlaySfx @800877F8 */
    void (*play_player_voice)(void *user, AeRam *r, int32_t id);    /* PlayPlayerVoice @8006896C */
    void (*event_callback)(void *user, AeRam *r, uint32_t function);/* callback tables @800B027C / @800B03AC */
} AeAnimEventHooks;

/* PlayerRunAnimEvents @80064454: state_table and events are the current PlayerStateSet pointers. */
void ae_PlayerRunAnimEvents(AeRam *r, const AeBodyHooks *body_hooks, const AeAnimEventHooks *event_hooks,
                            uint32_t state_table, uint32_t events);

/* PlayerAnimEventAddHitBodies @8007C468. The first original argument is unused. */
void ae_PlayerAnimEventAddHitBodies(AeRam *r, const AeBodyHooks *body_hooks, uint32_t unused,
                                    int32_t radius, uint32_t param_a, uint32_t param_b);

#ifdef __cplusplus
}
#endif
#endif
