#include "ae_area02.h"
#include "ae_zakohelpers.h"
#include "../camera/ae_camupd.h"
#include "../core/ae_sdk.h"

/* Named AeRam locations for ObjSeekPoint's two stack-local SVECTORs.  The
 * MIPS runner ignores the top-of-RAM stack window when comparing C and MIPS. */
#define AE_AREA02_SEEK_POINT_STACK_SOURCE 0x801FFEE0u
#define AE_AREA02_SEEK_POINT_STACK_DELTA  0x801FFED8u

static int32_t area02_s32(uint32_t bits) {
    int64_t value = bits;
    if ((bits & 0x80000000u) != 0u) value -= 0x100000000LL;
    return (int32_t)value;
}

static int32_t area02_s16(uint32_t bits) {
    uint32_t value = bits & 0xFFFFu;
    return (value & 0x8000u) != 0u ? (int32_t)value - 0x10000 : (int32_t)value;
}

static int32_t area02_s8(uint32_t bits) {
    uint32_t value = bits & 0xFFu;
    return (value & 0x80u) != 0u ? (int32_t)value - 0x100 : (int32_t)value;
}

static int32_t area02_add32(int32_t left, int32_t right) {
    return area02_s32((uint32_t)left + (uint32_t)right);
}

static int32_t area02_sub32(int32_t left, int32_t right) {
    return area02_s32((uint32_t)left - (uint32_t)right);
}

static int32_t area02_mullo32(int32_t left, int32_t right) {
    return area02_s32((uint32_t)left * (uint32_t)right);
}

static int32_t area02_asr32(int32_t value, unsigned shift) {
    if (shift == 0u) return value;
    uint32_t bits = (uint32_t)value;
    if (shift >= 32u) return value < 0 ? -1 : 0;
    uint32_t result = bits >> shift;
    if (value < 0) result |= UINT32_MAX << (32u - shift);
    return area02_s32(result);
}

static uint32_t area02_path_point(AeRam *r, uint32_t obj,
                                  int32_t point_index) {
    uint32_t shifted = (uint32_t)point_index << 16;
    int32_t byte_offset = area02_asr32(area02_s32(shifted), 13u);
    return ae_rd32(r, obj + AE_OBJ_PATH) + (uint32_t)byte_offset;
}

void ae_ObjMove(AeRam *r, uint32_t obj) {
    uint32_t position_x = ae_rd32(r, obj + AE_OBJ_POS_X);
    uint32_t position_y = ae_rd32(r, obj + AE_OBJ_POS_Y);
    uint32_t position_z = ae_rd32(r, obj + AE_OBJ_POS_Z);
    int32_t velocity_x = ae_rds16(r, obj + AE_OBJ_VELOCITY_X);
    int32_t velocity_y = ae_rds16(r, obj + AE_OBJ_VELOCITY_Y);
    int32_t velocity_z = ae_rds16(r, obj + AE_OBJ_VELOCITY_Z);

    ae_wr32(r, obj + AE_OBJ_POS_X, position_x + (uint32_t)velocity_x);
    ae_wr32(r, obj + AE_OBJ_POS_Y, position_y + (uint32_t)velocity_y);
    ae_wr32(r, obj + AE_OBJ_POS_Z, position_z + (uint32_t)velocity_z);
}

static int32_t area02_clear_seek_point_stack(AeRam *r,
                                             const AeZako1Hooks *h,
                                             uint32_t clear_address,
                                             int32_t target_z) {
    if (h == NULL || h->FUN_800A3740 == NULL) {
        ae_ram_fail(r, "ObjSeekPoint requires the FUN_800A3740 BIOS clear hook");
        return 0;
    }
    /* The original leaves target_z in $a3 when calling this three-argument
     * BIOS wrapper. Preserve that fourth argument word for hook observers. */
    (void)h->FUN_800A3740(h->user, r,
                          clear_address, 0u, 8u,
                          (uint32_t)target_z);
    return ae_ram_failed(r) == 0;
}

