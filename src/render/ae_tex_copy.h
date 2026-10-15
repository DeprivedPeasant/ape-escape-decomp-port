#ifndef AE_TEX_COPY_H
#define AE_TEX_COPY_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Calls outside this module receive the original game argument words. */
typedef uint32_t (*AeTexCopyHookFn)(void *, AeRam *, uint32_t, uint32_t,
                                    uint32_t, uint32_t);

typedef struct AeTexCopyHooks {
    void *user;
    uint32_t stack_pointer; /* Entry SP used for the original stack-backed RECT. */
    AeTexCopyHookFn HeapAlloc;       /* 8008C09C (size) */
    /* Public SDK API; the native renderer supplies these callbacks. */
    int32_t (*DrawSync)(void *, AeRam *, int32_t mode);
    int32_t (*LoadImage)(void *, AeRam *, uint32_t rect, uint32_t image);
    int32_t (*StoreImage)(void *, AeRam *, uint32_t rect, uint32_t image);
} AeTexCopyHooks;

int32_t ae_AreaTexCopyInit(AeRam *r, const AeTexCopyHooks *hooks,
                           uint32_t rect_address, uint32_t table_index,
                           uint32_t color_rgba, uint32_t unused); /* 8005E7EC */
int32_t ae_HeapFree(AeRam *r, uint32_t pointer); /* 8008C064 */

#ifdef __cplusplus
}
#endif

#endif
