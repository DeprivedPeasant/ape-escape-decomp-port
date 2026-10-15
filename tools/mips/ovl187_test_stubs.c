/* Explicit dependency models for check_ovl187; never linked into the game. */
#include "../../src/modes/ae_ovl187.h"
#include "../../src/core/ae_pose.h"
#include "../../src/player/ae_player_spawn.h"
#include "../../src/sound/ae_bgm.h"
int32_t o187_test_call(AeRam *, const char *, uint32_t, uint32_t, const uint32_t *);

int32_t ae_AreaGetLevel(AeRam *r, const AeAreaSetHooks *h, int32_t area_index) {
    (void)h;
    return (int32_t)o187_test_call(r, "AreaGetLevel", 0x80010CC0u, 1u, (const uint32_t[]){(uint32_t)area_index});
}

void ae_AreaSetZoneBoxes(AeRam *r, const AeAreaSetHooks *h, uint32_t feature) {
    (void)h;
    (void)o187_test_call(r, "AreaSetZoneBoxes", 0x80043380u, 1u, (const uint32_t[]){(uint32_t)feature});
}

uint32_t ae_HeapAlloc(AeRam *r, const AeAreaSetHooks *h, uint32_t byte_count) {
    (void)h;
    return (uint32_t)o187_test_call(r, "HeapAlloc", 0x8008C09Cu, 1u, (const uint32_t[]){(uint32_t)byte_count});
}

void ae_HeapInit(AeRam *r, const AeAreaSetHooks *h) {
    (void)h;
    (void)o187_test_call(r, "HeapInit", 0x8008C01Cu, 0u, NULL);
}

void ae_HudDrawTime(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y, int32_t frames) {
    (void)h;
    (void)o187_test_call(r, "HudDrawTime", 0x8005F978u, 3u, (const uint32_t[]){(uint32_t)x, (uint32_t)y, (uint32_t)frames});
}

void ae_MatrixRotYMul(AeRam *r, const AeMiscHooks *h, int32_t angle, uint32_t matrix) {
    (void)h;
    (void)o187_test_call(r, "MatrixRotYMul", 0x80014750u, 2u, (const uint32_t[]){(uint32_t)angle, (uint32_t)matrix});
}

void ae_MeshCalcEnvUVs(AeRam *r, const AeMiscHooks *h, uint32_t instance) {
    (void)h;
    (void)o187_test_call(r, "MeshCalcEnvUVs", 0x80016D60u, 1u, (const uint32_t[]){(uint32_t)instance});
}

void ae_ModelInit(AeRam *r, const AeAreaSetHooks *h, uint32_t instance, uint32_t model_header) {
    (void)h;
    (void)o187_test_call(r, "ModelInit", 0x80015AC0u, 2u, (const uint32_t[]){(uint32_t)instance, (uint32_t)model_header});
}

void ae_ModelPoseBones(AeRam *r, uint32_t actor, uint32_t flags) {
    (void)o187_test_call(r, "ModelPoseBones", 0x80015780u, 2u, (const uint32_t[]){(uint32_t)actor, (uint32_t)flags});
}

int32_t ae_PlaySfx(AeRam *r, const AeSoundHw *hw, int32_t id) {
    (void)hw;
    return (int32_t)o187_test_call(r, "PlaySfx", 0x800877F8u, 1u, (const uint32_t[]){(uint32_t)id});
}

uint32_t ae_ProgressGetByte(AeRam *r, uint32_t offset) {
    return (uint32_t)o187_test_call(r, "ProgressGetByte", 0x800963ACu, 1u, (const uint32_t[]){(uint32_t)offset});
}

void ae_SetColorMatrixMode(AeRam *r, const AeItemsInitHooks *h, uint32_t mode, uint32_t red, uint32_t green, uint32_t blue) {
    (void)h;
    (void)o187_test_call(r, "SetColorMatrixMode", 0x80015D58u, 4u, (const uint32_t[]){(uint32_t)mode, (uint32_t)red, (uint32_t)green, (uint32_t)blue});
}

int32_t ae_SoundStop(AeRam *r, const AeStageFadeHooks *h, int32_t mode) {
    (void)h;
    return (int32_t)o187_test_call(r, "SoundStop", 0x80086FD0u, 1u, (const uint32_t[]){(uint32_t)mode});
}

void ae_TextSetCursor(AeRam *r, int32_t x, int32_t y) {
    (void)o187_test_call(r, "TextSetCursor", 0x8008DDB0u, 2u, (const uint32_t[]){(uint32_t)x, (uint32_t)y});
}

int32_t ae_XaPlay(AeRam *r, const AePlayer1Hooks *hooks, int32_t index) {
    (void)hooks;
    return (int32_t)o187_test_call(r, "XaPlay", 0x80011F6Cu, 1u, (const uint32_t[]){(uint32_t)index});
}

void ae_pad_PadUpdate(AeRam *r, const AePadHooks *h, uint32_t flags) {
    (void)h;
    (void)o187_test_call(r, "PadUpdate", 0x8001325Cu, 1u, (const uint32_t[]){(uint32_t)flags});
}
