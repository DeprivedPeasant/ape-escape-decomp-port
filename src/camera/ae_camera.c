/* Readable RAM-image ports of the gameplay camera routines.  Original entry
 * addresses are recorded beside each function; offsets and wrapped arithmetic
 * are named so the code follows the game's data layout directly. */
#include "ae_camera.h"

#include <limits.h>
#include <stddef.h>

enum {
    AE_G_CAMERA_INPUT_EYE = 0x800F45A8u,
    AE_G_CAMERA_INPUT_TARGET = 0x800F45B0u,
    AE_G_CAMERA_SCRIPT_POINTER = 0x800E691Cu,
    AE_G_CAMERA_REGION_CURRENT = 0x800E68EBu,
    AE_G_PLAYER_SCRATCH_RIGHT = 0x800E5498u
};

static int32_t s32_bits(uint32_t value)
{
    if (value & 0x80000000u)
        return (int32_t)(value & 0x7FFFFFFFu) - 0x7FFFFFFF - 1;
    return (int32_t)value;
}

static int32_t s16_bits(uint32_t value)
{
    value &= 0xFFFFu;
    return value >= 0x8000u ? (int32_t)value - 0x10000 : (int32_t)value;
}

static int32_t s8_bits(uint32_t value)
{
    value &= 0xFFu;
    return value >= 0x80u ? (int32_t)value - 0x100 : (int32_t)value;
}

static int32_t add32(int32_t a, int32_t b)
{
    return s32_bits((uint32_t)a + (uint32_t)b);
}

static int32_t sub32(int32_t a, int32_t b)
{
    return s32_bits((uint32_t)a - (uint32_t)b);
}

static int32_t mul32(int32_t a, int32_t b)
{
    return s32_bits((uint32_t)a * (uint32_t)b);
}

static int32_t neg32(int32_t value)
{
    return s32_bits(0u - (uint32_t)value);
}

static int32_t floor_shr(int32_t value, unsigned shift)
{
    if (value >= 0)
        return (int32_t)((uint32_t)value >> shift);
    return -1 - (int32_t)((uint32_t)(-1 - value) >> shift);
}

static int32_t mod4096(int32_t value)
{
    int32_t adjusted = value < 0 ? add32(value, 0xFFF) : value;
    return sub32(value, mul32(floor_shr(adjusted, 12), 0x1000));
}

static int32_t trunc_shr(int32_t value, unsigned shift)
{
    if (value < 0)
        value = add32(value, (int32_t)((1u << shift) - 1u));
    return floor_shr(value, shift);
}

static int32_t mulhi_s32(int32_t a, int32_t b)
{
    int64_t product = (int64_t)a * (int64_t)b;
    int64_t high;
    if (product >= 0)
        high = product / 4294967296LL;
    else
        high = -1 - ((-1 - product) / 4294967296LL);
    return (int32_t)high;
}

static int32_t trunc_div3(int32_t value)
{
    int32_t high = mulhi_s32(value, 0x55555556);
    return sub32(high, value < 0 ? -1 : 0);
}

static int32_t cam_div(AeRam *r, int32_t numerator, int32_t denominator)
{
    if (denominator == 0 ||
        (denominator == -1 && numerator == INT32_MIN)) {
        ae_ram_fail(r, "Camera signed division trap (%d / %d)", numerator, denominator);
        return 0;
    }
    return numerator / denominator;
}

static uint32_t slot_addr(AeRam *r, uint32_t slot, uint32_t offset)
{
    (void)r;
    return slot + offset;
}

static void copy_ram(AeRam *r, uint32_t dst, uint32_t src, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++)
        ae_wr8(r, dst + i, ae_rd8(r, src + i));
}

static uint32_t camera_slots(AeRam *r)
{
    return ae_rd32(r, AE_G_CAMERA_SLOTS);
}

static uint32_t camera_slot_by_index(AeRam *r, uint32_t index)
{
    return camera_slots(r) + index * AE_CAMERA_SLOT_SIZE;
}

static int32_t call_camera(AeRam *r, const AeCameraHooks *h, AeCameraHook fn,
                           const char *name, uint32_t a0, uint32_t a1,
                           uint32_t a2, uint32_t a3)
{
    if (h == NULL || fn == NULL) {
        ae_ram_fail(r, "camera hook %s was not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

static int32_t call_atan(AeRam *r, const AeCameraHooks *h, int32_t x, int32_t z)
{
    return call_camera(r, h, h ? h->Atan2Safe : NULL, "Atan2Safe",
                       (uint32_t)x, (uint32_t)z, 0, 0);
}

static void call_normalize(AeRam *r, const AeCameraHooks *h, uint32_t vector)
{
    (void)call_camera(r, h, h ? h->VectorNormalSS : NULL, "VectorNormalSS",
                      vector, vector, 0, 0);
}

static int32_t call_ground(AeRam *r, const AeCameraHooks *h,
                           int32_t x, int32_t y, int32_t z)
{
    return call_camera(r, h, h ? h->GetGroundHeight : NULL, "GetGroundHeight",
                       (uint32_t)x, (uint32_t)y, (uint32_t)z, 0);
}

static void call_memset(AeRam *r, const AeCameraHooks *h,
                        uint32_t dst, uint32_t value, uint32_t count)
{
    (void)call_camera(r, h, h ? h->memset : NULL, "memset",
                      dst, value, count, 0);
}

static int32_t scratch_s16(AeRam *r, uint32_t offset)
{
    return ae_rds16(r, AE_SCRATCH_BASE + offset);
}

static int32_t slot_s16(AeRam *r, uint32_t slot, uint32_t offset)
{
    return ae_rds16(r, slot_addr(r, slot, offset));
}

static int32_t slot_s32(AeRam *r, uint32_t slot, uint32_t offset)
{
    return ae_rds32(r, slot_addr(r, slot, offset));
}

static void slot_set_s16(AeRam *r, uint32_t slot, uint32_t offset, int32_t value)
{
    ae_wr16(r, slot_addr(r, slot, offset), (uint32_t)value);
}

static void slot_set_s32(AeRam *r, uint32_t slot, uint32_t offset, int32_t value)
{
    ae_wr32(r, slot_addr(r, slot, offset), (uint32_t)value);
}

static int32_t camera_sqrt(AeRam *r, const AeCameraHooks *h, int32_t value)
{
    return call_camera(r, h, h ? h->SquareRoot0 : NULL, "SquareRoot0",
                       (uint32_t)value, 0, 0, 0);
}

static int32_t camera_cos(AeRam *r, const AeCameraHooks *h, int32_t angle)
{
    return call_camera(r, h, h ? h->rcos : NULL, "rcos",
                       (uint32_t)angle, 0, 0, 0);
}

static int32_t camera_sin(AeRam *r, const AeCameraHooks *h, int32_t angle)
{
    return call_camera(r, h, h ? h->rsin : NULL, "rsin",
                       (uint32_t)angle, 0, 0, 0);
}

static int32_t ground_s16(AeRam *r, const AeCameraHooks *h,
                          int32_t x, int32_t y, int32_t z)
{
    return s16_bits((uint32_t)call_ground(r, h, x, y, z));
}

static void saturating_increment(AeRam *r, uint32_t address)
{
    uint32_t value = ae_rd8(r, address);
    if (value != 0xFFu)
        ae_wr8(r, address, value + 1u);
}

/* 80046970 */
void ae_CameraKindFixed_Nop(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    (void)r;
    (void)h;
    (void)slot;
}

/* 8004C2E4 */
void ae_CameraSlotClearScript(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    (void)h;
    ae_wr8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE + 1u), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE + 2u), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_SCRIPT_ACTIVE), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_SCRIPT_STEP), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_SCRIPT_TIMER), 0);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_SCRIPT_POINTER), 0);
    ae_wr8(r, AE_G_CAMERA_SCRIPT_CLEAR, 0);
    ae_wr8(r, AE_G_CAMERA_SCRIPT_ACTIVE, 0);
}

/* 800493D4 */
void ae_CameraSlotResetFollow(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t player = AE_G_PLAYER;
    int32_t facing_x = ae_rds16(r, player + AE_PLAYER_FACING);
    int32_t facing_z = ae_rds16(r, player + AE_PLAYER_FACING + 4u);
    int32_t yaw = call_atan(r, h, neg32(facing_x), neg32(facing_z));
    int32_t pos_x = ae_rds32(r, player + AE_PLAYER_POSITION);
    int32_t pos_y = ae_rds32(r, player + AE_PLAYER_POSITION + 4u);
    int32_t pos_z = ae_rds32(r, player + AE_PLAYER_POSITION + 8u);

    ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW), (uint32_t)yaw);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_DISTANCE), 0x460u);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_Y), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_FOLLOW_FLAG_A), 0);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_FOLLOW_FLAG_B), 1);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_SAVED_YAW), (uint32_t)yaw);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS), (uint32_t)pos_x);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 2u), (uint32_t)pos_y);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 4u), (uint32_t)pos_z);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE_VELOCITY), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE_VELOCITY + 2u), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE_VELOCITY + 4u), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET_VELOCITY), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET_VELOCITY + 2u), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET_VELOCITY + 4u), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE), (uint32_t)pos_x - (uint32_t)floor_shr(facing_x, 1));
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE + 2u), (uint32_t)pos_y - 0x172u);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE + 4u), (uint32_t)pos_z - (uint32_t)floor_shr(facing_z, 1));
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET), (uint32_t)pos_x + (uint32_t)floor_shr(facing_x, 2));
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET + 2u), (uint32_t)pos_y - 0x6Eu);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET + 4u), (uint32_t)pos_z + (uint32_t)floor_shr(facing_z, 2));
    ae_wr8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE), 1);
}

/* 80047104 */
void ae_CameraSlotInitFollow(AeRam *r, const AeCameraHooks *h,
                             uint32_t slot, int32_t kind)
{
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE_HEIGHT), (uint32_t)-370);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET_HEIGHT), (uint32_t)-110);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_FOLLOW_DIST), 0x460u);
    copy_ram(r, slot_addr(r, slot, AE_CAM_EYE), AE_G_CAMERA_EYE, 8);
    copy_ram(r, slot_addr(r, slot, AE_CAM_TARGET), AE_G_CAMERA_TARGET, 8);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_DISTANCE), 0x460u);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_UNK_2C), 0);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_X), 0);
    if (kind == 1 || kind == 3)
        ae_CameraSlotResetFollow(r, h, slot);
    else if (kind == 4 || kind == 5)
        ae_CameraSlotClearScript(r, h, slot);
}

/* 800471E4 */
int32_t ae_CameraSlotCreate(AeRam *r, const AeCameraHooks *h,
                            int32_t kind, uint32_t eye)
{
    uint32_t slots = camera_slots(r);
    uint32_t index;
    for (index = 0; index < AE_CAMERA_SLOT_COUNT; index++) {
        uint32_t slot = slots + index * AE_CAMERA_SLOT_SIZE;
        if (ae_rd8(r, slot_addr(r, slot, AE_CAM_KIND)) == 0) {
            ae_wr8(r, slot_addr(r, slot, AE_CAM_KIND), (uint32_t)kind);
            copy_ram(r, slot_addr(r, slot, AE_CAM_EYE), eye, 8);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_LINKED_SLOT), index);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE + 3u), 0xFFu);
            ae_CameraSlotInitFollow(r, h, slot, kind);
            ae_wr16(r, AE_G_CAMERA_STATE + 2u,
                    ae_rd16(r, AE_G_CAMERA_STATE + 2u) + 1u);
            return (int32_t)index;
        }
    }
    return 0;
}

