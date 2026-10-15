/* Small integration surface for checking just the two boot overlays. */
#include "../../src/game/ae_calls.h"
#include "../../src/modes/ae_ovl181.h"
#include "../../src/modes/ae_ovl182.h"

#include <stdlib.h>
#include <string.h>

void ae_game_wire_body(AeGame *game);
void ae_game_wire_modules(AeGame *game);

typedef int32_t (*Trace)(void *, const char *, uint32_t, uint32_t,
                         const uint32_t *);
typedef struct Integration {
    AeGame game;
    Trace trace;
    void *user;
} Integration;

static int32_t trace_call(void *user, const char *name, uint32_t address,
                          uint32_t argc, uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3, uint32_t a4,
                          uint32_t a5) {
    Integration *s = user;
    const uint32_t args[6] = {a0, a1, a2, a3, a4, a5};
    return s->trace(s->user, name, address, argc, args);
}

static int32_t missing_call(void *user, AeRam *ram, const char *name,
                            uint32_t address, uint32_t a0, uint32_t a1,
                            uint32_t a2, uint32_t a3) {
    Integration *s = user;
    (void)ram;
    return trace_call(s, name, address, 4u, a0, a1, a2, a3, 0u, 0u);
}

static int32_t missing_words(void *user, AeRam *ram, const char *name,
                             uint32_t address, uint32_t count,
                             const uint32_t *args) {
    (void)ram;
    Integration *s = user;
    return s->trace(s->user, name, address, count, args);
}

static int32_t render_call(void *user, AeRam *ram, const char *name,
                           uint32_t a0, uint32_t a1, uint32_t a2,
                           uint32_t a3) {
    (void)ram;
    return trace_call(user, name, 0u, 4u, a0, a1, a2, a3, 0u, 0u);
}

static void debug_call(void *user, AeRam *ram, const char *name,
                       uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)ram;
    (void)trace_call(user, name, 0x800A3720u, 4u, a0, a1, a2, a3, 0u, 0u);
}

/* The generated host callback adapters use this test trace. */
#include "game_integration_callbacks.inc"

Integration *integration_create(Trace trace, void *user) {
    Integration *s = calloc(1, sizeof(*s));
    if (s == NULL) return NULL;
    AeRam *ram = ae_ram_create();
    if (ram == NULL) { free(s); return NULL; }
    s->trace = trace;
    s->user = user;
    AeGameHost host;
    memset(&host, 0, sizeof(host));
    host.user = s;
    host.missing = missing_call;
    host.missing_words = missing_words;
    host.render = render_call;
    host.debug = debug_call;
    integration_callbacks(&host);
    host.sound_hw.user = s;
    host.sound_hw.wait_idle = test_platform_SpuWaitIdle;
    ae_game_init_legacy(&s->game, ram, &host);
    ae_game_wire_modules(&s->game);
    ae_game_wire_body(&s->game);
    return s;
}

AeRam *integration_ram(Integration *s) { return s->game.ram; }

int integration_overlay_has_port(uint32_t address) {
    const AeGameCall *call = ae_game_find_call(address);
    return call != NULL && call->overlay_member == 0u;
}

static int32_t overlay_call(void *user, AeRam *ram, const char *name,
                            uint32_t address, uint32_t count,
                            const uint32_t *arguments) {
    Integration *s = user;
    (void)ram;
    return s->trace(s->user, name, address, count, arguments);
}

static int32_t overlay_render(void *user, AeRam *ram, const char *name,
                              uint32_t count, const uint32_t *values) {
    (void)ram;
    Integration *s = user;
    return s->trace(s->user, name, 0u, count, values);
}

int32_t integration_overlay_call(Integration *s, uint32_t index,
                                 uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4, uint32_t a5,
                                 uint32_t a6) {
    AeOverlayHooks hooks = {&s->game, s, overlay_call, overlay_render};
    AeRam *ram = s->game.ram;
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    switch (index) {
    case 0: return ae_Mode_Boot(ram, &hooks);
    case 1: return ae_Boot_ClearScreen(ram, &hooks);
    case 2: return ae_Boot_SetupDisplay(ram, &hooks);
    case 3: return ae_Boot_DrawLogoA(ram, &hooks);
    case 4: return ae_Boot_DrawLogoB(ram, &hooks);
    case 5: return ae_Mode_02(ram, &hooks);
    case 6: return ae_Mode_03(ram, &hooks);
    case 7: return ae_Ovl182_DrawScene3D(ram, &hooks);
    case 8: return ae_Ovl182_UpdateCamera(ram, &hooks);
    case 9: return ae_Ovl182_Init(ram, &hooks, a0);
    case 10: return ae_Ovl182_StateMachine(ram, &hooks);
    case 11: return ae_Ovl182_DrawIntroState(ram, &hooks);
    case 12: return ae_Ovl182_MenuInput(ram, &hooks);
    case 13: return ae_Ovl182_FadeTick(ram, &hooks);
    case 14: return ae_Ovl182_DrawFade(ram, &hooks);
    case 15: return ae_Ovl181_CheckCdHardwareIntegrity(ram, &hooks, a0, a1, a2);
    case 17: return ae_Ovl181_DrawIntegrityWarning(ram, &hooks);
    default: return 0;
    }
}
