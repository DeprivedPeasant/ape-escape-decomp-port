#ifndef AE_VRAM_ANIM_H
#define AE_VRAM_ANIM_H
#include "../core/ae_ram.h"
#include <stdint.h>

#define AE_VA_G_AREA_ANIM_COUNT          0x800B98F0u
#define AE_VA_G_AREA_ANIM_LIST           0x800B98F4u
#define AE_VA_AREA_ANIM_TYPE_FILTER        1u
#define AE_VA_RECT_X                       0x00u
#define AE_VA_RECT_Y                       0x02u
#define AE_VA_RECT_WIDTH                   0x04u
#define AE_VA_RECT_HEIGHT                  0x06u
#define AE_VA_AREA_ANIM_RECORD_SIZE          16u
#define AE_VA_AREA_ANIM_RECT_X               4u
#define AE_VA_AREA_ANIM_RECT_Y               6u
#define AE_VA_AREA_ANIM_SOURCE_X             8u
#define AE_VA_AREA_ANIM_SOURCE_Y             10u
#define AE_VA_AREA_ANIM_RECT_WIDTH           12u
#define AE_VA_AREA_ANIM_RECT_HEIGHT          13u

/* Public SDK API; supplied by the native renderer. RECT is an AeRam address. */
typedef struct AeVramAnimHooks {
    void *user;
    int32_t (*MoveImage)(void *user, AeRam *ram, uint32_t rect, int32_t x, int32_t y);
} AeVramAnimHooks;
int32_t ae_AreaSetVramAnimList(AeRam *ram, const AeVramAnimHooks *hooks, uint32_t list);
#endif