/* 80047290 */
void ae_CameraSlotRemove(AeRam *r, const AeCameraHooks *h, uint32_t index)
{
    uint32_t slot = camera_slot_by_index(r, index);
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t region_flag;
    uint32_t kind;
    (void)h;

    if (ae_rd8(r, state + AE_CAM_G_REMOVE_GUARD) != 0)
        ae_wr8(r, state + AE_CAM_G_REMOVE_GUARD, 0);
    region_flag = ae_rd8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE + 3u));
    if (region_flag != 0xFFu) {
        uint32_t region = ae_rd32(r, state + AE_CAM_G_REGION_PTR) +
                          region_flag * AE_CAMERA_REGION_STRIDE;
        ae_wr16(r, region + AE_CAM_REGION_FLAGS,
                ae_rd16(r, region + AE_CAM_REGION_FLAGS) & 0xFFF0u);
    }
    ae_wr16(r, state + AE_CAM_G_SLOT_COUNT,
            ae_rd16(r, state + AE_CAM_G_SLOT_COUNT) - 1u);
    kind = ae_rd8(r, slot_addr(r, slot, AE_CAM_KIND));
    if ((kind == 4u || kind == 5u) &&
        ae_rd8(r, state + AE_CAM_G_REGION_SLOT) == (uint8_t)index) {
        ae_wr8(r, state + AE_CAM_G_REGION_SLOT, 0xFFu);
        ae_wr8(r, state + AE_CAM_G_REGION_DELAY, 0);
        ae_wr8(r, state + AE_CAM_G_REGION_OLD, 0xFFu);
    }
    if (ae_rd16(r, state + AE_CAM_G_PRIOR_SLOT) == (uint16_t)index)
        ae_wr8(r, state + AE_CAM_G_ACTIVE_INDEX, (uint8_t)index);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_KIND), 0);
}

/* 80047360 */
void ae_CameraSlotAimAtFocus(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t input = AE_CAMERA_TEMP_AIM;
    uint32_t output = AE_CAMERA_TEMP_VECTOR;
    int32_t vx, vz, dx, dz;
    call_memset(r, h, input, 0, 8);
    ae_wr16(r, input, ae_rd16(r, AE_SCRATCH_BASE + AE_CAM_SCR_PLAYER_X) -
                      ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE)));
    ae_wr16(r, input + 4u, ae_rd16(r, AE_SCRATCH_BASE + AE_CAM_SCR_PLAYER_Z) -
                          ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 4u)));
    copy_ram(r, output, input, 8);
    call_normalize(r, h, output);
    vx = ae_rds16(r, output);
    vz = ae_rds16(r, output + 4u);
    dx = ae_rds16(r, AE_SCRATCH_BASE + AE_CAM_SCR_DIRECTION);
    dz = ae_rds16(r, AE_SCRATCH_BASE + AE_CAM_SCR_DIRECTION + 4u);
    ae_wr32(r, AE_SCRATCH_BASE + AE_CAM_SCR_AIM_DOT,
            (uint32_t)trunc_shr(neg32(add32(mul32(dx, vx), mul32(dz, vz))), 12));
    ae_wr32(r, AE_SCRATCH_BASE + AE_CAM_SCR_AIM_CROSS,
            (uint32_t)trunc_shr(sub32(mul32(dx, vz), mul32(dz, vx)), 12));
}

static int smooth_axis(AeRam *r, uint32_t slot, uint32_t field,
                       uint32_t desired, int32_t divisor)
{
    int32_t current = ae_rds16(r, slot_addr(r, slot, field));
    int32_t sum = add32(mul32(current, sub32(divisor, 1)),
                        ae_rds16(r, AE_SCRATCH_BASE + desired));
    int32_t result;
    if (divisor == 0 || (divisor == -1 && sum == INT32_MIN)) {
        (void)cam_div(r, sum, divisor);
        return 0;
    }
    result = cam_div(r, sum, divisor);
    ae_wr16(r, slot_addr(r, slot, field), (uint32_t)result);
    return 1;
}

/* 80047464 */
void ae_CameraSlotSmoothEye(AeRam *r, const AeCameraHooks *h, uint32_t slot,
                            int32_t dx, int32_t dy, int32_t dz)
{
    (void)h;
    if (!smooth_axis(r, slot, AE_CAM_EYE, AE_CAM_SCR_DESIRED_EYE, dx)) return;
    if (!smooth_axis(r, slot, AE_CAM_EYE + 2u, AE_CAM_SCR_DESIRED_EYE + 2u, dy)) return;
    (void)smooth_axis(r, slot, AE_CAM_EYE + 4u, AE_CAM_SCR_DESIRED_EYE + 4u, dz);
}

/* 80047558 */
void ae_CameraSlotSmoothTarget(AeRam *r, const AeCameraHooks *h, uint32_t slot,
                               int32_t dx, int32_t dy, int32_t dz)
{
    (void)h;
    if (!smooth_axis(r, slot, AE_CAM_TARGET, AE_CAM_SCR_DESIRED_TGT, dx)) return;
    if (!smooth_axis(r, slot, AE_CAM_TARGET + 2u, AE_CAM_SCR_DESIRED_TGT + 2u, dy)) return;
    (void)smooth_axis(r, slot, AE_CAM_TARGET + 4u, AE_CAM_SCR_DESIRED_TGT + 4u, dz);
}

/* 80047654 */
void ae_CameraKindAim_SnapToFocus(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    ae_CameraSlotAimAtFocus(r, h, slot);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER), 0x18u);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_HEIGHT_A), ae_rd32(r, slot_addr(r, slot, AE_CAM_YAW)));
    ae_wr32(r, slot_addr(r, slot, AE_CAM_HEIGHT_B), ae_rd32(r, slot_addr(r, slot, AE_CAM_TARGET_YAW)));
}

/* 8004AA24 */
void ae_CameraSlotFlagBit81(AeRam *r, const AeCameraHooks *h)
{
    uint32_t follow_index = ae_rd8(r, AE_G_CAMERA_STATE + AE_CAM_G_FOLLOW_INDEX);
    uint32_t slot = camera_slot_by_index(r, follow_index);
    (void)h;
    ae_wr8(r, slot_addr(r, slot, AE_CAM_FOLLOW_FLAG_B), 1);
}

/* 80048D10 */
void ae_CameraSlotPlaceBehindPlayer(AeRam *r, const AeCameraHooks *h,
                                    uint32_t slot, uint32_t position, int32_t distance)
{
    uint32_t player = AE_G_PLAYER;
    int32_t facing_x = ae_rds16(r, player + AE_PLAYER_FACING);
    int32_t facing_z = ae_rds16(r, player + AE_PLAYER_FACING + 4u);
    int32_t px = ae_rd16(r, position);
    int32_t py = ae_rd16(r, position + 2u);
    int32_t pz = ae_rd16(r, position + 4u);
    int32_t offset_x = floor_shr(mul32(facing_x, distance), 12);
    int32_t offset_z = floor_shr(mul32(facing_z, distance), 12);
    int32_t yaw;
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET), (uint32_t)px);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET + 2u),
            (uint32_t)py + ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET_HEIGHT)));
    ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET + 4u), (uint32_t)pz);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE), (uint32_t)px - (uint32_t)offset_x);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE + 2u),
            (uint32_t)py + ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE_HEIGHT)));
    ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE + 4u), (uint32_t)pz - (uint32_t)offset_z);
    yaw = call_atan(r, h, neg32(facing_x), neg32(facing_z));
    ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW), (uint32_t)yaw);
}

/* 80048DD0 */
void ae_CameraSlotUpdateYaw(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t scratch = AE_SCRATCH_BASE;
    uint32_t state = AE_G_CAMERA_STATE;
    int32_t desired_yaw = call_atan(r, h,
        neg32(ae_rds16(r, scratch + AE_CAM_SCR_DIRECTION)),
        neg32(ae_rds16(r, scratch + AE_CAM_SCR_DIRECTION + 4u)));
    int32_t yaw = ae_rds32(r, slot_addr(r, slot, AE_CAM_YAW));
    int32_t delta = sub32(yaw, desired_yaw);
    int32_t base_delta = add32(delta, 0x1000);
    int32_t quotient_source = base_delta >= 0 ? base_delta : add32(delta, 0x1FFF);
    int32_t rem;
    uint32_t stick = ae_rd32(r, AE_G_RIGHT_STICK_MAG);
    uint32_t i;

    rem = sub32(base_delta, mul32(floor_shr(quotient_source, 12), 0x1000));
    ae_wr32(r, slot_addr(r, slot, AE_CAM_TARGET_YAW), (uint32_t)desired_yaw);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW_DELTA), (uint32_t)rem);
    if (rem >= 0x801)
        ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW_DELTA), (uint32_t)(rem - 0x1000));
    ae_CameraSlotAimAtFocus(r, h, slot);
    if (ae_rd16(r, AE_G_RIGHT_STICK_MAG) < 0x65u)
        ae_wr8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD), 0);
    else {
        uint32_t value = ae_rd8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD));
        if (value != 0xFFu)
            value++;
        ae_wr8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD), value);
    }
    if (stick == 0 &&
        ae_rds16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS)) == ae_rds16(r, scratch + AE_CAM_SCR_PLAYER_X) &&
        ae_rds16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 2u)) == ae_rds16(r, scratch + AE_CAM_SCR_PLAYER_Y) &&
        ae_rds16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 4u)) == ae_rds16(r, scratch + AE_CAM_SCR_PLAYER_Z)) {
        uint32_t value = ae_rd8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES));
        if (value != 0xFFu)
            value++;
        ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), value);
    } else {
        ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);
    }
    {
        int32_t side = sub32(mul32(ae_rds16(r, scratch + AE_CAM_SCR_DIRECTION + 4u),
                                   ae_rds16(r, slot_addr(r, slot, AE_CAM_DIRECTION_X))),
                             mul32(ae_rds16(r, scratch + AE_CAM_SCR_DIRECTION),
                                   ae_rds16(r, slot_addr(r, slot, AE_CAM_DIRECTION_Z))));
        ae_wr32(r, slot_addr(r, slot, AE_CAM_SIDE), (uint32_t)floor_shr(side, 12));
    }
    for (i = 0; i < 8; i++)
        ae_wr8(r, slot_addr(r, slot, AE_CAM_DIRECTION_X) + i,
               ae_rd8(r, scratch + AE_CAM_SCR_DIRECTION + i));
    (void)state;
}

/* 80048F6C */
void ae_CameraSlotGroundClamp(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t vector = AE_CAMERA_TEMP_VECTOR;
    uint32_t scratch = AE_SCRATCH_BASE;
    int32_t eye_x = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE));
    int32_t eye_y = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE + 2u));
    int32_t eye_z = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE + 4u));
    int32_t focus_x = ae_rds16(r, scratch + AE_CAM_SCR_FOCUS);
    int32_t focus_y = ae_rds16(r, scratch + AE_CAM_SCR_FOCUS + 2u);
    int32_t focus_z = ae_rds16(r, scratch + AE_CAM_SCR_FOCUS + 4u);
    int32_t ground;
    int32_t normal_x, normal_z;

    ae_wr16(r, vector, (uint32_t)focus_x - (uint32_t)eye_x);
    ae_wr16(r, vector + 2u, (uint32_t)eye_y);
    ae_wr16(r, vector + 4u, (uint32_t)focus_z - (uint32_t)eye_z);
    call_normalize(r, h, vector);
    normal_x = ae_rds16(r, vector);
    normal_z = ae_rds16(r, vector + 4u);
    ground = s16_bits((uint32_t)call_ground(r, h,
        add32(eye_x, floor_shr(normal_x, 5)), eye_y,
        add32(eye_z, floor_shr(normal_z, 5))));
    if (ground - 0x80 < eye_y && eye_y < ground + 0x110) {
        if (focus_y < ground + 0x100)
            ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE + 2u), (uint32_t)(ground - 0x80));
        if (ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE + 2u)) < focus_y - 0x400)
            ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE + 2u), (uint32_t)(focus_y - 0x400));
    }
}

