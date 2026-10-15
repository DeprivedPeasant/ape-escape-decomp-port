/* Portable camera slot leaf routines (see native_camera.md).
 * The slot is the exact 0xB0-byte original CameraSlot, accessed through
 * explicit little-endian helpers so there is no struct padding or alignment
 * dependence. No dependencies beyond the C99 standard headers. */
#ifndef AE_NATIVE_CAMERA_H
#define AE_NATIVE_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_CAMERA_SLOT_SIZE 0xB0u

typedef struct AeCameraSlot {
    uint8_t raw[AE_CAMERA_SLOT_SIZE];
} AeCameraSlot;

/* Byte offsets inside the slot (from the original code's accesses). */
enum {
    AE_CAM_SCRIPT_ACTIVE = 0x03,  /* u8 */
    AE_CAM_FOLLOW_DIST_H = 0x04,  /* s16 */
    AE_CAM_EYE_HEIGHT    = 0x06,  /* s16 */
    AE_CAM_TARGET_HEIGHT = 0x08,  /* s16 */
    AE_CAM_EYE           = 0x0A,  /* s16[3] */
    AE_CAM_TARGET        = 0x12,  /* s16[3] */
    AE_CAM_LAST_POS      = 0x1A,  /* s16[3] */
    AE_CAM_UNK_2C        = 0x2C,  /* s32 */
    AE_CAM_STILL_FRAMES  = 0x30,  /* u8 */
    AE_CAM_YAW_HOLD      = 0x31,  /* u8 */
    AE_CAM_MODE_TIMER    = 0x32,  /* u8 */
    AE_CAM_FIXED_ACTIVE  = 0x33,  /* u8 */
    AE_CAM_CLAMP_STATE   = 0x34,  /* u8 */
    AE_CAM_CLAMP_TIMER   = 0x35,  /* u8 */
    AE_CAM_CLAMP_COUNT   = 0x36,  /* u8 */
    AE_CAM_CLAMP_FLAG    = 0x37,  /* u8 */
    AE_CAM_DIST          = 0x38,  /* s32 */
    AE_CAM_YAW           = 0x40,  /* s32 */
    AE_CAM_SAVED_YAW     = 0x44,  /* s32 */
    AE_CAM_EYE_VEL       = 0x50,  /* s16[3] */
    AE_CAM_TARGET_VEL    = 0x58,  /* s16[3] */
    AE_CAM_ORBIT_X       = 0x6C,  /* s16 */
    AE_CAM_ORBIT_Y       = 0x6E,  /* s16 */
    AE_CAM_FOLLOW_FLAG_A = 0x80,  /* u8 */
    AE_CAM_FOLLOW_FLAG_B = 0x81,  /* u8 */
    AE_CAM_SCRIPT        = 0x84,  /* s32 */
    AE_CAM_SCRIPT_TIMER  = 0x88,  /* s16 */
    AE_CAM_SCRIPT_STEP   = 0x8A,  /* s16 */
    AE_CAM_SCRIPT_STATE  = 0x90   /* u8[3] */
};

/* Little-endian accessors. Precondition: off + size of the access <= 0xB0
 * (not checked). Reads of signed values never rely on implementation-defined
 * conversions; writes store the value modulo 2^16 / 2^32. */
uint16_t ae_camera_u16(const AeCameraSlot *s, size_t off);
int16_t  ae_camera_i16(const AeCameraSlot *s, size_t off);
int32_t  ae_camera_i32(const AeCameraSlot *s, size_t off);
void     ae_camera_set16(AeCameraSlot *s, size_t off, int32_t value);
void     ae_camera_set32(AeCameraSlot *s, size_t off, int32_t value);

/* CameraSlotResetFollow (0x800493D4). original_yaw is the result of the
 * original Atan2Safe(-facing[0], -facing[2]), supplied by the caller. */
void ae_camera_reset_follow(AeCameraSlot *s, const int32_t pos[3],
                            const int16_t facing[3], int32_t original_yaw);

/* CameraSlotClearScript (0x8004C2E4), slot-resident part only. */
void ae_camera_clear_script(AeCameraSlot *s);

/* CameraSlotInitFollow (0x80047104). eye8 / target8 are copied verbatim
 * (8 bytes each, including the two padding bytes) to offsets 0x0A / 0x12. */
void ae_camera_init_follow(AeCameraSlot *s, int32_t kind, const uint8_t eye8[8],
                           const uint8_t target8[8], const int32_t pos[3],
                           const int16_t facing[3], int32_t original_yaw);

/* CameraSlotSmoothEye (0x80047464) / CameraSlotSmoothTarget (0x80047558).
 * desired[] are the scratch values; div[] the per-axis divisors.
 * Returns 1 on success, 0 if the original would trap (divisor 0, or
 * INT32_MIN / -1). Axes before the failing one stay written. */
int ae_camera_smooth_eye(AeCameraSlot *s, const int16_t desired[3], const int32_t div[3]);
int ae_camera_smooth_target(AeCameraSlot *s, const int16_t desired[3], const int32_t div[3]);

/* CameraSlotStickOrbit (0x8004923C). can_exit is PlayerCanExitStage()'s
 * result; fixed_active the byte at 0x800E6908; stick_mag g_RStickMag;
 * stick_x / stick_y the signed bytes at 0x800E53C2 / 0x800E53C3; held
 * g_PadHeld; player_y the halfword at scratch 0x1F80001A. */
void ae_camera_stick_orbit(AeCameraSlot *s, int can_exit, int fixed_active,
                           uint16_t stick_mag, int8_t stick_x, int8_t stick_y,
                           uint32_t held, int16_t player_y);

#ifdef __cplusplus
}
#endif

#endif
