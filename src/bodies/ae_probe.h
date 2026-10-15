#ifndef AE_PROBE_H
#define AE_PROBE_H

/*
 * Collision body probe helpers translated from the original EXE onto AeRam.
 * Original addresses and the RAM globals / record offsets used by the routines
 * are named here so callers and the differential checker share one layout.
 */
#include "../core/ae_ram.h"
#include "../world/ae_world.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Probe working storage and collision-area globals. */
#define AE_G_COLL_PROBE_POINTS       0x800B20B0u /* pointer to eight 0x10-byte probe records */
#define AE_G_COLL_PROBE_FLAGS        0x800B20C0u /* pointer to one byte per collider */
#define AE_G_COLL_PROBE_HANDLERS     0x800B20E0u /* 16-entry probe-handler address table */
#define AE_G_COLL_CURRENT_GRID       0x800B8CF0u
#define AE_G_COLL_CURRENT_SHAPES     0x800B8CF4u
#define AE_G_COLL_SHAPES             0x800F4558u
#define AE_G_COLLIDERS               0x800F455Cu
#define AE_G_COLL_SHAPE_GRID         0x800F4560u
#define AE_G_COLLIDER_COUNT          0x800F45D4u
#define AE_G_COLL_GRID_WIDTH         0x800F45F0u
#define AE_G_COLL_GRID_DEPTH         0x800F45F4u
#define AE_G_COLL_HALF_X             0x800F45F8u
#define AE_G_COLL_HALF_Z             0x800F45FCu

/* CollisionBody fields. */
#define AE_PROBE_BODY_X              0x00u
#define AE_PROBE_BODY_Y              0x04u
#define AE_PROBE_BODY_Z              0x08u
#define AE_PROBE_BODY_VELOCITY_X     0x0Cu
#define AE_PROBE_BODY_VELOCITY_Y     0x10u
#define AE_PROBE_BODY_VELOCITY_Z     0x14u

/* Probe record: s16 position, s16 shape extents, selected shape pointer. */
#define AE_PROBE_RECORD_SIZE         0x10u
#define AE_PROBE_X                   0x00u
#define AE_PROBE_Y                   0x02u
#define AE_PROBE_Z                   0x04u
#define AE_PROBE_HALF_X              0x06u
#define AE_PROBE_HALF_Y              0x08u
#define AE_PROBE_HALF_Z              0x0Au
#define AE_PROBE_SHAPE               0x0Cu
/* Global-buffer offsets shifted by CollisionShiftProbePoints. */
#define AE_PROBE_SHIFT_Z_PROBE_1     0x14u
#define AE_PROBE_SHIFT_X_PROBE_2     0x20u
#define AE_PROBE_SHIFT_Z_PROBE_2     0x24u
#define AE_PROBE_SHIFT_X_PROBE_3     0x30u
#define AE_PROBE_SHIFT_Z_PROBE_4     0x44u
#define AE_PROBE_SHIFT_X_PROBE_5     0x50u
#define AE_PROBE_SHIFT_Z_PROBE_5     0x54u
#define AE_PROBE_SHIFT_X_PROBE_6     0x60u

/* Collision shape record (0x14 bytes, addressed through g_CollShapes). */
#define AE_PROBE_SHAPE_X_CELL        0x00u
#define AE_PROBE_SHAPE_Y_BAND        0x02u
#define AE_PROBE_SHAPE_Z_CELL        0x04u
#define AE_PROBE_SHAPE_HALF_X        0x06u
#define AE_PROBE_SHAPE_HALF_Y        0x08u
#define AE_PROBE_SHAPE_HALF_Z        0x0Au
#define AE_PROBE_SHAPE_NEXT          0x10u
#define AE_PROBE_SHAPE_SIZE          0x14u

/* Collider box fields used by PointInBoxXYZ and CollisionBuildProbePoints. */
#define AE_PROBE_COLLIDER_X          0x00u
#define AE_PROBE_COLLIDER_Y          0x02u
#define AE_PROBE_COLLIDER_Z          0x04u
#define AE_PROBE_COLLIDER_HALF_X     0x06u
#define AE_PROBE_COLLIDER_HALF_Y     0x08u
#define AE_PROBE_COLLIDER_HALF_Z     0x0Au
#define AE_PROBE_COLLIDER_TYPE       0x12u
#define AE_PROBE_COLLIDER_FLAGS      0x18u
/* Written by the body-type 1 case; the structs file has not named this field. */
#define AE_PROBE_COLLIDER_BODY_MARKER 0x32u
#define AE_PROBE_COLLIDER_SIZE       0x38u

/* 8007304C */
int32_t ae_CollisionBuildProbePoints(AeRam *r, int32_t body_type, uint32_t body);
/* 8004F6E8 */
uint32_t ae_CollisionPointInGrid(AeRam *r, uint32_t point);
/* 800733DC */
int32_t ae_CollisionShiftProbePoints(AeRam *r, int32_t dx, int32_t dz);
/* 80073AA0 */
int32_t ae_PointInBoxXYZ(AeRam *r, uint32_t box, uint32_t point);
/* 80072D28; GetGroundHeight (80043400) is called directly through ae_GetGroundHeight. */
int32_t ae_BodyProbeGround(AeRam *r, const AeWorldHooks *world, uint32_t body);

#ifdef __cplusplus
}
#endif
#endif