/* 8004906C */
void ae_CameraSlotGroundClampNear(AeRam *r, const AeCameraHooks *h,
                                  uint32_t slot, int32_t near)
{
    uint32_t vector = AE_CAMERA_TEMP_FOCUS;
    uint32_t scratch = AE_SCRATCH_BASE;
    int32_t eye_x = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE));
    int32_t eye_y = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE + 2u));
    int32_t eye_z = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE + 4u));
    int32_t desired_x = ae_rds16(r, scratch + AE_CAM_SCR_DESIRED_EYE);
    int32_t desired_y = ae_rds16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u);
    int32_t desired_z = ae_rds16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u);
    int32_t len_sq, root, ground_a, ground_b;

    ae_wr16(r, vector, (uint32_t)desired_x - (uint32_t)eye_x);
    ae_wr16(r, vector + 2u, 0);
    ae_wr16(r, vector + 4u, (uint32_t)desired_z - (uint32_t)eye_z);
    len_sq = call_camera(r, h, h ? h->VectorNormalSS : NULL, "VectorNormalSS",
                         vector, vector, 0, 0);
    root = call_camera(r, h, h ? h->SquareRoot0 : NULL, "SquareRoot0",
                        (uint32_t)len_sq, 0, 0, 0);
    if (root >= 0x41)
        root = 0x40;
    ground_a = s16_bits((uint32_t)call_ground(r, h,
        add32(eye_x, floor_shr(mul32(ae_rds16(r, vector), root), 10)), eye_y,
        add32(eye_z, floor_shr(mul32(ae_rds16(r, vector + 4u), root), 10))));
    ground_b = s16_bits((uint32_t)call_ground(r, h,
        add32(eye_x, floor_shr(mul32(ae_rds16(r, vector), root), 11)), eye_y,
        add32(eye_z, floor_shr(mul32(ae_rds16(r, vector + 4u), root), 11))));
    if (ground_a < desired_y - 0x100 || ground_b < desired_y - 0x100) {
        uint32_t count;
        if (near != 0 && ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE)) == 0)
            ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW), ae_rd32(r, slot_addr(r, slot, AE_CAM_SAVED_YAW)));
        ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG), 0);
        count = ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT));
        if (count != 0xFFu)
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT), count + 1u);
        ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER), 6);
    } else {
        if (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER)) == 0)
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT), 0);
        if (ae_rd8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES)) == 0) {
            uint32_t flag = ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG));
            if (flag != 0xFFu)
                ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG), flag + 1u);
        }
    }
}

/* 8004923C */
void ae_CameraSlotStickOrbit(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    int32_t can_exit = call_camera(r, h, h ? h->PlayerCanExitStage : NULL,
                                   "PlayerCanExitStage", 0, 0, 0, 0);
    int32_t stick_x = 0;
    int32_t stick_y = 0;
    uint32_t held = ae_rd32(r, AE_G_PAD_HELD);
    int32_t eye_y;

    if (can_exit == 0)
        return;
    if (ae_rd8(r, AE_G_CAMERA_STATE + AE_CAM_G_FIXED_ACTIVE) == 1 &&
        ae_rd16(r, AE_G_RIGHT_STICK_MAG) > 100u) {
        stick_x = ae_rds8(r, AE_G_PLAYER_SCRATCH_X);
        stick_y = ae_rds8(r, AE_G_PLAYER_SCRATCH_Y);
    }
    ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_Y), 0);
    if (stick_x != 0 ||
        (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE)) == 0 &&
         ae_rd8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER)) == 0)) {
        if ((held & 0x2000u) != 0 || stick_x > 0) {
            ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_Y), 0x22);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG), 0);
        } else if ((held & 0x8000u) != 0 || stick_x < 0) {
            ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_Y), (uint32_t)-0x22);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG), 0);
        }
    }
    ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_X), 0);
    eye_y = ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE + 2u));
    if ((held & 0x1000u) != 0 || stick_y > 0) {
        int32_t limit = s16_bits(ae_rd16(r, AE_SCRATCH_BASE + AE_CAM_SCR_PLAYER_Y) - 0x404u);
        ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_X), (uint32_t)-0x30);
        if (eye_y - 0x30 < limit)
            ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_X), (uint32_t)(limit - eye_y));
    }
    if ((held & 0x4000u) != 0 || stick_y < 0) {
        int32_t limit = s16_bits(ae_rd16(r, AE_SCRATCH_BASE + AE_CAM_SCR_PLAYER_Y) + 0x100u);
        ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_X), 0x30);
        if (limit < eye_y + 0x30)
            ae_wr16(r, slot_addr(r, slot, AE_CAM_ORBIT_X), (uint32_t)(limit - eye_y));
    }
}

/* 80049514 */
void ae_CameraSlotSnapshotEye(AeRam *r, const AeCameraHooks *h)
{
    uint32_t follow_index = ae_rd8(r, AE_G_CAMERA_STATE + AE_CAM_G_FOLLOW_INDEX);
    uint32_t slot = camera_slot_by_index(r, follow_index);
    uint32_t player = AE_G_PLAYER;
    uint32_t vector = AE_CAMERA_TEMP_VECTOR;
    int32_t yaw;

    ae_wr8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE), 1);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER), 0);
    copy_ram(r, slot_addr(r, slot, AE_CAM_EYE), AE_G_CAMERA_EYE, 8);
    ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_DISTANCE), 0x460u);
    ae_wr16(r, slot_addr(r, slot, AE_CAM_FOLLOW_DIST), 0x460u);
    ae_wr16(r, vector, ae_rd16(r, player + AE_PLAYER_POSITION) -
                      ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE)));
    ae_wr16(r, vector + 2u, 0);
    ae_wr16(r, vector + 4u, ae_rd16(r, player + AE_PLAYER_POSITION + 8u) -
                            ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 4u)));
    call_normalize(r, h, vector);
    yaw = call_atan(r, h, neg32(ae_rds16(r, vector)), neg32(ae_rds16(r, vector + 4u)));
    ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW), (uint32_t)yaw);
}

/* 8004B8F4 */
void ae_CameraSlotGetFocusPoint(AeRam *r, const AeCameraHooks *h,
                                uint32_t slot, int32_t mode, uint32_t out)
{
    uint32_t player = AE_G_PLAYER;
    uint32_t direction = AE_CAMERA_TEMP_AIM;
    if (mode == 0) {
        int32_t dir_x, dir_z, dist;
        (void)call_camera(r, h, h ? h->CameraGetPlayerDir : NULL, "CameraGetPlayerDir",
                          slot, 0, direction, 0);
        dir_x = ae_rds16(r, direction);
        dir_z = ae_rds16(r, direction + 4u);
        dist = ae_rds16(r, slot_addr(r, slot, AE_CAM_FOCUS_DISTANCE));
        ae_wr16(r, out, (uint32_t)ae_rd16(r, player + AE_PLAYER_POSITION) +
                        (uint32_t)floor_shr(mul32(dir_x, dist), 12));
        ae_wr16(r, out + 2u, (uint32_t)ae_rd16(r, player + AE_PLAYER_POSITION + 4u) -
                             0x80u + ae_rd16(r, slot_addr(r, slot, AE_CAM_FOCUS_HEIGHT)));
        ae_wr16(r, out + 4u, (uint32_t)ae_rd16(r, player + AE_PLAYER_POSITION + 8u) +
                             (uint32_t)floor_shr(mul32(dir_z, dist), 12));
    } else if (mode == 2) {
        copy_ram(r, out, AE_G_CAMERA_FOCUS, 8);
    }
}

