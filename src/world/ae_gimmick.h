#ifndef AE_GIMMICK_H
#define AE_GIMMICK_H

#include "../core/ae_ram.h"

/* Original RAM globals used by the gimmick activation code. */
#define AE_G_GIMMICKS             0x800F4570u
#define AE_G_COLLIDERS            0x800F455Cu
#define AE_G_AREA_PROPS           0x800B4D64u
#define AE_G_CURRENT_LEVEL        0x800F4474u
#define AE_G_CURRENT_AREA         0x800F4476u

/* Gimmick records are 0x18 bytes (tools/structs.txt: Gimmick). */
#define AE_GIMMICK_SIZE           0x18u
#define AE_GIMMICK_TYPE           0x00u
#define AE_GIMMICK_STATE          0x02u
#define AE_GIMMICK_COLLIDER       0x04u
#define AE_GIMMICK_TIMER          0x06u
#define AE_GIMMICK_SUBTYPE        0x08u
#define AE_GIMMICK_PARAM_A        0x0Au
#define AE_GIMMICK_LINK           0x0Cu
#define AE_GIMMICK_PARAM_E        0x0Eu
#define AE_GIMMICK_PARAM_10       0x10u
#define AE_GIMMICK_PARAM_12       0x12u
#define AE_GIMMICK_PARAM_14       0x14u
#define AE_GIMMICK_PARAM_16       0x16u

/* Collider records are 0x38 bytes (tools/structs.txt: Collider). */
#define AE_COLLIDER_SIZE          0x38u
#define AE_COLLIDER_POS_X         0x00u
#define AE_COLLIDER_POS_Y         0x02u
#define AE_COLLIDER_POS_Z         0x04u
#define AE_COLLIDER_HALF_X        0x06u
#define AE_COLLIDER_HALF_Y        0x08u
#define AE_COLLIDER_HALF_Z        0x0Au
#define AE_COLLIDER_PROP_INDEX    0x0Eu
#define AE_COLLIDER_MODE          0x14u
#define AE_COLLIDER_TYPE          0x12u
#define AE_COLLIDER_FLAGS         0x18u

/* AreaProp records are 0x20 bytes; zoneMask is used by the render visibility pass. */
#define AE_AREA_PROP_SIZE         0x20u
#define AE_AREA_PROP_ZONE_MASK    0x16u

#define AE_COLLIDER_DISABLED      0x8000u
#define AE_COLLIDER_STATE_MASK    0x000Fu
#define AE_COLLIDER_STATE_2       0x0002u

/* Calls outside the port boundary keep their original function names.  The arguments are
 * 32-bit original argument words; pointers are PSX RAM addresses.  A missing callback fails
 * closed by setting AeRam's sticky error. */
typedef int32_t (*AeGimmickHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);

typedef struct AeGimmickHooks {
    void *user;
    AeGimmickHookFn PlaySfx;             /* 800877F8: sound playback */
    AeGimmickHookFn PlaySfxAt;           /* 80087A20: positional sound playback */
    AeGimmickHookFn PropSetMesh;         /* 80090E54: renderer-owned mesh replacement */
    AeGimmickHookFn FUN_801414a0;        /* area overlay callback for gimmick type 0x29 */
} AeGimmickHooks;

int32_t ae_GimmickActivate(AeRam *r, const AeGimmickHooks *h, int32_t index, uint32_t source_gimmick); /* 8008FC30 */
int32_t ae_GimmickDeactivateChain(AeRam *r, uint32_t gimmick);                                        /* 80090230 */
int32_t ae_SwingDoorSetPivot(AeRam *r, uint32_t collider, int32_t direction, int32_t pivot_mode);     /* 8008ECFC */

#endif
