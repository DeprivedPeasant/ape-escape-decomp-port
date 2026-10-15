/* Stage-clear unit boundaries and dispatcher, shared with the central ABI check. */
#include "../../src/modes/ae_ovl187.h"
#include "../../src/core/ae_pose.h"
static const AeOvl187Hooks *active_hooks;
int32_t o187_test_call(AeRam *r, const char *name, uint32_t address, uint32_t count, const uint32_t *args) {
    if (!active_hooks || !active_hooks->call) { ae_ram_fail(r, "187 test boundary missing: %s", name); return 0; }
    return active_hooks->call(active_hooks->user, r, name, address, count, args);
}
AeRam *o187_ram_create(void) { return ae_ram_create(); }
void o187_ram_destroy(AeRam *r) { ae_ram_destroy(r); }
uint8_t *o187_ram_ptr(AeRam *r, uint32_t address, uint32_t size) { return ae_ptr(r,address,size); }
int o187_ram_failed(AeRam *r) { return ae_ram_failed(r); }
const char *o187_ram_error(AeRam *r) { return ae_ram_error(r); }
void o187_ram_clear_error(AeRam *r) { ae_ram_clear_error(r); }
uint32_t o187_test_sdk_compmatrix(AeRam *r, uint32_t a, uint32_t b, uint32_t c) { return (uint32_t)ae_sdk_CompMatrix(r, a, b, c); }
uint32_t o187_test_sdk_vector_normal(AeRam *r, uint32_t a, uint32_t b) { return (uint32_t)ae_sdk_VectorNormal(r, a, b); }
uint32_t o187_test_sdk_vector_normal_s(AeRam *r, uint32_t a, uint32_t b) { return (uint32_t)ae_sdk_VectorNormalS(r, a, b); }
uint32_t o187_test_sdk_vector_normal_ss(AeRam *r, uint32_t a, uint32_t b) { return (uint32_t)ae_sdk_VectorNormalSS(r, a, b); }
uint32_t o187_test_sdk_squareroot0(AeRam *r, int32_t a) { return (uint32_t)ae_sdk_SquareRoot0(r, a); }
static uint32_t dispatch(AeRam *r, const AeOvl187Hooks *h, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    switch(address) {
    case 0x80136000u: return (uint32_t)ae_StageClearUi_LookAt(r, h, (uint32_t)a0, (uint32_t)a1);
    case 0x801361D8u: return (uint32_t)ae_StageClearUi_StringTableIndex(r, (uint32_t)a0, (int32_t)a1);
    case 0x80136208u: return (uint32_t)ae_Mode_StageClear(r, h);
    case 0x801368FCu: return (uint32_t)ae_StageClearUi_DrawWipe(r, h);
    case 0x801372D0u: ae_StageClearUi_DrawText(r, h, (uint32_t)a0); return 0;
    case 0x801373F8u: ae_StageClearUi_DrawMessage(r, h, (uint32_t)a0); return 0;
    case 0x801374C8u: ae_StageClearUi_RunScript(r, h); return 0;
    case 0x80137CA8u: ae_StageClearUi_SetDrawStp(r, h); return 0;
    case 0x80137D20u: ae_StageClearUi_CopyStrip(r, h, (int32_t)a0); return 0;
    case 0x80137DC4u: ae_StageClearUi_DrawBlinkIcons(r, h); return 0;
    case 0x80137F4Cu: ae_StageClearUi_UpdateObjects(r, h, (uint32_t)a0); return 0;
    case 0x80138258u: ae_StageClearUi_DrawBox(r, h, (int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3); return 0;
    case 0x8013836Cu: ae_StageClearUi_DrawBackdrop(r, h); return 0;
    case 0x801384A0u: ae_StageClearUi_DrawTimes(r, h); return 0;
    case 0x80138660u: return (uint32_t)ae_StageClearUi_PlayXa(r, h);
    case 0x801386B8u: ae_StageClearUi_DrawScanlines(r, h); return 0;
    case 0x80138A9Cu: ae_StageClearUi_CheckRequirements(r, h); return 0;
    case 0x80138B0Cu: return (uint32_t)ae_RippleGrid_Init(r, h);
    case 0x80138C24u: ae_RippleGrid_Smooth(r); return 0;
    case 0x80138E20u: ae_RippleGrid_Draw(r, h); return 0;
    case 0x80138F38u: ae_StageClearUi_Setup(r, h); return 0;
    case 0x801392B0u: return (uint32_t)ae_StageClearUi_ObjMatrix(r, h, (int32_t)a0, (uint32_t)a1);
    case 0x8013939Cu: ae_StageClearUi_DrawStars(r, h); return 0;
    default: ae_ram_fail(r,"187 test entry missing: %08X",address); return 0;
    }
}
uint32_t o187_call(AeRam *r, const AeOvl187Hooks *h, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeOvl187Hooks *previous = active_hooks;
    active_hooks = h;
    uint32_t result = dispatch(r,h,address,a0,a1,a2,a3);
    active_hooks = previous;
    return result;
}