/* 800495F4 */
void ae_CameraFollowUpdate(AeRam *r, const AeCameraHooks *h,
                           uint32_t slot, int32_t mode)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t player = AE_G_PLAYER;
    uint32_t scratch = AE_SCRATCH_BASE;
    uint32_t flags = ae_rd32(r, player + AE_PLAYER_STATE_FLAGS) & 0xF0u;
    uint32_t player_mode = ae_rd16(r, AE_G_PLAYER_MODE);
    uint32_t player_state = ae_rd16(r, player + AE_PLAYER_STATE);
    int32_t target_height = slot_s16(r, slot, AE_CAM_TARGET_HEIGHT);
    int32_t eye_height = slot_s16(r, slot, AE_CAM_EYE_HEIGHT);
    int32_t follow_distance = slot_s16(r, slot, AE_CAM_FOLLOW_DIST);
    int32_t aim_dot;
    int32_t distance;
    int hit = 0;
    uint32_t vector = AE_CAMERA_TEMP_VECTOR;
    uint32_t projected_point = AE_CAMERA_TEMP_MATRIX;
    uint32_t projected_xy = AE_CAMERA_TEMP_MATRIX + 8u;
    uint32_t projected_depth = AE_CAMERA_TEMP_MATRIX + 12u;
    uint32_t i;

    if (ae_rd8(r, state + AE_CAM_G_FOLLOW_GUARD) != 0)
        return;
    ae_wr8(r, state + AE_CAM_G_FOLLOW_GUARD, 1);

    /* CameraFollowUpdate projects the previous player position through the
     * previous view before changing the slot. */
    ae_wr16(r, projected_point, ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X));
    ae_wr16(r, projected_point + 2u,
            ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) - 0x80u);
    ae_wr16(r, projected_point + 4u, ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z));
    (void)call_camera(r, h, h ? h->SetRotMatrix : NULL, "SetRotMatrix",
                      0x800E78C0u, 0, 0, 0);
    (void)call_camera(r, h, h ? h->SetTransMatrix : NULL, "SetTransMatrix",
                      0x800E78C0u, 0, 0, 0);
    (void)call_camera(r, h, h ? h->RotTransPers : NULL, "RotTransPers",
                      projected_point, projected_xy, projected_depth,
                      projected_depth);
    {
        uint32_t sxy = ae_rd32(r, projected_xy);
        ae_wr32(r, scratch + AE_CAM_SCR_PROJECTED_X, sxy & 0xFFFFu);
        ae_wr32(r, scratch + AE_CAM_SCR_PROJECTED_Y,
                (uint32_t)s16_bits(sxy >> 16));
    }

    if (mode == 0) {
        if (ae_rd8(r, state + AE_CAM_G_EYE_HEIGHT_ON) != 0)
            eye_height = s16_bits(ae_rd16(r, state + AE_CAM_G_EYE_HEIGHT));
        if (ae_rd8(r, state + AE_CAM_G_DISTANCE_ON) != 0)
            follow_distance = s16_bits(ae_rd16(r, state + AE_CAM_G_FOLLOW_DIST));
        if (flags == 0x80u) {
            eye_height = -0xDC;
            follow_distance = 0x26C;
        }
        if (player_mode == 10u) {
            follow_distance = 0x2F8;
            target_height = -0x32;
            eye_height = -100;
        } else if (call_camera(r, h, h ? h->PlayerIsSubmerged : NULL,
                               "PlayerIsSubmerged", 0, 0, 0, 0) != 0) {
            int32_t move_y = ae_rds32(r, scratch + AE_CAM_SCR_MOVE_Y);
            follow_distance = 0x294;
            target_height = s16_bits((uint32_t)move_y << 5);
            eye_height = s16_bits(0u - ((uint32_t)move_y << 3));
        }
    } else if (ae_rd8(r, state + AE_CAM_G_STAGE_MODE) == 1) {
        eye_height = 0;
        target_height = -0x8C;
    }

    ae_CameraSlotUpdateYaw(r, h, slot);
    if (mode == 1) {
        if ((ae_rd32(r, scratch + AE_CAM_SCR_MOVE_X) |
             ae_rd32(r, scratch + AE_CAM_SCR_MOVE_Y) |
             ae_rd32(r, scratch + AE_CAM_SCR_MOVE_Z)) == 0) {
            ae_wr8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD), 0);
            saturating_increment(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES));
        } else {
            ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);
            saturating_increment(r, slot_addr(r, slot, AE_CAM_YAW_HOLD));
        }
    }
    ae_CameraSlotStickOrbit(r, h, slot);
    aim_dot = ae_rds32(r, scratch + AE_CAM_SCR_AIM_DOT);

    if ((uint32_t)aim_dot + 3999u < 7899u) {
        int32_t side = slot_s32(r, slot, AE_CAM_SIDE);
        if (side < 0)
            side = neg32(side);
        if (side < 0x640 && ae_rd8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD)) != 0 &&
            ae_rd8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER)) == 0) {
            int32_t delta = slot_s32(r, slot, AE_CAM_YAW_DELTA);
            slot_set_s32(r, slot, AE_CAM_YAW_DELTA,
                         trunc_shr(mul32(delta, 0x3F), 6));
        }
    }

    /* Turn damping while the RC car is active or the player is standing still. */
    {
        int damp = 0;
        int32_t amount = 0;
        uint32_t follow_flag = ae_rd8(r, slot_addr(r, slot, AE_CAM_FOLLOW_FLAG_A));
        if (follow_flag != 0 &&
            call_camera(r, h, h ? h->CameraRcCarActive : NULL,
                        "CameraRcCarActive", 0, 0, 0, 0) != 0) {
            amount = follow_flag < 0x31u ? (int32_t)follow_flag : 0x30;
            damp = 1;
        } else if (ae_rd8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES)) >= 0x12u &&
                   aim_dot < 0xF3C && player_mode != 10u) {
            int32_t still = (int32_t)ae_rd8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES)) - 0x12;
            amount = still <= 0 ? 1 : (still < 0x31 ? still : 0x30);
            damp = 1;
        }
        if (damp) {
            int32_t numerator_factor = 0x40 - amount;
            int32_t divisor = numerator_factor + 1;
            int32_t numerator = mul32(slot_s32(r, slot, AE_CAM_YAW_DELTA), numerator_factor);
            slot_set_s32(r, slot, AE_CAM_YAW_DELTA,
                         cam_div(r, numerator, divisor));
        }
    }

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE)) != 0) {
        uint32_t clamp = ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE)) - 1u;
        int32_t delta;
        ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE), clamp);
        delta = trunc_shr(mul32(slot_s32(r, slot, AE_CAM_YAW_DELTA), 15), 4);
        slot_set_s32(r, slot, AE_CAM_YAW_DELTA, delta);
        if (delta < 0)
            delta = neg32(delta);
        if (delta < 0xB)
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE), 0);
    }

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER)) == 0 || flags == 0x80u) {
        if (player_mode == 10u) {
            distance = slot_s32(r, slot, AE_CAM_DISTANCE);
            slot_set_s32(r, slot, AE_CAM_DISTANCE,
                         trunc_shr(add32(mul32(distance, 7), follow_distance), 3));
        } else {
            distance = slot_s32(r, slot, AE_CAM_DISTANCE);
            if (add32(follow_distance, 0x100) < distance)
                slot_set_s32(r, slot, AE_CAM_DISTANCE,
                    trunc_shr(add32(add32(mul32(distance, 3), 0x100), follow_distance), 2));
            distance = slot_s32(r, slot, AE_CAM_DISTANCE);
            if (distance < add32(follow_distance, -0x100))
                slot_set_s32(r, slot, AE_CAM_DISTANCE,
                    trunc_shr(add32(add32(mul32(distance, 3), -0x100), follow_distance), 2));
        }
    } else {
        distance = slot_s32(r, slot, AE_CAM_DISTANCE);
        slot_set_s32(r, slot, AE_CAM_DISTANCE,
                     trunc_shr(add32(add32(distance, -0x80), follow_distance), 1));
    }

    slot_set_s32(r, slot, AE_CAM_YAW,
        mod4096(add32(add32(slot_s32(r, slot, AE_CAM_YAW_DELTA), 0x1000),
                      add32(slot_s32(r, slot, AE_CAM_TARGET_YAW),
                            slot_s16(r, slot, AE_CAM_ORBIT_Y)))));
    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER)) != 0) {
        int32_t height_delta;
        int32_t adjusted;
        int32_t value;
        ae_wr8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER),
               ae_rd8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER)) - 1u);
        height_delta = mod4096(sub32(slot_s32(r, slot, AE_CAM_HEIGHT_A),
                                     slot_s32(r, slot, AE_CAM_HEIGHT_B)));
        if (height_delta >= 0x801)
            height_delta = sub32(height_delta, 0x1000);
        adjusted = trunc_shr(mul32(height_delta, 7), 3);
        value = adjusted < 0 ? neg32(adjusted) : adjusted;
        if (value < 0xB)
            ae_wr8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER), 0);
        value = mod4096(add32(slot_s32(r, slot, AE_CAM_HEIGHT_B),
                              add32(adjusted, 0x1000)));
        slot_set_s32(r, slot, AE_CAM_HEIGHT_A, value);
        slot_set_s32(r, slot, AE_CAM_YAW, value);
    }

    /* Three ground probes pull the eye forward when the camera crosses a wall. */
    if (aim_dot < 1000) {
        int32_t eye_y = slot_s16(r, slot, AE_CAM_EYE + 2u);
        int32_t ground_ref = ae_rds32(r, scratch + AE_CAM_SCR_GROUND_REF);
        int32_t difference = sub32(ground_ref, eye_y);
        int in_range = difference >= 0 ? difference < 0x800 : sub32(eye_y, ground_ref) < 0x800;
        int32_t stick_x = ae_rds8(r, AE_G_PLAYER_SCRATCH_RIGHT_X);
        if (stick_x < 0)
            stick_x = neg32(stick_x);
        if (in_range && stick_x > 0x32) {
            int32_t px, pz, perpendicular_x, perpendicular_z;
            uint32_t step_x, step_y, step_z;
            int32_t offset_x, offset_y, offset_z;
            ae_wr16(r, vector, ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE)) -
                             ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X));
            ae_wr16(r, vector + 2u, 0);
            ae_wr16(r, vector + 4u, ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 4u)) -
                                   ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z));
            call_normalize(r, h, vector);
            px = ae_rds16(r, vector);
            pz = ae_rds16(r, vector + 4u);
            if (ae_rds8(r, AE_G_PLAYER_SCRATCH_RIGHT_X) > 0) {
                perpendicular_x = neg32(pz);
                perpendicular_z = px;
            } else {
                perpendicular_x = pz;
                perpendicular_z = neg32(px);
            }
            offset_x = sub32(floor_shr(mulhi_s32(perpendicular_x, 0x2AAAAAAB), 1),
                             perpendicular_x < 0 ? -1 : 0);
            offset_z = sub32(floor_shr(mulhi_s32(perpendicular_z, 0x2AAAAAAB), 1),
                             perpendicular_z < 0 ? -1 : 0);
            step_x = (uint32_t)trunc_shr(sub32(slot_s16(r, slot, AE_CAM_EYE),
                                               scratch_s16(r, AE_CAM_SCR_PLAYER_X)), 2);
            step_y = (uint32_t)trunc_shr(sub32(slot_s16(r, slot, AE_CAM_EYE + 2u),
                                               s16_bits(ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) - 0x80u)), 2);
            step_z = (uint32_t)trunc_shr(sub32(slot_s16(r, slot, AE_CAM_EYE + 4u),
                                               scratch_s16(r, AE_CAM_SCR_PLAYER_Z)), 2);
            offset_y = (int32_t)(ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) - 0x80u);
            for (i = 0; i < 3; i++) {
                int32_t probe_y = s16_bits((uint32_t)offset_y);
                int32_t ground = ground_s16(r, h,
                    s16_bits(ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X) + (uint32_t)offset_x),
                    probe_y,
                    s16_bits(ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z) + (uint32_t)offset_z));
                if (ground < probe_y - 0xC0) {
                    hit = 1;
                    break;
                }
                offset_y = add32(offset_y, s16_bits(step_y));
                offset_z = add32(offset_z, s16_bits(step_z));
                offset_x = add32(offset_x, s16_bits(step_x));
            }
        }
    }

    /* A held camera checks the ground immediately behind its eye. */
    if (aim_dot > 2000 && ae_rd8(r, slot_addr(r, slot, AE_CAM_YAW_HOLD)) != 0) {
        int32_t ground;
        call_memset(r, h, vector, 0, 8);
        ae_wr16(r, vector, ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X) -
                         ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE)));
        ae_wr16(r, vector + 2u, 0);
        ae_wr16(r, vector + 4u, ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z) -
                               ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 4u)));
        call_normalize(r, h, vector);
        ground = ground_s16(r, h,
            sub32(slot_s16(r, slot, AE_CAM_EYE), trunc_shr(ae_rds16(r, vector), 5)),
            slot_s16(r, slot, AE_CAM_EYE + 2u),
            sub32(slot_s16(r, slot, AE_CAM_EYE + 4u), trunc_shr(ae_rds16(r, vector + 4u), 5)));
        if (ground < slot_s16(r, slot, AE_CAM_EYE + 2u)) {
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE), 5);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT), 0);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER), 0);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG), 0xFFu);
        }
    }

    if (hit) {
        if (aim_dot < 0xDAC) {
            int32_t d;
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE), 8);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT), 0);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER), 0);
            d = sub32(slot_s32(r, slot, AE_CAM_DISTANCE), 0x258);
            slot_set_s32(r, slot, AE_CAM_DISTANCE, d < 0 ? 0 : d);
            d = slot_s32(r, slot, AE_CAM_DISTANCE);
            slot_set_s32(r, slot, AE_CAM_DISTANCE,
                         add32(trunc_shr(mul32(d, 7), 3), 0x258));
        }
    } else {
        uint32_t raw_delta = (uint32_t)ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) -
                             ae_rd16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 2u));
        int32_t delta = s16_bits(raw_delta);
        int32_t magnitude = delta < 0 ? neg32(delta) : delta;
        if (delta != 0 && magnitude < 0x60 &&
            (flags == 0x20u || flags == 0x30u || flags == 0xA0u || player_state == 6u)) {
            uint32_t scaled = raw_delta;
            int32_t value;
            if (aim_dot > 0 && delta < 0)
                scaled = (uint32_t)magnitude << 2;
            value = s16_bits(scaled);
            slot_set_s16(r, slot, AE_CAM_EYE_VELOCITY + 2u,
                         trunc_shr(neg32(value), 1));
            slot_set_s16(r, slot, AE_CAM_TARGET_VELOCITY + 2u,
                         sub32(mulhi_s32(value, 0x55555556), value < 0 ? -1 : 0));
        }
    }

    /* The target follows the player's movement direction and vertical state. */
    {
        int32_t move_x = ae_rds32(r, scratch + AE_CAM_SCR_MOVE_X);
        int32_t move_y = ae_rds32(r, scratch + AE_CAM_SCR_MOVE_Y);
        int32_t move_z = ae_rds32(r, scratch + AE_CAM_SCR_MOVE_Z);
        int32_t root = camera_sqrt(r, h,
            add32(mul32(move_x, move_x), mul32(move_z, move_z)));
        int32_t amount = root >= 0x21 ? 0x20 : root;
        int32_t dir_x = scratch_s16(r, AE_CAM_SCR_DIRECTION);
        int32_t dir_z = scratch_s16(r, AE_CAM_SCR_DIRECTION + 4u);
        int32_t x_component = trunc_shr(mul32(trunc_shr(dir_x, 4), amount), 5);
        int32_t z_component = trunc_shr(mul32(trunc_shr(dir_z, 4), amount), 5);
        if (mode == 1) {
            x_component = neg32(x_component);
            z_component = neg32(z_component);
        }
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u,
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) +
                (uint32_t)target_height - 0x80u);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT,
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X) + (uint32_t)x_component +
                (uint32_t)mul32(move_x, amount));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 4u,
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z) + (uint32_t)z_component +
                (uint32_t)mul32(move_z, amount));
        if (flags == 0x50u && move_y > 0)
            ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u,
                    ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u) +
                    ((uint32_t)move_y << 6));
        if (ae_rds32(r, scratch + AE_CAM_SCR_PROJECTED_Y) > 0xD0 &&
            move_y > 0x20 && flags != 0x40u && aim_dot < 0) {
            int32_t shortened = trunc_shr(mul32(slot_s32(r, slot, AE_CAM_DISTANCE), 7), 3);
            slot_set_s32(r, slot, AE_CAM_DISTANCE, shortened < 0x200 ? 0x200 : shortened);
        }
        if (ae_rds16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u) > 0 &&
            ae_rd16(r, AE_G_CURRENT_AREA) != 7u)
            ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u, 0);
    }

    /* Desired eye position from the target, yaw and smoothed distance. */
    {
        int32_t yaw = slot_s32(r, slot, AE_CAM_YAW);
        int32_t dist = slot_s32(r, slot, AE_CAM_DISTANCE);
        int32_t cosv = camera_cos(r, h, yaw);
        int32_t sinv;
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE,
                ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET)) +
                (uint32_t)floor_shr(mul32(cosv, dist), 12));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u,
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) +
                (uint32_t)eye_height - 0x80u);
        sinv = camera_sin(r, h, yaw);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u,
                ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET + 4u)) +
                (uint32_t)floor_shr(mul32(sinv, dist), 12));
    }

    if (s8_bits(ae_rd8(r, player + AE_PLAYER_CURRENT_GADGET)) == 3 &&
        ae_rd8(r, state + AE_CAM_G_AIM_MODE) != 1)
        ae_CameraFollowGadgetVariant(r, h, slot);

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER)) != 0)
        ae_wr8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER),
               ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER)) - 1u);
    ae_CameraSlotGroundClampNear(r, h, slot, 0);

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_TIMER)) != 0) {
        int32_t yaw = slot_s32(r, slot, AE_CAM_YAW);
        int32_t dist = slot_s32(r, slot, AE_CAM_DISTANCE);
        int32_t sinv, cosv;
        if (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_COUNT)) >= 5u) {
            dist = sub32(dist, 0x30);
            if (dist < 0x180) {
                dist = 0x180;
                if (slot_s16(r, slot, AE_CAM_EYE + 2u) <
                    sub32(scratch_s16(r, AE_CAM_SCR_PLAYER_Y), 0x60))
                    slot_set_s16(r, slot, AE_CAM_EYE_VELOCITY + 2u, 8);
            }
            slot_set_s32(r, slot, AE_CAM_DISTANCE, dist);
            slot_set_s16(r, slot, AE_CAM_FOLLOW_DIST, dist);
        }
        cosv = camera_cos(r, h, yaw);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE,
                ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET)) +
                (uint32_t)floor_shr(mul32(cosv, dist), 12));
        sinv = camera_sin(r, h, yaw);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u,
                ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET + 4u)) +
                (uint32_t)floor_shr(mul32(sinv, dist), 12));
        ae_CameraSlotGroundClampNear(r, h, slot, 1);
    }

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE)) == 0 &&
        slot_s16(r, slot, AE_CAM_FOLLOW_DIST) < 0x460 &&
        ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_FLAG)) >= 0x0D) {
        int32_t follow = slot_s16(r, slot, AE_CAM_FOLLOW_DIST);
        slot_set_s16(r, slot, AE_CAM_FOLLOW_DIST,
                     trunc_shr(add32(mul32(follow, 3), 0x460), 2));
    }

    if ((ae_rd32(r, AE_G_STAGE_FLAGS) & 1u) == 0 &&
        ae_rd8(r, slot_addr(r, slot, AE_CAM_CLAMP_STATE)) == 0) {
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE,
                ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_EYE) +
                (uint32_t)trunc_shr(mul32(ae_rds32(r, scratch + AE_CAM_SCR_MOVE_X), 3), 1));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u,
                ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u) +
                (uint32_t)trunc_shr(mul32(ae_rds32(r, scratch + AE_CAM_SCR_MOVE_Z), 3), 1));
    }
    if (flags == 0x50u && ae_rds32(r, scratch + AE_CAM_SCR_MOVE_Y) > 0)
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u,
                ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u) -
                ((uint32_t)ae_rd32(r, scratch + AE_CAM_SCR_MOVE_Y) << 6));
    if (ae_rds16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u) > 0)
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u, 0);
    if (player_state == 0x2Du) {
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE,
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X) -
                (uint32_t)trunc_shr(scratch_s16(r, AE_CAM_SCR_DIRECTION), 6));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u,
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z) -
                (uint32_t)trunc_shr(scratch_s16(r, AE_CAM_SCR_DIRECTION + 4u), 6));
    }

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE)) != 0) {
        slot_set_s16(r, slot, AE_CAM_TARGET,
                     ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X) +
                     (uint32_t)trunc_shr(scratch_s16(r, AE_CAM_SCR_DIRECTION), 4));
        slot_set_s16(r, slot, AE_CAM_TARGET + 2u,
                     ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) +
                     (uint32_t)target_height - 0x80u);
        slot_set_s16(r, slot, AE_CAM_TARGET + 4u,
                     ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z) +
                     (uint32_t)trunc_shr(scratch_s16(r, AE_CAM_SCR_DIRECTION + 4u), 4));
    }

    /* Target smoothing and vertical velocity response. */
    slot_set_s16(r, slot, AE_CAM_TARGET,
        trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_TGT),
                        mul32(slot_s16(r, slot, AE_CAM_TARGET), 31)), 5));
    slot_set_s16(r, slot, AE_CAM_TARGET + 4u,
        trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_TGT + 4u),
                        mul32(slot_s16(r, slot, AE_CAM_TARGET + 4u), 31)), 5));
    if (flags == 0x40u)
        slot_set_s16(r, slot, AE_CAM_TARGET + 2u,
            trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_TGT + 2u),
                            mul32(slot_s16(r, slot, AE_CAM_TARGET + 2u), 31)), 5));
    else
        slot_set_s16(r, slot, AE_CAM_TARGET + 2u,
            trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_TGT + 2u),
                            mul32(slot_s16(r, slot, AE_CAM_TARGET + 2u), 15)), 4));

    {
        int32_t projected_y = ae_rds32(r, scratch + AE_CAM_SCR_PROJECTED_Y);
        int32_t move_y = ae_rds32(r, scratch + AE_CAM_SCR_MOVE_Y);
        int skip = flags == 0x50u ? projected_y >= 0x30 : projected_y >= 0x20;
        if (!skip && move_y < 0)
            slot_set_s16(r, slot, AE_CAM_TARGET_VELOCITY + 2u, move_y - 8);
        if (projected_y > 0xD0 && move_y > 0) {
            uint32_t velocity = ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET_VELOCITY + 2u)) +
                                (uint32_t)trunc_shr(move_y, 1);
            slot_set_s16(r, slot, AE_CAM_TARGET_VELOCITY + 2u, (int32_t)velocity);
            if (s16_bits(velocity) > 0x60)
                slot_set_s16(r, slot, AE_CAM_TARGET_VELOCITY + 2u, 0x60);
        }
        {
            int32_t velocity = slot_s16(r, slot, AE_CAM_TARGET_VELOCITY + 2u);
            slot_set_s16(r, slot, AE_CAM_TARGET + 2u,
                ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET + 2u)) +
                ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET_VELOCITY + 2u)));
            slot_set_s16(r, slot, AE_CAM_TARGET_VELOCITY + 2u,
                         trunc_shr(mul32(velocity, 7), 3));
        }
    }

    if (ae_rd8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE)) == 0) {
        int32_t eye_velocity;
        slot_set_s16(r, slot, AE_CAM_EYE,
            trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_EYE),
                            slot_s16(r, slot, AE_CAM_EYE)), 1));
        slot_set_s16(r, slot, AE_CAM_EYE + 4u,
            trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_EYE + 4u),
                            slot_s16(r, slot, AE_CAM_EYE + 4u)), 1));
        if (flags == 0x40u)
            slot_set_s16(r, slot, AE_CAM_EYE + 2u,
                trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_EYE + 2u),
                                mul32(slot_s16(r, slot, AE_CAM_EYE + 2u), 63)), 6));
        else
            slot_set_s16(r, slot, AE_CAM_EYE + 2u,
                trunc_shr(add32(scratch_s16(r, AE_CAM_SCR_DESIRED_EYE + 2u),
                                mul32(slot_s16(r, slot, AE_CAM_EYE + 2u), 31)), 5));
        eye_velocity = slot_s16(r, slot, AE_CAM_EYE_VELOCITY + 2u);
        slot_set_s16(r, slot, AE_CAM_EYE + 2u,
            ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 2u)) +
            ae_rd16(r, slot_addr(r, slot, AE_CAM_ORBIT_X)));
        slot_set_s16(r, slot, AE_CAM_EYE + 2u,
            ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 2u)) +
            ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE_VELOCITY + 2u)));
        slot_set_s16(r, slot, AE_CAM_EYE_VELOCITY + 2u,
                     trunc_shr(mul32(eye_velocity, 7), 3));
    } else {
        int32_t yaw;
        copy_ram(r, slot_addr(r, slot, AE_CAM_EYE),
                 scratch + AE_CAM_SCR_DESIRED_EYE, 8);
        ae_wr16(r, vector, ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET)) -
                         ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE)));
        ae_wr16(r, vector + 2u, 0);
        ae_wr16(r, vector + 4u, ae_rd16(r, slot_addr(r, slot, AE_CAM_TARGET + 4u)) -
                               ae_rd16(r, slot_addr(r, slot, AE_CAM_EYE + 4u)));
        call_normalize(r, h, vector);
        yaw = call_atan(r, h, neg32(ae_rds16(r, vector)),
                        neg32(ae_rds16(r, vector + 4u)));
        slot_set_s32(r, slot, AE_CAM_YAW, yaw);
    }

    if (player_state != 0x2Du)
        ae_CameraSlotGroundClamp(r, h, slot);

    {
        int32_t dx = sub32(slot_s16(r, slot, AE_CAM_TARGET),
                           slot_s16(r, slot, AE_CAM_EYE));
        int32_t dz = sub32(slot_s16(r, slot, AE_CAM_TARGET + 4u),
                           slot_s16(r, slot, AE_CAM_EYE + 4u));
        int32_t result;
        ae_wr16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS),
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_X));
        ae_wr16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 2u),
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y));
        ae_wr16(r, slot_addr(r, slot, AE_CAM_LAST_PLAYER_POS + 4u),
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Z));
        result = camera_sqrt(r, h, add32(mul32(dx, dx), mul32(dz, dz)));
        slot_set_s32(r, slot, AE_CAM_DISTANCE, result);
    }
    ae_wr8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE), 0);
    slot_set_s32(r, slot, AE_CAM_SAVED_YAW, slot_s32(r, slot, AE_CAM_YAW));
}

