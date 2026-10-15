#ifndef AE_ITEMS_INIT_H
#define AE_ITEMS_INIT_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*AeItemsInitCallHook)(void *, AeRam *, uint32_t, uint32_t,
                                      uint32_t, uint32_t);
typedef void (*AeItemsInitColorHook)(void *, AeRam *, uint32_t, uint32_t,
                                    uint32_t);
typedef void (*AeItemsInitMatrixHook)(void *, AeRam *, uint32_t matrix);

/* Calls outside this batch remain explicit host or native-renderer boundaries. */
typedef struct AeItemsInitHooks {
    void *user;
    AeItemsInitCallHook GetLevelFirstArea; /* 8009764C */
    AeItemsInitCallHook printf;            /* 800A3720 */
    /* Public SDK APIs with unchanged arguments; supplied by the native host. */
    AeItemsInitColorHook SetBackColor;
    AeItemsInitColorHook SetFarColor;
    AeItemsInitMatrixHook SetColorMatrix;
    int32_t (*rsin)(void *, AeRam *, int32_t angle);
    int32_t (*rcos)(void *, AeRam *, int32_t angle);
} AeItemsInitHooks;

uint32_t ae_GetCollectedItemsTable(AeRam *r); /* 800962E8 */
int32_t ae_CollectedItemsHas(AeRam *r, uint32_t item_id); /* 80084904 */
void ae_MatrixCopy8(AeRam *r, uint32_t destination, uint32_t source); /* 80015F24 */
void ae_SetColorMatrixMode(AeRam *r, const AeItemsInitHooks *h,
                           uint32_t mode, uint32_t red, uint32_t green,
                           uint32_t blue); /* 80015D58 */
void ae_ApplyLevelFog(AeRam *r, const AeItemsInitHooks *h); /* 800323F8 */
void ae_PickupSpawnSlot(AeRam *r, const AeItemsInitHooks *h, uint32_t kind, uint32_t position,
                        int32_t mode, int32_t slot); /* 80083844 */
void ae_ItemsLoadForArea(AeRam *r, const AeItemsInitHooks *h); /* 80083470 */

#ifdef __cplusplus
}
#endif
#endif
