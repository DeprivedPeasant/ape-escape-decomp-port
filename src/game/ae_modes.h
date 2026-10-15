#ifndef AE_MODES_H
#define AE_MODES_H
#include "../core/ae_ram.h"

/* Named subsystem calls. Native pointers never travel in PSX argument words.
 * stack_pointer locates addressable local buffers passed to platform APIs. */
typedef int32_t (*AeModeCallFn)(void *, AeRam *, const char *, uint32_t,
                              uint32_t, const uint32_t *);
typedef struct AeModeHooks {
    void *user;
    AeModeCallFn original_call;
    uint32_t stack_pointer;
} AeModeHooks;

void ae_NullInit(AeRam *r);
uint32_t ae_BumpAllocPeek(AeRam *r, uint32_t size);
void ae_ResetFrameFlip(AeRam *r);
int32_t ae_StageInitMemory(AeRam *r, const AeModeHooks *h, uint32_t list_index);
int32_t ae_StageRenderInit(AeRam *r, const AeModeHooks *h);
int32_t ae_Mode_04(AeRam *r);
int32_t ae_Mode_06(AeRam *r, const AeModeHooks *h);
int32_t ae_main(AeRam *r, const AeModeHooks *h);
int32_t ae_Mode_Stage(AeRam *r, const AeModeHooks *h);
/* Native scheduling of the original stage loop, sharing its body and exit. */
int ae_mode_stage_frame(AeRam *r, const AeModeHooks *h);
void ae_mode_stage_begin(AeRam *r, const AeModeHooks *h);
#endif