/* 800482C0 */
void ae_CameraFollowDispatch(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    int32_t player_mode = ae_rds16(r, AE_G_PLAYER_MODE);
    if (player_mode == 3) {
        ae_CameraKind3FollowUpdate(r, h, slot);
        return;
    }
    if (player_mode == 1) {
        (void)call_camera(r, h, h ? h->CameraAreaScriptDispatch : NULL,
                          "CameraAreaScriptDispatch", 1, slot, 0, 0);
        return;
    }
    if (call_camera(r, h, h ? h->CameraRcCarActive : NULL,
                    "CameraRcCarActive", 0, 0, 0, 0) != 0) {
        (void)call_camera(r, h, h ? h->CameraRcCarFollow : NULL,
                          "CameraRcCarFollow", slot, 0, 0, 0);
        return;
    }
    if (call_camera(r, h, h ? h->IsFlightArea : NULL,
                    "IsFlightArea", 0, 0, 0, 0) != 0) {
        (void)call_camera(r, h, h ? h->FUN_8013d000 : NULL,
                          "FUN_8013d000", slot, 0, 0, 0);
        return;
    }
    if (call_camera(r, h, h ? h->BossUpdate : NULL,
                    "BossUpdate", slot, 0, 0, 0) != 0)
        return;
    if (ae_rd8(r, AE_G_CAMERA_STATE + AE_CAM_G_FIXED_ACTIVE) != 1)
        ae_CameraFollowUpdate(r, h, slot, 0);
}

