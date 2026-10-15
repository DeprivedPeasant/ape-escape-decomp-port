#ifndef AE_NATIVE_COLLISION_H
#define AE_NATIVE_COLLISION_H
#include <stdint.h>
#include "native_ground.h"
/* All coordinates retain the game's integer, negative-up coordinate system. */
typedef struct { int32_t height, extra, slope_x, slope_z; } AeShapeHit;
typedef struct AeCollision AeCollision;
typedef struct { int32_t height, kind, index, type, surface, slope_x, slope_z, extra_a, extra_b; } AeSolidHit;
AeCollision *ae_collision_load(const char *path);
void ae_collision_free(AeCollision *collision);
const char *ae_collision_error(void);
void ae_collision_info(const AeCollision *collision,uint32_t out[4]);
/* Read-only original collider record for host surface/volume adapters. */
uint16_t ae_collision_surface_type(const AeCollision *collision,const int32_t pos[3],int16_t *water_y);
int ae_collision_record(const AeCollision *collision,uint32_t index,int16_t out[28]);
/* Immutable little-endian Windows views, valid until collision is freed. */
void ae_collision_arrays(const AeCollision *collision,const int16_t **grid,const int16_t (**shapes)[10],const int16_t (**colliders)[28]);
/* Ray-test dispatch, including scratch-output aliasing. Movement uses resolve. */
int ae_collision_probe(int16_t record[28],uint32_t type,int collider,int32_t radius,int32_t height,const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_grid_height(const AeCollision *collision,int32_t x,int32_t y,int32_t z,
                             int32_t wall_mode,AeSolidHit *hit);
int ae_collision_solid_height(const AeCollision *collision,int32_t x,int32_t y,int32_t z,
                              int32_t wall_mode,int32_t ignore_collider,int32_t skip_water,AeSolidHit *hit);
int ae_collision_ground_height(const AeCollision *collision,const AeGround *ground,int32_t x,int32_t y,int32_t z,
                               int32_t wall_mode,int32_t ignore_collider,int32_t skip_water,AeGroundHit *hit,AeSolidHit *contact);
int ae_collision_shape(int32_t type, const int16_t half[3], const int32_t point[3],
                       int32_t wall_mode, AeShapeHit *hit);
int32_t ae_collision_sqrt(int32_t n, int32_t sdk_lookup);
/* Reconstructed box response handler. The original mutates the six s16 box words. */
int ae_collision_box_probe(int16_t box[6],int32_t type,int32_t radius,int32_t body_height,
                           const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_math_load(const char *original_exe);
int32_t ae_collision_atan(int32_t y,int32_t x);
void ae_collision_normalize(int32_t x,int32_t z,int32_t out[2]);
int ae_collision_collider(const int16_t record[28],const int32_t point[3],int32_t wall_mode,AeShapeHit *out);
int ae_collision_triangle_probe(const int16_t box[6],int32_t type,int32_t radius,int32_t body_height,
                                const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_radial_probe(const int16_t box[6],int32_t type,int32_t radius,int32_t body_height,
                              const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_ramp_probe(const int16_t box[6],int32_t type,int32_t radius,int32_t body_height,
                            const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_wall_band_probe(const int16_t box[6],int32_t type,int32_t radius,int32_t body_height,
                                 const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_ring_probe(const int16_t box[6],int32_t type,int32_t radius,int32_t body_height,
                            const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_strip_probe(const int16_t record[28],int32_t type,int32_t radius,int32_t body_height,
                             const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_curved_probe(const int16_t record[28],int32_t type,int32_t radius,int32_t body_height,
                              const int32_t point[3],const int32_t previous[3],int32_t out[6]);
int ae_collision_trench_probe(const int16_t record[28],int32_t type,int32_t radius,int32_t body_height,
                              const int32_t point[3],const int32_t previous[3],int32_t out[6]);
typedef struct { int32_t position[3],velocity[3],push_x,push_z,ceiling,contact; } AeMoveHit;
int ae_collision_response_supported(const AeCollision *collision,uint32_t out[2]);
int ae_collision_resolve(const AeCollision *collision,const int32_t position[3],const int32_t velocity[3],
                         int32_t radius,int32_t body_height,AeMoveHit *out);
#endif
