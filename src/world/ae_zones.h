#ifndef AE_ZONES_H
#define AE_ZONES_H

/* Area-load terrain masks, collider zone masks, and debug-menu setup from the main EXE. */
#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Global objects used by the original routines. */
#define AE_ZONES_G_LEVEL_GRID 0x800EBFA0u
#define AE_ZONES_G_COLLIDERS 0x800F455Cu
#define AE_ZONES_G_COLLIDER_COUNT 0x800F45D4u
#define AE_ZONES_G_AREA_PROPS 0x800B4D64u
#define AE_ZONES_G_SPECIAL_COLLIDER 0x800B3384u
#define AE_ZONES_G_SPECIAL_COLLIDER_RESET 0x800B3388u

/* LevelGrid fields used by BuildTerrainBlockMask. */
#define AE_ZONES_GRID_WIDTH 0x00u
#define AE_ZONES_GRID_DEPTH 0x02u
#define AE_ZONES_GRID_CELLS 0x08u
#define AE_ZONES_SVECTOR_X 0x00u
#define AE_ZONES_SVECTOR_Y 0x02u
#define AE_ZONES_SVECTOR_Z 0x04u

/* Collider fields used by ColliderZoneMasksUpdate and ColliderComputeQuadrantMask. */
#define AE_ZONES_COLLIDER_SIZE 0x38u
#define AE_ZONES_COLLIDER_POSITION 0x00u
#define AE_ZONES_COLLIDER_POSITION_Z 0x04u
#define AE_ZONES_COLLIDER_HALF_SIZE 0x06u
#define AE_ZONES_COLLIDER_HALF_SIZE_Z 0x0Au
#define AE_ZONES_COLLIDER_PROP_INDEX 0x0Eu
#define AE_ZONES_COLLIDER_TYPE 0x12u
#define AE_ZONES_COLLIDER_FLAGS 0x18u
#define AE_ZONES_COLLIDER_ZONE_MASK 0x20u
#define AE_ZONES_COLLIDER_SHAPE_PRODUCT 0x0Cu
#define AE_ZONES_COLLIDER_SHAPE_EXTENT 0x1Cu
#define AE_ZONES_COLLIDER_DERIVED_A 0x32u
#define AE_ZONES_COLLIDER_DERIVED_B 0x34u
#define AE_ZONES_COLLIDER_FLOW_X 0x2Au
#define AE_ZONES_COLLIDER_FLOW_Y 0x2Cu
#define AE_ZONES_COLLIDER_FLOW_Z 0x2Eu
#define AE_ZONES_COLLIDER_FLOW_PAD 0x36u
#define AE_ZONES_COLLIDER_DISABLED 0x8000u
#define AE_ZONES_COLLIDER_SKIP_MASK 0x000Fu

/* AreaProp fields written when a collider owns a prop. */
#define AE_ZONES_AREA_PROP_SIZE 0x20u
#define AE_ZONES_AREA_PROP_POSITION 0x10u
#define AE_ZONES_AREA_PROP_ROTATION 0x1Cu

/* Debug menu item layout and globals. */
#define AE_ZONES_MENU_ITEM_SIZE 0x24u
#define AE_ZONES_MENU_ITEM_VARIABLE 0x08u
#define AE_ZONES_MENU_ITEM_CALLBACK 0x0Cu
#define AE_ZONES_MENU_ITEM_MINIMUM 0x10u
#define AE_ZONES_MENU_ITEM_MAXIMUM 0x14u
#define AE_ZONES_MENU_ITEM_VALUE 0x18u
#define AE_ZONES_MENU_ITEM_STEP 0x1Cu
#define AE_ZONES_MENU_ITEM_VALUE_SIZE 0x20u
#define AE_ZONES_MENU_ITEM_DEFAULT_CALLBACK 0x8007D110u
#define AE_ZONES_DEBUG_FLAGS_CALLBACK 0x8005CA14u
#define AE_ZONES_DEBUG_PLAYER_CALLBACK 0x8005CA84u
#define AE_ZONES_DEBUG_COUNTERS_CALLBACK 0x8005CB9Cu
#define AE_ZONES_DEBUG_FLAGS_ITEM 0x800AFEC8u
#define AE_ZONES_DEBUG_FLAGS_VARIABLE 0x800B01BCu
#define AE_ZONES_DEBUG_FLAGS_ITEM_GLOBAL 0x800B00C8u
#define AE_ZONES_DEBUG_PLAYER_ITEMS 0x800AFC1Cu
#define AE_ZONES_DEBUG_PLAYER_VARIABLES 0x800EC010u
#define AE_ZONES_DEBUG_PLAYER_ITEM_COUNT 18u
#define AE_ZONES_DEBUG_COUNTER_ITEMS 0x800AFB68u
#define AE_ZONES_DEBUG_COUNTER_VARIABLES 0x800F4610u
#define AE_ZONES_DEBUG_COUNTER_ITEM_GLOBAL 0x800B0080u
#define AE_ZONES_DEBUG_COUNTER_COUNT 5u
#define AE_ZONES_DEBUG_COUNTER2_ITEMS 0x800AFF10u
#define AE_ZONES_DEBUG_COUNTER2_VARIABLES 0x800F4614u
#define AE_ZONES_DEBUG_COUNTER2_ITEM_GLOBAL 0x800B00ECu
#define AE_ZONES_DEBUG_COUNTER2_COUNT 6u

typedef struct AeZonesHooks {
    void *user;
    /* AreaBumpAlloc @8008C1B8. */
    uint32_t (*area_bump_alloc)(void *user, AeRam *r, uint32_t byte_count);
    /* GetColliderQuadrantMaskXZ @800889B4. */
    int32_t (*get_collider_quadrant_mask_xz)(void *user, AeRam *r, int32_t x, int32_t z);
    /* FUN_8013FC00: area/overlay-specific callback absent from the EXE image. */
    void (*FUN_8013fc00)(void *user, AeRam *r, int32_t collider_index);
} AeZonesHooks;

/* 800147D0 */
void ae_SvecToDiagMatrix(AeRam *r, uint32_t vector, uint32_t matrix);
/* 800438F8 */
uint32_t ae_BuildTerrainBlockMask(AeRam *r, const AeZonesHooks *h);
/* 8005CA34 */
void ae_DebugMenuInitFlags(AeRam *r);
/* 8005CB1C */
void ae_DebugMenuInitPlayerParams(AeRam *r);
/* 8005CC58 */
void ae_DebugMenuInitCounters(AeRam *r);
/* 8005CCDC */
void ae_DebugMenuInitCounters2(AeRam *r);
/* 8007D118 */
int32_t ae_ReadSizedValue(AeRam *r, uint32_t address, int32_t size);
/* 8007D1E0 */
int32_t ae_MenuItemInitBound(AeRam *r, uint32_t item, uint32_t variable, int32_t size);
/* 8007D29C */
void ae_MenuItemSetCallback(AeRam *r, uint32_t item, uint32_t callback);
/* 80088670 */
int32_t ae_ColliderZoneMasksUpdate(AeRam *r, const AeZonesHooks *h);
/* 800889EC */
int32_t ae_ColliderComputeQuadrantMask(AeRam *r, const AeZonesHooks *h, uint32_t collider);
/* 8013FC00, runtime overlay callback shim */
void ae_FUN_8013fc00(AeRam *r, const AeZonesHooks *h, int32_t collider_index);

#ifdef __cplusplus
}
#endif
#endif
