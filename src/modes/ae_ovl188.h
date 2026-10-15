#ifndef AE_OVL188_H
#define AE_OVL188_H

#include "../core/ae_ram.h"
#include "../world/ae_stage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Calls across the stage-start overlay boundary. `name` and `address` identify the original
   SDK / EXE entry; args contains the original argument words in o32 order. Rendering calls
   carry semantic values to the native renderer and never build PS1 packets in this module. */
typedef int32_t (*AeOvl188CallFn)(void *user, AeRam *ram, const char *name,
                                  uint32_t address, uint32_t argc,
                                  const uint32_t *args);
typedef void (*AeOvl188RenderFn)(void *user, AeRam *ram, const char *name,
                                 uint32_t argc, const uint32_t *args);
/* Untouched high bytes of XaPlay's filter/mode byte locals (selectors 0/1). */
typedef uint32_t (*AeOvl188XaPaddingFn)(void *user, AeRam *ram, uint32_t local);

typedef struct AeOvl188Hooks {
    void *user;
    uint32_t stack_pointer;
    const AeStageHooks *stage_hooks;
    AeOvl188CallFn call;
    AeOvl188RenderFn render;
    AeOvl188XaPaddingFn xa_padding;
} AeOvl188Hooks;

void ae_StageStartUi_LookAt(AeRam *ram, const AeOvl188Hooks *hooks,
                            uint32_t eye, uint32_t target);
uint32_t ae_StageStartUi_StringTableIndex(AeRam *ram, uint32_t strings,
                                          int32_t index);
int32_t ae_Mode_StageStart(AeRam *ram, const AeOvl188Hooks *hooks);
int32_t ae_StageStartUi_RunA(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_CopyStrip(AeRam *ram, const AeOvl188Hooks *hooks);
int32_t ae_StageStartUi_DrawWipeA(AeRam *ram, const AeOvl188Hooks *hooks);
int32_t ae_StageStartUi_RunB(AeRam *ram, const AeOvl188Hooks *hooks);
int32_t ae_StageStartUi_DrawWipeB(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_MeshInit(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_SetAreaMeshTable(AeRam *ram);
void ae_StageStartUi_AreaInit(AeRam *ram, const AeOvl188Hooks *hooks,
                              int32_t static_model_count);
void ae_StageStartUi_RunScript(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_DrawNumber(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_DrawTitleLines(AeRam *ram, const AeOvl188Hooks *hooks,
                                    uint32_t records, int32_t count);
void ae_StageStartUi_DrawObjectives(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_DrawStageIcon(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_DrawText(AeRam *ram, const AeOvl188Hooks *hooks,
                              uint32_t text);
void ae_StageStartUi_SetDrawStp(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_StageStartUi_DrawCounter(AeRam *ram, const AeOvl188Hooks *hooks,
                                 int32_t x, int32_t y, int32_t value);
void ae_StageStartUi_UpdateObjects(AeRam *ram, const AeOvl188Hooks *hooks,
                                   uint32_t objects);
void ae_StageStartUi_CopyBuffer(AeRam *ram, const AeOvl188Hooks *hooks,
                                int32_t row);
void ae_StageStartUi_ClampVerts(AeRam *ram, int32_t mode, int32_t count,
                                int32_t y);
void ae_StageStartUi_WipeQuadA(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t x, int32_t height);
void ae_StageStartUi_WipeQuadB(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t x, int32_t height);
void ae_StageStartUi_DrawScanlines(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_RippleGrid_Init2(AeRam *ram, const AeOvl188Hooks *hooks);
void ae_RippleGrid_Smooth2(AeRam *ram);
void ae_RippleGrid_Draw2(AeRam *ram, const AeOvl188Hooks *hooks);

#ifdef __cplusplus
}
#endif
#endif