/* 8004B5FC */
void ae_CameraKind3FollowUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t scratch = AE_SCRATCH_BASE;
    uint32_t player = AE_G_PLAYER;
    int32_t distance = ae_rds32(r, slot_addr(r, slot, AE_CAM_DISTANCE));
    int32_t half_distance = trunc_shr(add32(distance, 0x464), 1);
    uint32_t held = ae_rd32(r, AE_G_PAD_HELD);
    int32_t side;
    int32_t yaw;
    int32_t trig;
    int32_t pos_x = ae_rd16(r, player + AE_PLAYER_POSITION);
    int32_t pos_y = ae_rd16(r, player + AE_PLAYER_POSITION + 4u);
    int32_t pos_z = ae_rd16(r, player + AE_PLAYER_POSITION + 8u);

    ae_wr16(r, scratch + AE_CAM_SCR_PLAYER_X, (uint32_t)pos_x);
    ae_wr16(r, scratch + AE_CAM_SCR_CAMERA_POSITION_Y, (uint32_t)pos_y - 0x80u);
    ae_wr16(r, scratch + AE_CAM_SCR_PLAYER_Z, (uint32_t)pos_z);
    ae_wr32(r, slot_addr(r, slot, AE_CAM_DISTANCE), (uint32_t)half_distance);
    ae_CameraSlotAimAtFocus(r, h, slot);
    held = ae_rd32(r, AE_G_PAD_HELD);
    side = ae_rds32(r, scratch + AE_CAM_SCR_AIM_CROSS);
    yaw = ae_rds32(r, slot_addr(r, slot, AE_CAM_YAW));
    if ((held & 0xA000u) == 0) {
        if (side < -182)
            yaw = add32(yaw, 0x1016) % 0x1000;
        else if (side >= 183)
            yaw = add32(yaw, 0x0FEAu) % 0x1000;
    }
    if ((held & 0x8000u) != 0)
        yaw = add32(yaw, 0x0FC8) % 0x1000;
    if ((held & 0x2000u) != 0)
        yaw = add32(yaw, 0x1038) % 0x1000;
    ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW), (uint32_t)yaw);
    copy_ram(r, scratch + AE_CAM_SCR_DESIRED_TGT,
             scratch + AE_CAM_SCR_PLAYER_X, 6);

    trig = call_camera(r, h, h ? h->rcos : NULL, "rcos", (uint32_t)yaw, 0, 0, 0);
    ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE,
            (uint32_t)ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_TGT) +
            (uint32_t)floor_shr(mul32(trig, half_distance), 12));
    ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u,
            (uint32_t)ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u) - 0x1E0u);
    trig = call_camera(r, h, h ? h->rsin : NULL, "rsin", (uint32_t)yaw, 0, 0, 0);
    ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u,
            (uint32_t)ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 4u) +
            (uint32_t)floor_shr(mul32(trig, half_distance), 12));
    if ((held & 0x4000u) != 0)
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u,
                ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u) + 0x180u);
    if ((held & 0x1000u) != 0) {
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u,
                ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u) - 0x200u);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u,
                ae_rd16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u) + 0x100u);
    }
    ae_CameraSlotSmoothTarget(r, h, slot, 8, 8, 8);
    ae_CameraSlotSmoothEye(r, h, slot, 5, 8, 5);
}

static int32_t atan_yaw_remainder(int32_t value)
{
    return mod4096(value);
}

/* 8004CA70 */
void ae_CameraSlotUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    int32_t timer;
    uint32_t state;
    uint32_t focus = AE_CAMERA_TEMP_FOCUS;
    uint32_t eye = AE_CAMERA_TEMP_FOCUS + 8u;
    int32_t mode;
    uint32_t i;

    ae_wr8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE), 0);
    timer = ae_rds16(r, slot_addr(r, slot, AE_CAM_SCRIPT_TIMER));
    if (timer == 0)
        return;
    if (timer > 0)
        ae_wr16(r, slot_addr(r, slot, AE_CAM_SCRIPT_TIMER), (uint32_t)(timer - 1));
    mode = ae_rd8(r, slot_addr(r, slot, AE_CAM_SCRIPT_ACTIVE));
    if (mode != 0) {
        (void)call_camera(r, h, h ? h->CallIndexedHandler : NULL,
                          "CallIndexedHandler", slot, (uint32_t)mode, 0, 0);
        return;
    }

    mode = ae_rd8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE + 1u));
    ae_CameraSlotGetFocusPoint(r, h, slot, mode, focus);
    copy_ram(r, eye, slot_addr(r, slot, AE_CAM_EYE), 8);
    state = ae_rd8(r, slot_addr(r, slot, AE_CAM_SCRIPT_STATE));

    if (state == 2u) {
        ae_CameraSlotGetFocusPoint(r, h, slot, mode, eye);
    } else if (state == 5u) {
        int32_t yaw = ae_rds32(r, slot_addr(r, slot, AE_CAM_YAW));
        int32_t distance = ae_rds32(r, slot_addr(r, slot, AE_CAM_DISTANCE));
        int32_t cosv = call_camera(r, h, h ? h->rcos : NULL, "rcos", (uint32_t)yaw, 0, 0, 0);
        int32_t sinv = call_camera(r, h, h ? h->rsin : NULL, "rsin", (uint32_t)yaw, 0, 0, 0);
        ae_wr16(r, eye, (uint32_t)ae_rd16(r, focus) +
                        (uint32_t)floor_shr(mul32(cosv, distance), 12));
        ae_wr16(r, eye + 4u, (uint32_t)ae_rd16(r, focus + 4u) +
                            (uint32_t)floor_shr(mul32(sinv, distance), 12));
        ae_wr16(r, eye + 2u, ae_rd16(r, eye + 2u) +
                            ae_rd16(r, slot_addr(r, slot, AE_CAM_SCRIPT_VELOCITY + 2u)));
        ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW),
                (uint32_t)atan_yaw_remainder(add32(yaw,
                    add32(ae_rds16(r, slot_addr(r, slot, AE_CAM_SCRIPT_RATE)), 0x1000))));
        ae_wr32(r, slot_addr(r, slot, AE_CAM_DISTANCE),
                (uint32_t)add32(distance,
                    ae_rds16(r, slot_addr(r, slot, AE_CAM_SCRIPT_VELOCITY))));
    } else if (state == 7u) {
        for (i = 0; i < 3; i++)
            ae_wr16(r, eye + 2u * i,
                    ae_rd16(r, eye + 2u * i) +
                    ae_rd16(r, slot_addr(r, slot, AE_CAM_SCRIPT_VELOCITY + 2u * i)));
    } else if (state == 8u || state == 9u) {
        uint32_t vector = AE_CAMERA_TEMP_MATRIX;
        uint32_t player = AE_G_PLAYER;
        int32_t normalized[3];
        ae_wr16(r, vector, ae_rd16(r, player + AE_PLAYER_POSITION) - ae_rd16(r, eye));
        ae_wr16(r, vector + 2u, ae_rd16(r, player + AE_PLAYER_POSITION + 4u) -
                               (ae_rd16(r, eye + 2u) + 0x80u));
        ae_wr16(r, vector + 4u, ae_rd16(r, player + AE_PLAYER_POSITION + 8u) -
                               ae_rd16(r, eye + 4u));
        call_normalize(r, h, vector);
        normalized[0] = ae_rds16(r, vector);
        normalized[1] = ae_rds16(r, vector + 2u);
        normalized[2] = ae_rds16(r, vector + 4u);
        if (state == 8u) {
            for (i = 0; i < 3; i++) {
                if (i != 1u) {
                    uint32_t axis = 0xA8u + 2u * i;
                    int32_t anchor = ae_rds16(r, slot_addr(r, slot, axis));
                    int32_t player_pos = ae_rds32(r, player + AE_PLAYER_POSITION + 4u * i);
                    int32_t delta = sub32(player_pos, anchor);
                    ae_wr16(r, eye + 2u * i,
                            (uint32_t)add32(anchor, trunc_shr(delta, 2)));
                }
            }
        } else {
            copy_ram(r, eye, slot_addr(r, slot, AE_CAM_ANCHOR), 8);
            ae_wr8(r, slot_addr(r, slot, AE_CAM_FIXED_ACTIVE), 1);
        }
        for (i = 0; i < 3; i++) {
            int32_t current = ae_rds16(r, focus + 2u * i);
            int32_t operand = ae_rds16(r, slot_addr(r, slot, AE_CAM_SCRIPT_FOCUS + 2u * i));
            int32_t sum = add32(normalized[i], operand);
            ae_wr16(r, focus + 2u * i,
                    (uint32_t)add32(ae_rds16(r, eye + 2u * i), trunc_shr(sum, 1)));
            (void)current;
        }
    }

    if (ae_rd8(r, AE_G_CAMERA_SCRIPT_ACTIVE) != 0) {
        ae_wr16(r, focus + 2u, ae_rd16(r, focus + 2u) + 3u);
        ae_wr16(r, eye + 2u, ae_rd16(r, eye + 2u) - 6u);
    }
    copy_ram(r, slot_addr(r, slot, AE_CAM_TARGET), focus, 8);
    copy_ram(r, slot_addr(r, slot, AE_CAM_EYE), eye, 8);
}

/* 8004AC84 */
void ae_CameraFollowAimedVariant(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t player = AE_G_PLAYER;
    uint32_t scratch = AE_SCRATCH_BASE;

    if (ae_rd8(r, state + AE_CAM_G_AIM_MODE) == 1u)
        ae_wr8(r, state + AE_CAM_G_MODE_TIMER, 0);

    if (ae_rd16(r, state + AE_CAM_G_ACTIVE_MODE) != 2u) {
        int32_t distance = slot_s32(r, slot, AE_CAM_DISTANCE);
        int32_t adjusted_distance = add32(distance, 0xE00);
        ae_CameraSlotPlaceBehindPlayer(r, h, slot,
                                       scratch + AE_CAM_SCR_PLAYER_X,
                                       distance);
        ae_wr16(r, slot_addr(r, slot, AE_CAM_TARGET_Y),
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) - 0x12Cu);
        ae_wr16(r, slot_addr(r, slot, AE_CAM_EYE_Y),
                ae_rd16(r, scratch + AE_CAM_SCR_PLAYER_Y) - 0x12Cu);
        if (adjusted_distance < 0)
            adjusted_distance = add32(adjusted_distance, 7);
        slot_set_s32(r, slot, AE_CAM_DISTANCE,
                     floor_shr(adjusted_distance, 3));
        return;
    }

    {
        uint32_t matrix = 0x801FF100u;
        uint32_t facing = player + AE_PLAYER_FACING;
        uint32_t active_slot;
        int32_t height_offset =
            (ae_rd32(r, player + AE_PLAYER_STATE_FLAGS) & 0xF0u) == 0x80u ? 0x50 : 0;
        int32_t stick_x = ae_rds8(r, AE_G_PLAYER_SCRATCH_X);
        int32_t angle = trunc_shr(stick_x, 2);
        int32_t stick_y = ae_rds8(r, AE_G_PLAYER_SCRATCH_Y);
        int32_t target_aim_y;
        int32_t target_y;
        int32_t eye_y;

        if ((ae_rd32(r, AE_G_PAD_HELD) & 0x8000u) != 0)
            angle = sub32(angle, 0x18);
        if ((ae_rd32(r, AE_G_PAD_HELD) & 0x2000u) != 0)
            angle = add32(angle, 0x18);

        (void)call_camera(r, h, h ? h->MatrixIdentity : NULL,
                          "MatrixIdentity", matrix, 0, 0, 0);
        (void)call_camera(r, h, h ? h->RotMatrixY : NULL,
                          "RotMatrixY", (uint32_t)angle, matrix, 0, 0);
        (void)call_camera(r, h, h ? h->ApplyMatrixSV : NULL,
                          "ApplyMatrixSV", matrix, facing, facing, 0);
        call_normalize(r, h, facing);

        target_y = add32(ae_rds16(r, player + AE_PLAYER_POSITION_Y),
                         height_offset - 0xC8);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT,
                (uint32_t)ae_rds16(r, player + AE_PLAYER_POSITION_X) +
                (uint32_t)trunc_shr(ae_rds16(r, facing), 1));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u,
                (uint32_t)target_y);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 4u,
                (uint32_t)ae_rds16(r, player + AE_PLAYER_POSITION_Z) +
                (uint32_t)trunc_shr(ae_rds16(r, facing + 4u), 1));

        target_aim_y = add32(ae_rds16(r, slot_addr(r, slot, AE_CAM_TARGET_Y)),
                             (int32_t)((uint32_t)stick_y << 1));
        if ((ae_rd32(r, AE_G_PAD_HELD) & 0x1000u) != 0)
            target_aim_y = add32(target_aim_y, 0x100);
        if ((ae_rd32(r, AE_G_PAD_HELD) & 0x4000u) != 0)
            target_aim_y = sub32(target_aim_y, 0x100);
        eye_y = add32(ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE_Y)),
                      ae_rd8(r, state + AE_CAM_G_AIM_MODE) == 1u ? 0x400 : 0x734);
        if (target_aim_y > eye_y)
            target_aim_y = eye_y;
        if (target_aim_y < sub32(ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE_Y)), 0xABC))
            target_aim_y = sub32(ae_rds16(r, slot_addr(r, slot, AE_CAM_EYE_Y)), 0xABC);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE,
                ae_rd16(r, player + AE_PLAYER_POSITION_X));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 2u,
                (uint32_t)target_y);
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_EYE + 4u,
                ae_rd16(r, player + AE_PLAYER_POSITION_Z));
        ae_wr16(r, scratch + AE_CAM_SCR_DESIRED_TGT + 2u,
                (uint32_t)target_aim_y);

        ae_CameraSlotSmoothTarget(r, h, slot, 4, 3, 4);
        ae_CameraSlotSmoothEye(r, h, slot, 4, 3, 4);
        ae_wr8(r, state + AE_CAM_G_MODE_TIMER + 1u, 0);

        active_slot = camera_slot_by_index(r,
                         ae_rd8(r, state + AE_CAM_G_FOLLOW_INDEX));
        ae_CameraSlotPlaceBehindPlayer(r, h, active_slot,
                                       scratch + AE_CAM_SCR_PLAYER_X, 0x333);
        if (ae_rd16(r, AE_G_PLAYER_MODE) == 1u) {
            (void)call_camera(r, h, h ? h->CameraAreaScriptDispatch : NULL,
                              "CameraAreaScriptDispatch", 2, slot, 0, 0);
            return;
        }
        if (ae_rd8(r, state + AE_CAM_G_AIM_MODE) == 1u)
            ae_CameraFollowGadgetVariant(r, h, slot);
    }
}

