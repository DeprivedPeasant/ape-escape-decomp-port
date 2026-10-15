#include "../../src/modes/ae_ovl188.h"

#if defined(_WIN32)
#define O188_TEST_EXPORT __declspec(dllexport)
#else
#define O188_TEST_EXPORT
#endif

O188_TEST_EXPORT AeRam *ovl188_ram_create(void) { return ae_ram_create(); }
O188_TEST_EXPORT void ovl188_ram_destroy(AeRam *ram) { ae_ram_destroy(ram); }
O188_TEST_EXPORT uint8_t *ovl188_ram_main(AeRam *ram) {
    return ae_ptr(ram, AE_RAM_BASE, AE_RAM_SIZE);
}
O188_TEST_EXPORT uint8_t *ovl188_ram_scratch(AeRam *ram) {
    return ae_ptr(ram, AE_SCRATCH_BASE, AE_SCRATCH_SIZE);
}
O188_TEST_EXPORT void ovl188_ram_clear_error(AeRam *ram) {
    ae_ram_clear_error(ram);
}
O188_TEST_EXPORT int ovl188_ram_failed(const AeRam *ram) {
    return ae_ram_failed(ram);
}
O188_TEST_EXPORT const char *ovl188_ram_error(const AeRam *ram) {
    return ae_ram_error(ram);
}

O188_TEST_EXPORT int32_t ovl188_call(AeRam *ram, uint32_t function,
                                     const uint32_t args[6],
                                     uint32_t stack_pointer,
                                     AeOvl188CallFn call,
                                     AeOvl188RenderFn render, void *user) {
    const AeOvl188Hooks hooks = {
        .user = user,
        .stack_pointer = stack_pointer,
        .stage_hooks = NULL,
        .call = call,
        .render = render,
    };
    switch (function) {
    case 0: ae_StageStartUi_LookAt(ram, &hooks, args[0], args[1]); return 0;
    case 1: return (int32_t)ae_StageStartUi_StringTableIndex(ram, args[0], (int32_t)args[1]);
    case 2: return ae_Mode_StageStart(ram, &hooks);
    case 3: return ae_StageStartUi_RunA(ram, &hooks);
    case 4: ae_StageStartUi_CopyStrip(ram, &hooks); return 0;
    case 5: return ae_StageStartUi_DrawWipeA(ram, &hooks);
    case 6: return ae_StageStartUi_RunB(ram, &hooks);
    case 7: return ae_StageStartUi_DrawWipeB(ram, &hooks);
    case 8: ae_StageStartUi_MeshInit(ram, &hooks); return 0;
    case 9: ae_StageStartUi_SetAreaMeshTable(ram); return 0;
    case 10: ae_StageStartUi_AreaInit(ram, &hooks, (int32_t)args[0]); return 0;
    case 11: ae_StageStartUi_RunScript(ram, &hooks); return 0;
    case 12: ae_StageStartUi_DrawNumber(ram, &hooks); return 0;
    case 13: ae_StageStartUi_DrawTitleLines(ram, &hooks, args[0], (int32_t)args[1]); return 0;
    case 14: ae_StageStartUi_DrawObjectives(ram, &hooks); return 0;
    case 15: ae_StageStartUi_DrawStageIcon(ram, &hooks); return 0;
    case 16: ae_StageStartUi_DrawText(ram, &hooks, args[0]); return 0;
    case 17: ae_StageStartUi_SetDrawStp(ram, &hooks); return 0;
    case 18: ae_StageStartUi_DrawCounter(ram, &hooks, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2]); return 0;
    case 19: ae_StageStartUi_UpdateObjects(ram, &hooks, args[0]); return 0;
    case 20: ae_StageStartUi_CopyBuffer(ram, &hooks, (int32_t)args[0]); return 0;
    case 21: ae_StageStartUi_ClampVerts(ram, (int32_t)args[0], (int32_t)args[1], (int32_t)args[2]); return 0;
    case 22: ae_StageStartUi_WipeQuadA(ram, &hooks, (int32_t)args[0], (int32_t)args[1]); return 0;
    case 23: ae_StageStartUi_WipeQuadB(ram, &hooks, (int32_t)args[0], (int32_t)args[1]); return 0;
    case 24: ae_StageStartUi_DrawScanlines(ram, &hooks); return 0;
    case 25: ae_RippleGrid_Init2(ram, &hooks); return 0;
    case 26: ae_RippleGrid_Smooth2(ram); return 0;
    case 27: ae_RippleGrid_Draw2(ram, &hooks); return 0;
    default: ae_ram_fail(ram, "unknown overlay 188 test function %u", function); return 0;
    }
}
