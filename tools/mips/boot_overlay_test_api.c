/* The boot unit dispatches through the central public adapters in proxy runs. */
#include "../../src/game/ae_calls.h"
#include "../../src/modes/ae_ovl181.h"
#include "../../src/modes/ae_ovl182.h"
typedef int32_t (*Trace)(void *, const char *, uint32_t, uint32_t, const uint32_t *);
/* Matches the owning unit shim; RAM and callback ownership stay there. */
typedef struct Integration { AeGame game; Trace trace; void *user; } Integration;
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

int32_t game_proxy_overlay_call(Integration *s, uint32_t index,
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