enum {
    AE_GPU_PACKET_TAG = 0x00u,
    AE_GPU_PACKET_CODE = 0x03u,
    AE_GPU_PACKET_WORD = 0x04u,
    AE_GPU_PACKET_SIZE = 0x10u,
    AE_GPU_POLY_BYTES = 0x28u,
    AE_GPU_POLY_TAG = 0x00u,
    AE_GPU_POLY_CODE = 0x03u,
    AE_GPU_POLY_COLOR = 0x04u,
    AE_GPU_POLY_XY0 = 0x08u,
    AE_GPU_POLY_TEX0 = 0x0Cu,
    AE_GPU_POLY_XY1 = 0x10u,
    AE_GPU_POLY_TEX1 = 0x14u,
    AE_GPU_POLY_XY2 = 0x18u,
    AE_GPU_POLY_UV2 = 0x1Cu,
    AE_GPU_POLY_XY3 = 0x20u,
    AE_GPU_POLY_UV3 = 0x24u,
    AE_GPU_POLY_SIZE = 0x28u,
    AE_GPU_BUCKET_BACK = 0x04u,
    AE_GPU_BUCKET_FRONT = 0x08u
};

static uint32_t pack_xy16(int32_t y, int32_t x)
{
    return ((uint32_t)y << 16) | ((uint32_t)x & 0xFFFFu);
}

static uint32_t link_gpu_packet(AeRam *r, uint32_t bucket, uint32_t link_field,
                                uint32_t packet)
{
    uint32_t old_link = ae_rd32(r, bucket + link_field);
    uint32_t packet_tag = ae_rd32(r, packet + AE_GPU_PACKET_TAG);
    ae_wr32(r, packet + AE_GPU_PACKET_TAG,
            (packet_tag & 0xFF000000u) | (old_link & 0x00FFFFFFu));
    ae_wr32(r, bucket + link_field,
            (old_link & 0xFF000000u) | (packet & 0x00FFFFFFu));
    return packet;
}

static void camera_emit_gadget_primitives(AeRam *r, int32_t center_x,
                                          int32_t center_y,
                                          int32_t flat_x,
                                          int32_t flat_y,
                                          int32_t half_size,
                                          int draw_flat,
                                          int draw_textured)
{
    uint32_t order_index = ae_rd32(r, AE_G_CAMERA_GADGET_ORDER_INDEX);
    uint32_t bucket_offset = (uint32_t)((uint32_t)order_index * 0x4078u);
    uint32_t flat_bucket = AE_G_CAMERA_DRAW_ENV + bucket_offset;
    uint32_t textured_bucket = flat_bucket;
    uint32_t cursor = ae_rd32(r, AE_G_CAMERA_GADGET_PRIM_CURSOR);
    int32_t line_left = sub32(flat_x, 7);
    int32_t line_right = add32(flat_x, 7);
    uint32_t iteration;

    if (draw_flat) {
        uint32_t flat = cursor;
        uint32_t draw_mode = flat + AE_GPU_PACKET_SIZE;
        ae_wr8(r, flat + AE_GPU_PACKET_CODE, 3);
        ae_wr32(r, flat + AE_GPU_PACKET_WORD, 0x43010101u);
        ae_wr32(r, flat + AE_GPU_PACKET_WORD + 4u,
                pack_xy16(flat_y, line_left));
        ae_wr32(r, flat + AE_GPU_PACKET_WORD + 8u,
                pack_xy16(flat_y, line_right));
        (void)link_gpu_packet(r, flat_bucket, AE_GPU_BUCKET_FRONT, flat);

        ae_wr8(r, draw_mode + AE_GPU_PACKET_CODE, 1);
        ae_wr32(r, draw_mode + AE_GPU_PACKET_WORD, 0xE1000200u);
        (void)link_gpu_packet(r, flat_bucket, AE_GPU_BUCKET_FRONT, draw_mode);
        cursor += AE_GPU_PACKET_SIZE + 8u;
    }

    if (draw_textured) {
        int32_t left = sub32(center_x, half_size);
        int32_t right = add32(center_x, half_size);
        int32_t top = sub32(center_y, half_size);
        int32_t bottom = add32(center_y, half_size);
        uint32_t upper_left = pack_xy16(top, left);
        uint32_t upper_right = pack_xy16(top, right);
        uint32_t lower_left = pack_xy16(bottom, left);
        uint32_t lower_right = pack_xy16(bottom, right);

        for (iteration = 0; iteration < 2u; iteration++) {
            uint32_t textured = cursor;
        ae_wr8(r, textured + AE_GPU_POLY_CODE, 9);
        ae_wr32(r, textured + AE_GPU_POLY_COLOR, 0x2F000000u);
        ae_wr32(r, textured + AE_GPU_POLY_XY0, upper_left);
        ae_wr32(r, textured + AE_GPU_POLY_TEX0, 0x7D2D0040u);
        ae_wr32(r, textured + AE_GPU_POLY_XY1, upper_right);
        ae_wr32(r, textured + AE_GPU_POLY_TEX1, 0x001B006Fu);
        ae_wr32(r, textured + AE_GPU_POLY_XY2, lower_left);
        ae_wr16(r, textured + AE_GPU_POLY_UV2, 0x3040u);
        ae_wr32(r, textured + AE_GPU_POLY_XY3, lower_right);
        ae_wr16(r, textured + AE_GPU_POLY_UV3, 0x306Fu);
            (void)link_gpu_packet(r, textured_bucket, AE_GPU_BUCKET_BACK, textured);
            cursor += AE_GPU_POLY_BYTES;
        }
    }
    ae_wr32(r, AE_G_CAMERA_GADGET_PRIM_CURSOR, cursor);
    ae_wr8(r, AE_G_CAMERA_GADGET_PRIM_COUNT,
           ae_rd8(r, AE_G_CAMERA_GADGET_PRIM_COUNT) + 1u);
}

/* 8004AF78 */
void ae_CameraFollowGadgetVariant(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t player = AE_G_PLAYER;
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t player_state = ae_rd16(r, player + AE_PLAYER_STATE);
    uint32_t mode = 1;
    uint32_t aim_mode = ae_rd8(r, state + AE_CAM_G_AIM_MODE);
    uint32_t special_state = (player_state >= 0x82u && player_state <= 0x84u) ||
                             player_state == 0x86u;
    int32_t aim_controls = 0;
    int32_t screen_x = 0;
    int32_t screen_y = 0;
    int32_t flat_x = 0;
    int32_t flat_y = 0;
    int32_t half_size;

    if (call_camera(r, h, h ? h->StageExitPending : NULL,
                    "StageExitPending", 0, 0, 0, 0) != 0)
        return;

    if (aim_mode != 0) {
        screen_x = 0xC0;
        screen_y = 0x78;
        if (special_state)
            mode = 2;
    } else {
        if (special_state) {
            mode = 2;
            aim_controls = 1;
            if (ae_rd16(r, AE_G_RIGHT_STICK_MAG) >= 51u &&
                call_camera(r, h, h ? h->BossKindForArea : NULL,
                            "BossKindForArea", 0, 0, 0, 0) == 0) {
                int32_t stick_x = ae_rds8(r, AE_G_PLAYER_SCRATCH_X);
                int32_t yaw = slot_s32(r, slot, AE_CAM_YAW);
                int32_t yaw_step = stick_x > 0 ? 0x0FEA : 0x1016;
                int32_t adjusted_yaw = add32(yaw, yaw_step);
                int32_t stick_y = ae_rds8(r, AE_G_PLAYER_SCRATCH_Y);
                int32_t focus_y = ae_rds16(r, AE_SCRATCH_BASE + AE_CAM_SCR_DESIRED_EYE + 2u);
                int32_t camera_distance = ae_rd16(r,
                    slot_addr(r, slot, AE_CAM_DISTANCE));

                slot_set_s32(r, slot, AE_CAM_YAW, adjusted_yaw % 0x1000);
                ae_wr8(r, slot_addr(r, slot, AE_CAM_STILL_FRAMES), 0);

                if (stick_y < -0x50) {
                    focus_y = s16_bits(add32(focus_y, camera_distance));
                    if (focus_y > ae_rds32(r, player + AE_PLAYER_POSITION_Y) - 0x40)
                        focus_y = (int32_t)(ae_rd16(r, player + AE_PLAYER_POSITION_Y) - 0x40u);
                    ae_wr16(r, AE_SCRATCH_BASE + AE_CAM_SCR_DESIRED_EYE + 2u,
                            (uint32_t)focus_y);
                } else if (stick_y >= 0x51) {
                    focus_y = sub32(focus_y, camera_distance);
                    ae_wr16(r, AE_SCRATCH_BASE + AE_CAM_SCR_DESIRED_EYE + 2u,
                            (uint32_t)focus_y);
                }
            }
        } else {
            mode = 0;
        }
    }

    if (mode == 1u)
        ae_wr8(r, AE_G_CAMERA_GADGET_PRIM_COUNT, 19);
    if (mode == 0u)
        return;

    if (aim_mode == 0u && special_state) {
        uint32_t matrix = AE_G_CAMERA_MATRIX;
        uint32_t point = AE_CAMERA_TEMP_MATRIX;
        uint32_t screen = AE_CAMERA_TEMP_MATRIX + 8u;
        uint32_t depth = AE_CAMERA_TEMP_MATRIX + 12u;
        int32_t facing_x = ae_rds16(r, player + AE_PLAYER_FACING_X);
        int32_t facing_z = ae_rds16(r, player + AE_PLAYER_FACING_Z);
        int32_t x = add32(ae_rds16(r, player + AE_PLAYER_POSITION_X),
                          trunc_div3(facing_x));
        int32_t y = sub32(ae_rds16(r, player + AE_PLAYER_POSITION_Y), 0x80);
        int32_t z = add32(ae_rds16(r, player + AE_PLAYER_POSITION_Z),
                          trunc_div3(facing_z));

        (void)call_camera(r, h, h ? h->SetRotMatrix : NULL,
                          "SetRotMatrix", matrix, 0, 0, 0);
        (void)call_camera(r, h, h ? h->SetTransMatrix : NULL,
                          "SetTransMatrix", matrix, 0, 0, 0);
        ae_wr16(r, point, (uint32_t)x);
        ae_wr16(r, point + 2u, (uint32_t)y);
        ae_wr16(r, point + 4u, (uint32_t)z);
        (void)call_camera(r, h, h ? h->RotTransPers : NULL,
                          "RotTransPers", point, screen, depth, depth);
        screen_x = ae_rds16(r, screen);
        screen_y = ae_rds16(r, screen + 2u);

        {
            int32_t ground_height = call_ground(r, h, x, y, z);
            ae_wr16(r, point + 2u, (uint32_t)ground_height);
        }
        (void)call_camera(r, h, h ? h->RotTransPers : NULL,
                          "RotTransPers", point, screen, depth, depth);
        flat_x = ae_rds16(r, screen);
        flat_y = ae_rds16(r, screen + 2u);
    }

    if (mode == 2) {
        uint32_t bucket_value = ae_rd8(r, AE_G_CAMERA_GADGET_PRIM_COUNT) & 0x1Fu;
        half_size = bucket_value < 16u ? (int32_t)bucket_value + 12 :
                    (int32_t)(31u - bucket_value) + 12;
        if (aim_controls != 0)
            half_size = floor_shr(half_size, 1);
    } else {
        half_size = 24;
    }
    camera_emit_gadget_primitives(r, screen_x, screen_y, flat_x, flat_y, half_size,
                                  aim_mode == 0u && special_state,
                                  mode != 0u);
}