static int32_t area02_seek_point_at(AeRam *r, const AeZako1Hooks *h,
                                    uint32_t obj, int32_t x, int32_t y,
                                    int32_t z, int32_t radius,
                                    uint32_t clear_address) {
    if (!area02_clear_seek_point_stack(r, h, clear_address, z)) return 0;

    int32_t delta_x = area02_sub32(x, (int32_t)ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t delta_y = area02_sub32(y, (int32_t)ae_rd16(r, obj + AE_OBJ_POS_Y));
    int32_t delta_z = area02_sub32(z, (int32_t)ae_rd16(r, obj + AE_OBJ_POS_Z));
    ae_wr16(r, AE_AREA02_SEEK_POINT_STACK_SOURCE, (uint32_t)delta_x);
    ae_wr16(r, AE_AREA02_SEEK_POINT_STACK_SOURCE + AE_AREA02_SVECTOR_Y,
            (uint32_t)delta_y);
    ae_wr16(r, AE_AREA02_SEEK_POINT_STACK_SOURCE + AE_AREA02_SVECTOR_Z,
            (uint32_t)delta_z);

    /* The LWL/LWR + SWL/SWR pairs copy these two aligned words verbatim. */
    ae_wr32(r, AE_AREA02_SEEK_POINT_STACK_DELTA,
            ae_rd32(r, AE_AREA02_SEEK_POINT_STACK_SOURCE));
    ae_wr32(r, AE_AREA02_SEEK_POINT_STACK_DELTA + AE_AREA02_STACK_WORD_SIZE,
            ae_rd32(r, AE_AREA02_SEEK_POINT_STACK_SOURCE + AE_AREA02_STACK_WORD_SIZE));

    int32_t short_x = ae_rds16(r, AE_AREA02_SEEK_POINT_STACK_DELTA);
    int32_t short_y = ae_rds16(r, AE_AREA02_SEEK_POINT_STACK_DELTA + AE_AREA02_SVECTOR_Y);
    int32_t short_z = ae_rds16(r, AE_AREA02_SEEK_POINT_STACK_DELTA + AE_AREA02_SVECTOR_Z);
    int32_t distance_squared = area02_add32(
        area02_add32(area02_mullo32(short_x, short_x),
                     area02_mullo32(short_y, short_y)),
        area02_mullo32(short_z, short_z));
    int32_t radius_squared = area02_mullo32(radius, radius);

    if (distance_squared < radius_squared) {
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X,
                ae_rd16(r, AE_AREA02_SEEK_POINT_STACK_DELTA));
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
                ae_rd16(r, AE_AREA02_SEEK_POINT_STACK_DELTA + AE_AREA02_SVECTOR_Y));
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z,
                ae_rd16(r, AE_AREA02_SEEK_POINT_STACK_DELTA + AE_AREA02_SVECTOR_Z));

        if (short_x != 0 || short_z != 0) {
            ae_ObjTurnTowardsXZ(r, h, obj, short_x, short_z, 0,
                                ae_rds16(r, obj + AE_OBJ_UNK_B0));
        }
        return 1;
    }

    ae_ObjSetVelAlongDir(r, h, obj, AE_AREA02_SEEK_POINT_STACK_DELTA,
                         radius, 0);
    return 0;
}

int32_t ae_ObjSeekPoint(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                        int32_t x, int32_t y, int32_t z,
                        int32_t radius) {
    return area02_seek_point_at(r, h, obj, x, y, z, radius,
                                AE_AREA02_SEEK_POINT_CLEAR_DIRECT);
}

int32_t ae_ZakoHooksAdd(AeRam *r, uint32_t callback) {
    uint32_t count = ae_rd8(r, AE_G_ZAKO_HOOK_COUNT);
    if (count >= AE_AREA02_HOOK_TABLE_CAPACITY) return 0;

    for (int32_t index = (int32_t)AE_AREA02_HOOK_TABLE_CAPACITY - 1;
         index >= 0; --index) {
        uint32_t entry = AE_G_ZAKO_HOOKS +
                         (uint32_t)index * AE_AREA02_HOOK_ENTRY_SIZE;
        if (ae_rd32(r, entry) == callback) return 2;
    }

    ae_wr32(r, AE_G_ZAKO_HOOKS + count * AE_AREA02_HOOK_ENTRY_SIZE,
            callback);
    ae_wr8(r, AE_G_ZAKO_HOOK_COUNT, count + 1u);
    return 1;
}

static int32_t area02_seek_path_point_at(AeRam *r, const AeZako1Hooks *h,
                                         uint32_t obj,
                                         uint32_t point_index,
                                         int32_t mode, int32_t radius,
                                         uint32_t clear_address) {
    uint32_t index = point_index & 0xFFu;
    uint32_t point = ae_rd32(r, obj + AE_OBJ_PATH) +
                     index * AE_AREA02_PATH_POINT_SIZE;
    int32_t point_mode = area02_s8((uint32_t)mode);
    int32_t x = ae_rds16(r, point + AE_AREA02_PATH_POINT_X);
    int32_t y = point_mode == 1
        ? ae_rds16(r, point + AE_AREA02_PATH_POINT_Y)
        : ae_rds16(r, obj + AE_OBJ_POS_Y);
    int32_t z = ae_rds16(r, point + AE_AREA02_PATH_POINT_Z);
    return area02_s16((uint32_t)area02_seek_point_at(
        r, h, obj, x, y, z, area02_s16((uint32_t)radius), clear_address));
}

int32_t ae_ObjSeekPathPoint(AeRam *r, const AeZako1Hooks *h,
                            uint32_t obj, uint32_t point_index,
                            int32_t mode, int32_t radius) {
    return area02_seek_path_point_at(r, h, obj, point_index, mode, radius,
                                     AE_AREA02_SEEK_POINT_CLEAR_PATH);
}

