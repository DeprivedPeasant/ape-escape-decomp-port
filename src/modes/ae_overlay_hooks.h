#ifndef AE_OVERLAY_HOOKS_H
#define AE_OVERLAY_HOOKS_H

#include "../core/ae_ram.h"
#include "../game/ae_calls_api.h"

/* Overlay code runs against AeRam.  Calls to already ported EXE functions go
 * through AeGame's address dispatcher (and therefore their native C ports).
 * Calls at the SDK / native-system boundary keep their original public name,
 * address and o32 argument words. */
typedef int32_t (*AeOverlayCallFn)(void *user, AeRam *ram, const char *name,
                                   uint32_t address, uint32_t count,
                                   const uint32_t *arguments);
typedef int32_t (*AeOverlayRenderFn)(void *user, AeRam *ram, const char *name,
                                     uint32_t count, const uint32_t *values);

typedef struct AeOverlayHooks {
    AeGame *game;
    void *user;
    AeOverlayCallFn call;
    AeOverlayRenderFn render;
} AeOverlayHooks;

static inline int32_t ae_overlay_call(AeRam *ram, const AeOverlayHooks *hooks,
                                      const char *name, uint32_t address,
                                      uint32_t count, const uint32_t *arguments)
{
    const AeGameCall *port = ae_game_find_call(address);
    /* The ledger entry for CameraLookAt proves only its yaw matrix adapter.
     * The complete five word LIBGTE call remains a native renderer hook. */
    if (address == 0x80046670u) port = NULL;
    if (port != NULL && port->overlay_member == 0u) {
        if (hooks == NULL || hooks->game == NULL) {
            ae_ram_fail(ram, "%s: AeGame is required for the verified EXE port", name);
            return 0;
        }
        uint32_t a0 = count > 0u ? arguments[0] : 0u;
        uint32_t a1 = count > 1u ? arguments[1] : 0u;
        uint32_t a2 = count > 2u ? arguments[2] : 0u;
        uint32_t a3 = count > 3u ? arguments[3] : 0u;
        switch (count) {
        case 0u: return ae_game_call(hooks->game, address, 0u, 0u, 0u, 0u);
        case 1u: return ae_game_call(hooks->game, address, a0, 0u, 0u, 0u);
        case 2u: return ae_game_call(hooks->game, address, a0, a1, 0u, 0u);
        case 3u: return ae_game_call(hooks->game, address, a0, a1, a2, 0u);
        case 4u: return ae_game_call(hooks->game, address, a0, a1, a2, a3);
        case 5u: return ae_game_call5(hooks->game, address, a0, a1, a2, a3, arguments[4]);
        case 6u: return ae_game_call6(hooks->game, address, a0, a1, a2, a3, arguments[4], arguments[5]);
        case 7u: return ae_game_call7(hooks->game, address, a0, a1, a2, a3,
                                     arguments[4], arguments[5], arguments[6]);
        default:
            ae_ram_fail(ram, "%s: unsupported EXE port argument count %u", name, count);
            return 0;
        }
    }
    if (hooks == NULL || hooks->call == NULL) {
        ae_ram_fail(ram, "%s hook not provided", name);
        return 0;
    }
    return hooks->call(hooks->user, ram, name, address, count, arguments);
}

static inline int32_t ae_overlay_native_call(AeRam *ram,
                                             const AeOverlayHooks *hooks,
                                             const char *name,
                                             uint32_t address,
                                             uint32_t count,
                                             const uint32_t *arguments)
{
    if (hooks == NULL || hooks->call == NULL) {
        ae_ram_fail(ram, "%s hook not provided", name);
        return 0;
    }
    return hooks->call(hooks->user, ram, name, address, count, arguments);
}

static inline int32_t ae_overlay_render(AeRam *ram, const AeOverlayHooks *hooks,
                                        const char *name, uint32_t count,
                                        const uint32_t *values)
{
    if (hooks == NULL || hooks->render == NULL) {
        ae_ram_fail(ram, "%s renderer hook not provided", name);
        return 0;
    }
    return hooks->render(hooks->user, ram, name, count, values);
}

#endif
