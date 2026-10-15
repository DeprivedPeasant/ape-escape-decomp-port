/* Integration surface for the save-menu overlay differential checker. */
#include "../../src/game/ae_calls.h"
#include "../../src/modes/ae_ovl186a.h"

#include <stdlib.h>
#include <string.h>

void ae_game_wire_body(AeGame *game);
void ae_game_wire_modules(AeGame *game);
int32_t ovl186a_test_card_state_step(AeRam *ram,
                                     const AeOverlayHooks *hooks);
int32_t ovl186a_test_card_result_step(AeRam *ram,
                                      const AeOverlayHooks *hooks);

typedef int32_t (*Trace)(void *, const char *, uint32_t, uint32_t,
                         const uint32_t *);
typedef struct Integration {
    AeGame game;
    Trace trace;
    void *user;
} Integration;

static int32_t trace_words(void *user, const char *name, uint32_t address,
                           uint32_t count, const uint32_t *args) {
    Integration *s = user;
    return s->trace(s->user, name, address, count, args);
}

static int32_t trace_call(void *user, const char *name, uint32_t address,
                          uint32_t count, uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3, uint32_t a4,
                          uint32_t a5) {
    const uint32_t args[6] = {a0, a1, a2, a3, a4, a5};
    return trace_words(user, name, address, count, args);
}

static int32_t trace_four(void *user, AeRam *ram, const char *name,
                          uint32_t address, uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3) {
    Integration *s = user;
    const uint32_t args[4] = {a0, a1, a2, a3};
    (void)ram;
    return trace_words(s, name, address, 4u, args);
}

static int32_t trace_missing(void *user, AeRam *ram, const char *name,
                             uint32_t address, uint32_t a0, uint32_t a1,
                             uint32_t a2, uint32_t a3) {
    return trace_four(user, ram, name, address, a0, a1, a2, a3);
}

static int32_t trace_missing_words(void *user, AeRam *ram, const char *name,
                                   uint32_t address, uint32_t count,
                                   const uint32_t *args) {
    (void)ram;
    return trace_words(user, name, address, count, args);
}

static int32_t trace_render(void *user, AeRam *ram, const char *name,
                            uint32_t a0, uint32_t a1, uint32_t a2,
                            uint32_t a3) {
    Integration *s = user;
    const uint32_t args[4] = {a0, a1, a2, a3};
    (void)ram;
    return trace_words(s, name, 0u, 4u, args);
}

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
    host.missing = trace_missing;
    host.missing_words = trace_missing_words;
    host.render = trace_render;
    integration_callbacks(&host);
    host.sound_hw.user = s;
    host.sound_hw.wait_idle = test_platform_SpuWaitIdle;
    ae_game_init_legacy(&s->game, ram, &host);
    ae_game_wire_modules(&s->game);
    ae_game_wire_body(&s->game);
    return s;
}

AeRam *integration_ram(Integration *s) { return s->game.ram; }

void integration_destroy(Integration *s) {
    if (s == NULL) return;
    ae_ram_destroy(s->game.ram);
    free(s);
}

int integration_overlay_has_port(uint32_t address) {
    const AeGameCall *call = ae_game_find_call(address);
    return call != NULL && call->overlay_member == 0u;
}

static int32_t overlay_call(void *user, AeRam *ram, const char *name,
                            uint32_t address, uint32_t count,
                            const uint32_t *arguments) {
    (void)ram;
    return trace_words(user, name, address, count, arguments);
}

static int32_t overlay_render(void *user, AeRam *ram, const char *name,
                              uint32_t count, const uint32_t *values) {
    (void)ram;
    return trace_words(user, name, 0u, count, values);
}

int32_t integration_overlay_call(Integration *s, uint32_t index,
                                 uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4, uint32_t a5,
                                 uint32_t a6) {
    AeOverlayHooks hooks = {&s->game, s, overlay_call, overlay_render};
    AeRam *ram = s->game.ram;
    switch (index) {
    case 0: return ae_SaveUi_DrawText(ram, &hooks, a0, (int32_t)a1, (int32_t)a2, (int32_t)a3);
    case 1: return ae_SaveUi_TextWidth(ram, a0);
    case 2: {
        const uint32_t args[4] = {a0, a1, a2, a3};
        return overlay_render(s, ram, "SaveUi_DrawWindow", 4u, args);
    }
    case 3: {
        const uint32_t args[4] = {a0, a1, a2, a3};
        return overlay_render(s, ram, "SaveUi_DrawWindowBorder", 4u, args);
    }
    case 4: ae_SaveUi_LookAt(ram, a0, a1); return 0;
    case 5: return (int32_t)ae_SaveUi_StringTableIndex(ram, a0, (int32_t)a1);
    case 6: return ae_Mode_SaveMenu(ram, &hooks, a0);
    case 7: ae_SaveUi_FrameReset(ram, &hooks); return 0;
    case 8: return ae_SaveUi_SceneInit(ram, &hooks);
    case 9: return ae_SaveUi_RunMenuA(ram, &hooks);
    case 10: return ae_SaveUi_RunMenuB(ram, &hooks);
    case 11: ae_SaveUi_FileSelectInit(ram, &hooks); return 0;
    case 12: return ae_SaveUi_TotalMonkeyCount(ram);
    case 13: return ae_SaveUi_MenuUpdate(ram, &hooks);
    case 14: return ae_SaveUi_RunMenuC(ram, &hooks, (int32_t)a0);
    case 15: return ae_SaveUi_CountEntries(ram, a0, a1);
    case 16: return ae_SaveUi_SetState(ram, &hooks, (int32_t)a0, (int32_t)a1);
    case 17: return ae_SaveUi_FormatNumber3(ram, (int32_t)a0, a1);
    case 18: return ae_SaveUi_MemCardStart(ram, &hooks, (int32_t)a0);
    case 19: return ae_SaveUi_StateTick(ram, &hooks);
    case 20: return ae_SaveUi_StateMachine(ram, &hooks);
    case 21: return ae_SaveUi_OnCardResult_0(ram, &hooks);
    case 22: return ae_SaveUi_OnCardResult_1(ram, &hooks);
    case 23: return ae_SaveUi_OnCardResult_2(ram, &hooks);
    case 24: return ae_SaveUi_OnCardResult_3(ram, &hooks);
    case 25: return ae_SaveUi_OnCardResult_4(ram, &hooks);
    case 26: return ae_SaveUi_OnCardResult_5(ram, &hooks);
    case 27: return ae_SaveUi_OnCardResult_6(ram, &hooks);
    case 28: ae_SaveUi_DrawFileSelect(ram, &hooks); return 0;
    case 29: ae_SaveUi_DrawSlot(ram, &hooks, (int32_t)a0, (int32_t)a1); return 0;
    case 30: return ovl186a_test_card_state_step(ram, &hooks);
    case 31: return ovl186a_test_card_result_step(ram, &hooks);
    default: (void)a4; (void)a5; (void)a6; return 0;
    }
}

int32_t integration_get_error(Integration *s, char *out, uint32_t size) {
    const char *error = ae_ram_error(s->game.ram);
    if (error == NULL || error[0] == '\0') {
        if (size != 0u) out[0] = '\0';
        return 0;
    }
    if (size != 0u) {
        strncpy(out, error, size - 1u);
        out[size - 1u] = '\0';
    }
    return 1;
}

void integration_clear_error(Integration *s) { ae_ram_clear_error(s->game.ram); }
