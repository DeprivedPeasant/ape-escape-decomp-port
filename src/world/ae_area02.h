#ifndef AE_AREA02_H
#define AE_AREA02_H

#include "ae_zako1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Area 02 and shared EXE movement helpers used by the 18C enemy overlay. */
#define AE_AREA02_PATH_POINT_SIZE       0x08u
#define AE_AREA02_PATH_POINT_X         0x00u
#define AE_AREA02_PATH_POINT_Y         0x02u
#define AE_AREA02_PATH_POINT_Z         0x04u
#define AE_AREA02_SEEK_POINT_CLEAR_DIRECT 0x801FFEE0u
#define AE_AREA02_SEEK_POINT_CLEAR_PATH   0x801FFEC0u
#define AE_AREA02_SEEK_POINT_CLEAR_FOLLOW 0x801FFEA0u
#define AE_AREA02_SVECTOR_Y             0x02u
#define AE_AREA02_SVECTOR_Z             0x04u
#define AE_AREA02_STACK_WORD_SIZE       0x04u
#define AE_AREA02_HOOK_ENTRY_SIZE       0x04u
#define AE_AREA02_HOOK_TABLE_CAPACITY  2u

int32_t ae_HalfPipeRangeTest(AeRam *r, int32_t y, int32_t body_height,
                             int32_t lower_y, int32_t extent); /* 80050490 */

void ae_ObjMove(AeRam *r, uint32_t obj); /* 800554F0 */
int32_t ae_ObjSeekPoint(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                        int32_t x, int32_t y, int32_t z,
                        int32_t radius); /* 80055718 */
int32_t ae_ZakoHooksAdd(AeRam *r, uint32_t callback); /* 80057588 */
int32_t ae_ObjSeekPathPoint(AeRam *r, const AeZako1Hooks *h,
                            uint32_t obj, uint32_t point_index,
                            int32_t mode, int32_t radius); /* 80058400 */
void ae_ObjAdvancePath(AeRam *r, uint32_t obj,
                       int32_t reverse); /* 800584C8 */
int32_t ae_ObjFollowPath(AeRam *r, const AeZako1Hooks *h,
                         uint32_t obj, int32_t radius,
                         int32_t reverse, int32_t mode); /* 800585D0 */
void ae_ObjSetPosToPathPoint(AeRam *r, uint32_t obj,
                             int32_t point_index); /* 80058638 */
void ae_ObjFacePathPoint(AeRam *r, uint32_t obj,
                         int32_t point_index); /* 8005866C */

#ifdef __cplusplus
}
#endif
#endif
