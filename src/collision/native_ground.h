#ifndef AE_NATIVE_GROUND_H
#define AE_NATIVE_GROUND_H
#include <stdint.h>
typedef struct AeGround AeGround;
typedef struct { int32_t height, slope_x, slope_z, surface, selector; } AeGroundHit;
AeGround *ae_ground_load(const char *path);
void ae_ground_free(AeGround *ground);
const char *ae_ground_error(void);
void ae_ground_info(const AeGround *ground, uint32_t out[3]); /* flags, width, depth */
int ae_ground_entry(const AeGround *ground, uint32_t entry, int32_t out[9]);
int ae_ground_query(const AeGround *ground, int32_t x, int32_t y, int32_t z, AeGroundHit *out);
int32_t ae_ground_raw_surface(const AeGround *ground,int32_t x,int32_t z);
#endif
