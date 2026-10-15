#include "../../src/ui/ae_hud.h"

#if defined(_WIN32)
#define AE_HUD_TEST_EXPORT __declspec(dllexport)
#else
#define AE_HUD_TEST_EXPORT
#endif

static int32_t ae_hud_test_sfx_ids[32];
static uint32_t ae_hud_test_sfx_count_value;

/* The production ports called by ae_hud.c are mirrored here so the focused
 * checker DLL does not pull in unrelated player, world and sound systems. */
int ae_player_is_swimming(uint16_t state) {
    return (uint32_t)(state - 0x46u) < 2u || state == 0x49u ||
           state == 0x4Au || state == 0x4Cu;
}

int ae_player_can_switch_gadget(uint16_t mode, uint16_t state) {
    if (mode == 9u || (mode < 4u && mode != 0u) || mode == 0x0Bu)
        return 0;
    return !ae_player_is_swimming(state);
}

int32_t ae_PlayerCanExitStage(AeRam *ram) {
    uint32_t exit_code = ae_rd8(ram, 0x800F447Cu);
    if (exit_code < 3u || exit_code > 5u) return 0;
    uint32_t player_state = ae_rd16(ram, 0x800EC1E0u + 0x5Eu);
    if (player_state == 0x23u || player_state == 0x4Au || player_state == 0x2Du)
        return 0;
    uint32_t state_class = ae_rd32(ram, 0x800EC1E0u + 0x70u) & 0xF0u;
    return state_class != 0xE0u;
}

int32_t ae_AreaFeature30GetState(AeRam *ram) {
    return (int32_t)ae_rd8(ram, 0x800F5AA0u);
}

int32_t ae_IsTimeAttack(AeRam *ram) {
    return ae_rd16(ram, 0x800F4470u) == 0x13u ||
           ae_rd16(ram, 0x800F4472u) == 3u;
}

int32_t ae_PlaySfx(AeRam *ram, const AeSoundHw *sound, int32_t id) {
    (void)sound;
    if (ae_hud_test_sfx_count_value < 32u)
        ae_hud_test_sfx_ids[ae_hud_test_sfx_count_value++] = id;
    uint32_t effect = 0x800B2D08u + (uint32_t)id * 8u;
    int32_t bank_slot = ae_rds16(ram, effect);
    int32_t bank = ae_rds16(ram, 0x800BF670u + (uint32_t)bank_slot * 2u);
    return bank < 0 ? -1 : 0;
}

AE_HUD_TEST_EXPORT AeRam *ae_hud_test_create(void) {
    return ae_ram_create();
}

AE_HUD_TEST_EXPORT void ae_hud_test_destroy(AeRam *ram) {
    ae_ram_destroy(ram);
}

AE_HUD_TEST_EXPORT uint8_t *ae_hud_test_main(AeRam *ram) {
    return ae_ptr(ram, AE_RAM_BASE, AE_RAM_SIZE);
}

AE_HUD_TEST_EXPORT uint8_t *ae_hud_test_scratch(AeRam *ram) {
    return ae_ptr(ram, AE_SCRATCH_BASE, AE_SCRATCH_SIZE);
}

AE_HUD_TEST_EXPORT uint8_t *ae_hud_test_pointer(AeRam *ram,
                                                 uint32_t address,
                                                 uint32_t size) {
    return ae_ptr(ram, address, size);
}

AE_HUD_TEST_EXPORT void ae_hud_test_clear_sfx(void) {
    ae_hud_test_sfx_count_value = 0;
}

AE_HUD_TEST_EXPORT uint32_t ae_hud_test_sfx_count(void) {
    return ae_hud_test_sfx_count_value;
}

AE_HUD_TEST_EXPORT int32_t ae_hud_test_sfx_id(uint32_t index) {
    return index < ae_hud_test_sfx_count_value ? ae_hud_test_sfx_ids[index] : 0;
}

AE_HUD_TEST_EXPORT void ae_hud_test_clear_error(AeRam *ram) {
    ae_ram_clear_error(ram);
}

AE_HUD_TEST_EXPORT int ae_hud_test_failed(const AeRam *ram) {
    return ae_ram_failed(ram);
}

AE_HUD_TEST_EXPORT const char *ae_hud_test_error(const AeRam *ram) {
    return ae_ram_error(ram);
}

AE_HUD_TEST_EXPORT int32_t ae_hud_test_call(AeRam *ram,
                                             const AeHudHooks *hooks,
                                             uint32_t function,
                                             const uint32_t args[6]) {
    switch (function) {
    case 0: return ae_RadarUsable(ram);
    case 1: ae_HudDrawIconSprite(ram, hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2], (int32_t)args[3]); return 0;
    case 2: ae_HudDrawGadgetIcons(ram, hooks); return 0;
    case 3: return ae_GetDAT_800B01C2(ram);
    case 4: ae_StageDrawScreenOverlay(ram, hooks, args[0], args[1]); return 0;
    case 5: ae_HudDrawHealth(ram, hooks); return 0;
    case 6: ae_AreaTexCopyDraw(ram, hooks, (int32_t)args[0]); return 0;
    case 7: ae_HudPanelA_DrawIcon(ram, hooks, (int32_t)args[0], (int32_t)args[1]); return 0;
    case 8: ae_HudPanelB_DrawSprite(ram, hooks, (int32_t)args[0], (int32_t)args[1]); return 0;
    case 9: ae_HudDrawDigit(ram, hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2]); return 0;
    case 10: ae_HudDrawNumber(ram, hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2]); return 0;
    case 11: ae_HudDrawTime(ram, hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2]); return 0;
    case 12: ae_HudDrawIcon16(ram, hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2]); return 0;
    case 13: return ae_HudCounterC_Draw(ram, hooks);
    case 14: ae_HudDrawTimerGauge(ram, hooks, (int32_t)args[0]); return 0;
    case 15: ae_HudDrawGadgetSlotsFrame(ram, hooks); return 0;
    case 16: ae_HudDrawGadgetSlotIcons(ram, hooks); return 0;
    case 17: ae_HudDrawGadgetSlots(ram, hooks); return 0;
    case 18: return (int32_t)ae_HudDrawGadgetIcon(ram, hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2], args[3]);
    case 19: ae_FontDrawGlyph(ram, hooks, (int32_t)args[0], (int32_t)args[1], args[2]); return 0;
    case 20: return (int32_t)ae_TextNextGlyph(ram, args[0], args[1]);
    case 21: ae_TextSetCursor(ram, (int32_t)args[0], (int32_t)args[1]); return 0;
    case 22: ae_MessageBoxUpdateDraw(ram, hooks); return 0;
    case 23: ae_MessageBoxDrawText(ram, hooks); return 0;
    case 24: ae_MessageBoxDrawPrompt(ram, hooks); return 0;
    case 25: ae_MessageBoxDrawWindow(ram, hooks); return 0;
    case 26: return ae_HudPanelA_Update(ram, hooks);
    case 27: return ae_HudPanelB_Update(ram, hooks);
    case 28: return ae_HudCounterC_Update(ram, hooks);
    default: return 0;
    }
}