/* 80048454 */
void ae_CameraKindAimUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t player = AE_G_PLAYER;
    uint32_t buttons = ae_rd32(r, AE_G_GAMEPAD_PRESSED) &
                       ae_rd32(r, AE_G_GAMEPAD_CHANGED);
    uint32_t aim_context = ae_rd8(r, AE_G_CAMERA_AIM_CONTEXT);

    if (aim_context == 1u) {
        if ((buttons & 0x5u) != 0) {
            int32_t saved_distance = ae_rds16(r, slot_addr(r, slot, AE_CAM_FOLLOW_DIST));
            (void)call_camera(r, h, h ? h->AimRequestClear : NULL,
                              "AimRequestClear", 0, 0, 0, 0);
            (void)call_camera(r, h, h ? h->CameraFixedModeOff : NULL,
                              "CameraFixedModeOff", slot, 0, 0, 0);
            slot_set_s32(r, slot, AE_CAM_DISTANCE, saved_distance);
            (void)call_camera(r, h, h ? h->PlaySfx : NULL,
                              "PlaySfx", 5, 0, 0, 0);
        }
    } else if (call_camera(r, h, h ? h->StageExitPending : NULL,
                           "StageExitPending", 0, 0, 0, 0) == 0) {
        if ((buttons & 0x1u) != 0) {
            uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
            uint32_t player_mode = ae_rd16(r, AE_G_PLAYER_MODE);
            int32_t gadget = ae_rds8(r, player + AE_PLAYER_CURRENT_GADGET);
            uint32_t player_state = ae_rd16(r, player + AE_PLAYER_STATE);

            if (area == 0x13u || area == 0x24u) {
                (void)call_camera(r, h, h ? h->BossAimSfx : NULL,
                                  "BossAimSfx", 0, 0, 0, 0);
            } else if (player_mode == 2u || player_mode == 3u) {
                (void)call_camera(r, h, h ? h->BossAimSfx : NULL,
                                  "BossAimSfx", 0, 0, 0, 0);
            } else if (player_mode == 0u || player_mode >= 4u) {
                if (gadget == 2) {
                    /* The radar gadget reserves the aim press for its own mode. */
                } else if (gadget == 3) {
                    if (ae_rd8(r, state + AE_CAM_G_AIM_MODE) == 1u) {
                        (void)call_camera(r, h, h ? h->AimModeLeave : NULL,
                                          "AimModeLeave", 0, 0, 0, 0);
                    } else if (player_state == (uint32_t)gadget || player_state == 0u ||
                               player_state == 5u || (player_state >= 8u && player_state < 11u) ||
                               (player_state >= 16u && player_state < 18u) ||
                               player_state == 19u ||
                               (player_state >= 0x80u && player_state < 0x88u)) {
                        (void)call_camera(r, h, h ? h->AimModeEnter : NULL,
                                          "AimModeEnter", 0, 0, 0, 0);
                    }
                } else if (player_state >= 2u && player_state != 0x46u) {
                    (void)call_camera(r, h, h ? h->BossAimSfx : NULL,
                                      "BossAimSfx", 0, 0, 0, 0);
                } else {
                    (void)call_camera(r, h, h ? h->AimRequestStart : NULL,
                                      "AimRequestStart", 0, 0, 0, 0);
                }
            }
        }

        if ((buttons & 0x4u) != 0) {
            uint32_t active_slot;
            if (ae_rd8(r, state + AE_CAM_G_AIM_MODE) == 1u) {
                (void)call_camera(r, h, h ? h->AimModeLeave : NULL,
                                  "AimModeLeave", 0, 0, 0, 0);
            } else if (ae_rd8(r, state + AE_CAM_G_MODE_TIMER) != 1u &&
                       ae_rd8(r, state + AE_CAM_G_REMOVE_GUARD) == 0u &&
                       ae_rd8(r, state + AE_CAM_G_UNK_3E) == 0u &&
                       ae_rd16(r, AE_G_PLAYER_MODE) != 1u &&
                       call_camera(r, h, h ? h->PlayerCanExitStage : NULL,
                                   "PlayerCanExitStage", 0, 0, 0, 0) != 0) {
                active_slot = camera_slot_by_index(
                    r, ae_rd8(r, state + AE_CAM_G_FOLLOW_INDEX));
                if (ae_rd8(r, slot_addr(r, active_slot, AE_CAM_MODE_TIMER)) < 15u) {
                    ae_CameraKindAim_SnapToFocus(r, h, active_slot);
                    ae_wr8(r, slot_addr(r, active_slot, AE_CAM_FOLLOW_FLAG_B), 0);
                    (void)call_camera(r, h, h ? h->PlaySfx : NULL,
                                      "PlaySfx", 4, 0, 0, 0);
                }
            }
        }
    }

    ae_CameraFollowAimedVariant(r, h, slot);
}

/* 8004875C */
void ae_CameraKindFixedUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot)
{
    uint32_t state = AE_G_CAMERA_STATE;
    uint32_t scratch = AE_SCRATCH_BASE;

    if (ae_rd8(r, state + AE_CAM_G_FIXED_ACTIVE) == 1u) {
        int32_t radar_usable = call_camera(r, h, h ? h->RadarUsable : NULL,
                                           "RadarUsable", 0, 0, 0, 0);
        if (radar_usable != 0) {
            uint32_t target = ae_rd32(r, AE_G_CAMERA_RADAR_TARGET);
            if (call_camera(r, h, h ? h->MonkeyFlag248Set : NULL,
                            "MonkeyFlag248Set", target, 0, 0, 0) == 0) {
                (void)call_camera(r, h, h ? h->CameraFixedModeOff : NULL,
                                  "CameraFixedModeOff", 0, 0, 0, 0);
                return;
            }
        } else {
            (void)call_camera(r, h, h ? h->CameraFixedModeOff : NULL,
                              "CameraFixedModeOff", 0, 0, 0, 0);
            return;
        }
    }

    if ((ae_rd32(r, AE_G_GAMEPAD_PRESSED) &
         ae_rd32(r, AE_G_GAMEPAD_CHANGED) & 1u) != 0 &&
        call_camera(r, h, h ? h->RadarUsable : NULL,
                    "RadarUsable", 0, 0, 0, 0) != 0) {
        uint32_t fixed_state;
        (void)call_camera(r, h, h ? h->AimRequestClear : NULL,
                          "AimRequestClear", 0, 0, 0, 0);
        fixed_state = ae_rd8(r, state + AE_CAM_G_FIXED_ACTIVE);
        if (fixed_state == 0u) {
            (void)call_camera(r, h, h ? h->CameraRadarFocusSelect : NULL,
                              "CameraRadarFocusSelect", 0, 0, 0, 0);
            (void)call_camera(r, h, h ? h->AimModeLeave : NULL,
                              "AimModeLeave", 0, 0, 0, 0);
            if (ae_rd32(r, AE_G_CAMERA_RADAR_TARGET) != 0) {
                ae_wr8(r, state + AE_CAM_G_FIXED_ACTIVE, 1);
                (void)call_camera(r, h, h ? h->ScreenFadeSet : NULL,
                                  "ScreenFadeSet", 3, 200, (uint32_t)-60, 0);
                ae_wr8(r, state + AE_CAM_G_MODE_TIMER, 0);
                (void)call_camera(r, h, h ? h->PlaySfx : NULL,
                                  "PlaySfx", 4, 0, 0, 0);
                (void)call_camera(r, h, h ? h->CameraRadarFocusUpdate : NULL,
                                  "CameraRadarFocusUpdate", 0, 0, 0, 0);
                ae_CameraSlotPlaceBehindPlayer(r, h, slot,
                                               scratch + AE_CAM_SCR_PLAYER_X,
                                               0x333);
                ae_wr32(r, slot_addr(r, slot, AE_CAM_YAW),
                        (uint32_t)call_atan(r, h,
                            neg32(ae_rds16(r, scratch + AE_CAM_SCR_DIRECTION)),
                            neg32(ae_rds16(r, scratch + AE_CAM_SCR_DIRECTION + 4u))));
                slot_set_s32(r, slot, AE_CAM_DISTANCE, 0x460);
                slot_set_s16(r, slot, AE_CAM_FOLLOW_DIST, 0x460);
                ae_wr8(r, slot_addr(r, slot, AE_CAM_MODE_TIMER), 0);
            }
        } else if (fixed_state == 1u) {
            (void)call_camera(r, h, h ? h->CameraFixedModeOff : NULL,
                              "CameraFixedModeOff", 0, 0, 0, 0);
            (void)call_camera(r, h, h ? h->ScreenFadeSet : NULL,
                              "ScreenFadeSet", 3, 200, (uint32_t)-60, 0);
            (void)call_camera(r, h, h ? h->PlaySfx : NULL,
                              "PlaySfx", 5, 0, 0, 0);
        } else if (fixed_state == 2u) {
            ae_CameraKindFixed_Nop(r, h, slot);
        }
    }

    if (ae_rd8(r, state + AE_CAM_G_FIXED_ACTIVE) == 1u) {
        int32_t ground;
        (void)call_camera(r, h, h ? h->CameraRadarFocusUpdate : NULL,
                          "CameraRadarFocusUpdate", 0, 0, 0, 0);
        ground = call_ground(r, h,
            ae_rds16(r, scratch + AE_CAM_SCR_PLAYER_X),
            ae_rds16(r, scratch + AE_CAM_SCR_CAMERA_POSITION_Y),
            ae_rds16(r, scratch + AE_CAM_SCR_PLAYER_Z));
        ae_wr32(r, scratch + AE_CAM_SCR_GROUND_REF, (uint32_t)s16_bits((uint32_t)ground));
        ae_CameraFollowUpdate(r, h, slot, 1);
        (void)call_camera(r, h, h ? h->HudDrawMonkeyInfo : NULL,
                          "HudDrawMonkeyInfo", ae_rd32(r, AE_G_CAMERA_RADAR_TARGET),
                          0, 0, 0);
    }
}