void ae_ObjAdvancePath(AeRam *r, uint32_t obj, int32_t reverse) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    uint32_t count = ae_rd16(r, obj + AE_OBJ_PATH_POINT);
    uint32_t index = ae_rd8(r, obj + AE_OBJ_PATH_INDEX);

    if ((reverse & 0xFF) == 0) {
        if ((flags & AE_OBJ_FLAG_UNK_6) != 0u) {
            if (count == 0u) {
                ae_ram_fail(r, "ObjAdvancePath signed division by zero");
                return;
            }
            int32_t numerator = area02_add32((int32_t)index,
                                              (int32_t)count - 1);
            ae_wr8(r, obj + AE_OBJ_PATH_INDEX,
                   (uint32_t)(numerator % (int32_t)count));
        } else {
            uint32_t next_index = index + 1u;
            if (count == 0u) {
                ae_ram_fail(r, "ObjAdvancePath signed division by zero");
                return;
            }
            ae_wr8(r, obj + AE_OBJ_PATH_INDEX, next_index % count);
        }
        return;
    }

    if ((flags & AE_OBJ_FLAG_UNK_6) != 0u) {
        if (index != 0u) {
            ae_wr8(r, obj + AE_OBJ_PATH_INDEX, index - 1u);
        } else {
            ae_wr32(r, obj + AE_OBJ_FLAGS, flags & ~AE_OBJ_FLAG_UNK_6);
            ae_wr8(r, obj + AE_OBJ_PATH_INDEX, 1u);
        }
        return;
    }

    uint32_t endpoint_count = ae_rd16(r, obj + AE_OBJ_PATH_POINT);
    uint32_t next_index = index + 1u;
    ae_wr8(r, obj + AE_OBJ_PATH_INDEX, next_index);
    if ((next_index & 0xFFu) == endpoint_count) {
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags | AE_OBJ_FLAG_UNK_6);
        ae_wr8(r, obj + AE_OBJ_PATH_INDEX,
               ae_rd8(r, obj + AE_OBJ_PATH_POINT) - 2u);
    }
}

int32_t ae_ObjFollowPath(AeRam *r, const AeZako1Hooks *h,
                         uint32_t obj, int32_t radius,
                         int32_t reverse, int32_t mode) {
    uint32_t point_index = ae_rd8(r, obj + AE_OBJ_PATH_INDEX);
    int32_t reached = area02_seek_path_point_at(
        r, h, obj, point_index, area02_s8((uint32_t)mode),
        area02_s16((uint32_t)radius), AE_AREA02_SEEK_POINT_CLEAR_FOLLOW);
    if (ae_ram_failed(r)) return 0;
    if (area02_s16((uint32_t)reached) == 0) return 0;

    ae_ObjAdvancePath(r, obj, area02_s8((uint32_t)reverse));
    return 1;
}

void ae_ObjSetPosToPathPoint(AeRam *r, uint32_t obj,
                             int32_t point_index) {
    uint32_t point = area02_path_point(r, obj, point_index);
    ae_wr32(r, obj + AE_OBJ_POS_X,
            (uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_X));
    ae_wr32(r, obj + AE_OBJ_POS_Y,
            (uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_Y));
    ae_wr32(r, obj + AE_OBJ_POS_Z,
            (uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_Z));
}

void ae_ObjFacePathPoint(AeRam *r, uint32_t obj,
                         int32_t point_index) {
    uint32_t point = area02_path_point(r, obj, point_index);
    int32_t delta_x = area02_sub32(
        (int32_t)ae_rd16(r, point + AE_AREA02_PATH_POINT_X),
        (int32_t)ae_rd16(r, obj + AE_OBJ_POS_X));
    int32_t delta_z = area02_sub32(
        (int32_t)ae_rd16(r, point + AE_AREA02_PATH_POINT_Z),
        (int32_t)ae_rd16(r, obj + AE_OBJ_POS_Z));
    ae_wr16(r, obj + AE_OBJ_FACING_X, (uint32_t)delta_x);
    ae_wr16(r, obj + AE_OBJ_FACING_Y, 0u);
    ae_wr16(r, obj + AE_OBJ_FACING_Z, (uint32_t)delta_z);
    (void)ae_sdk_VectorNormalSS(r, obj + AE_OBJ_FACING_X,
                                obj + AE_OBJ_FACING_X);
    if (ae_ram_failed(r)) return;

    int32_t angle = ae_Atan2Safe(r,
        ae_rds16(r, obj + AE_OBJ_FACING_X),
        ae_rds16(r, obj + AE_OBJ_FACING_Z));
    int32_t unwrapped = area02_add32(angle, 4096);
    int32_t rounded = unwrapped < 0
        ? area02_add32(angle, 8191) : unwrapped;
    int32_t quotient = area02_asr32(rounded, 12u);
    int32_t heading = area02_sub32(unwrapped,
        area02_s32((uint32_t)quotient << 12));
    ae_wr16(r, obj + AE_OBJ_HEADING, (uint32_t)heading);
}
