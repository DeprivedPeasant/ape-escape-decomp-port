#ifndef AE_SOLID_H
#define AE_SOLID_H
#include "../core/ae_ram.h"

/* Collider is the original 0x38-byte RAM record. */
#define AE_COL_SIZE 0x38u
#define AE_COL_X 0x00u
#define AE_COL_Y 0x02u
#define AE_COL_Z 0x04u
#define AE_COL_EXTENT_X 0x06u
#define AE_COL_EXTENT_Y 0x08u
#define AE_COL_EXTENT_Z 0x0Au
#define AE_COL_TYPE 0x10u
#define AE_COL_SURFACE_TYPE 0x12u
#define AE_COL_MODE 0x14u
#define AE_COL_CONTACT_PARAMETER 0x16u
#define AE_COL_FLAGS 0x18u
#define AE_COL_ZONE_MASK 0x20u
#define AE_COL_INNER_RADIUS 0x24u
#define AE_COL_LOW_HEIGHT 0x26u
#define AE_COL_FLOW_X 0x2Au
#define AE_COL_FLOW_Y 0x2Cu
#define AE_COL_FLOW_Z 0x2Eu
#define AE_COL_SURFACE_PARAMETER 0x1Cu
#define AE_COL_STRIP_DIRECTION 0x2Cu
#define AE_COLL_PROBE_COUNT 0x800B2130u
#define AE_COLL_WALL_PROBE 0x800F45E6u
#define AE_COLL_SLOPE_X 0x800AF2ECu
#define AE_COLL_SLOPE_Z 0x800AF2F0u
#define AE_SHAPE_EXTENT_X 0u
#define AE_SHAPE_HEIGHT 2u
#define AE_SHAPE_EXTENT_Z 4u
#define AE_SHAPE_SIZE 0x14u
#define AE_SHAPE_BAND 2u
#define AE_SHAPE_PARAMETERS 6u
#define AE_SHAPE_CONTACT_MODE 0xCu
#define AE_SHAPE_CONTACT_PARAMETER 0xEu
#define AE_SHAPE_NEXT 0x10u
#define AE_SHAPE_TYPE 0x12u
#define AE_COLL_SHAPES 0x800F4558u
#define AE_COLL_GRID 0x800F4560u
#define AE_COLLIDERS 0x800F455Cu
#define AE_COLLIDER_COUNT 0x800F45D4u
#define AE_COLL_IGNORE 0x800F45E0u
#define AE_COLL_SURFACE_INDEX 0x800F45E4u
#define AE_COLL_SKIP_SURFACE 0x800F45E8u
#define AE_COLL_GRID_WIDTH 0x800F45F0u
#define AE_COLL_HALF_X 0x800F45F8u
#define AE_COLL_HALF_Z 0x800F45FCu
#define AE_COLL_CURRENT_SHAPES 0x800B8CF4u
#define AE_COLL_CURRENT_GRID 0x800B8CF0u
#define AE_COLL_CONTACT_PTR 0x800AF2E4u
#define AE_COLL_SURFACE_PTR 0x800AF2E8u
#define AE_COLL_SHAPE_HANDLERS 0x800AF180u
#define AE_COLL_SURFACE_SCRATCH 0x1F8003D4u

/* Only needed for original undefined caller values, never for external game logic.
 * NULL fails closed if a path actually consumes one of these values. */
typedef struct AeSolidHooks {
    int32_t stale_wall_axis;
    int16_t stale_edge_x, stale_edge_z;
    int32_t stale_initial_layer;
    int32_t stale_slope_corner_x, stale_slope_corner_z;
    int16_t stale_slope_angle_low, stale_slope_angle_high;
} AeSolidHooks;

int32_t ae_CollProbeCounterInc(AeRam *r);
int32_t ae_GetColliderQuadrantMaskXZ(AeRam *r, int32_t x, int32_t z);
int32_t ae_ColliderSlopeCorner(AeRam *r, uint32_t collider, uint32_t out);
int32_t ae_ColliderSlopeSide(AeRam *r, uint32_t collider, uint32_t point, int32_t radius, uint32_t angles);
int32_t ae_ColliderHalfSpaceInside(AeRam *r, uint32_t collider, uint32_t point);
int32_t ae_ColliderTypeActive(AeRam *r, uint32_t type, int32_t enabled);
int32_t ae_ColliderStripHeight(AeRam *r, uint32_t collider, uint32_t point);
int32_t ae_ColliderStripHeightOut(AeRam *r, uint32_t collider, uint32_t point, uint32_t out);
int32_t ae_CollShape_Box(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_Ramp2(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_Ramp4(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_Ramp45(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_Wedge(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_Cylinder(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollHalfPipeHeight(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_ColliderRingHeight(AeRam *r, uint32_t collider, uint32_t point, uint32_t out);
int32_t ae_DiagonalSignedDistance(AeRam *r, uint32_t type, uint32_t point, uint32_t center, uint32_t out);
int32_t ae_ColliderTrenchHeight(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_WallThin(AeRam *r, const AeSolidHooks *h, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_WallEdge(AeRam *r, const AeSolidHooks *h, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_WallDiagonal(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_CollShape_DiagonalBox(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out);
int32_t ae_ColliderSlopeHeight(AeRam *r, const AeSolidHooks *h, uint32_t collider, uint32_t point, uint32_t vector, uint32_t out);
int32_t ae_GetSurfaceType(AeRam *r, int32_t x, int32_t y, int32_t z);
int32_t ae_ColliderHeightAt(AeRam *r, const AeSolidHooks *h, uint32_t collider, int32_t x, int32_t y, int32_t z);
int32_t ae_GetSolidHeight(AeRam *r, const AeSolidHooks *h, int32_t x, int32_t y, int32_t z);
#endif
