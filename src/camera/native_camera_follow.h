/* Portable CameraFollowUpdate (0x800495F4) with CameraSlotUpdateYaw (0x80048DD0)
 * and CameraSlotAimAtFocus (0x80047360). See native_camera_follow.md.
 * Builds on native_camera.h / native_camera_math.h / native_camera_collision.h.
 * The PSX scratchpad and control globals the original reads and writes are
 * carried in AeCameraFollowFrame as byte arrays (little endian). No global
 * state; no dependencies beyond the C99 standard headers. */
#ifndef AE_NATIVE_CAMERA_FOLLOW_H
#define AE_NATIVE_CAMERA_FOLLOW_H

#include <stdint.h>

#include "native_camera.h"
#include "native_camera_collision.h"
#include "native_camera_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bits ORed into AeCameraFollowFrame.status when a call returns 0. */
enum {
    AE_CAMERA_FOLLOW_MISSING_CALLBACK  = 1,  /* null slot/tables/queries, tables not loaded, or a callback is null */
    AE_CAMERA_FOLLOW_UNSUPPORTED_GADGET = 2, /* original calls CameraFollowGadgetVariant (not ported) */
    AE_CAMERA_FOLLOW_MATH_DOMAIN       = 4,  /* atan failed or an original division would trap */
    AE_CAMERA_FOLLOW_UNSUPPORTED_MODE  = 8   /* mode other than 0 / 1 */
};

typedef struct AeCameraFollowFrame {
    uint8_t scratch[0x80];   /* PSX scratchpad 0x1F800000..0x1F80007F */
    uint8_t controls[0x50];  /* globals 0x800E68D0..0x800E691F (index = address - 0x800E68D0) */
    uint32_t state_flags;    /* g_Player.stateFlags, 0x800EC250 (full word) */
    uint32_t state;          /* g_Player.state, 0x800EC23E (original reads a halfword) */
    uint32_t player_mode;    /* g_PlayerMode, 0x800F44A0 (halfword) */
    uint32_t current_area;   /* g_CurrentArea, 0x800F4476 (halfword) */
    uint32_t current_gadget; /* signed byte at 0x800EC2D2; only the low byte is used, as int8 */
    uint32_t stage_entry_flags; /* g_StageEntryFlags, 0x800F4450 (word) */
    uint32_t pad_held;       /* g_PadHeld */
    uint32_t right_stick_word; /* word at 0x800E54A0; low 16 bits = g_RStickMag */
    uint32_t projected_sxy;  /* RotTransPers screen XY of (scratch 0x10, 0x1A - 0x80, 0x14) with the previous view */
    uint32_t status;         /* AE_CAMERA_FOLLOW_* bits, ORed on failure */
    int32_t can_exit;        /* PlayerCanExitStage() return value */
    int32_t submerged;       /* PlayerIsSubmerged() return value */
    int32_t rccar_active;    /* CameraRcCarActive() return value */
    int8_t stick_x;          /* signed byte at 0x800E53C2 */
    int8_t stick_y;          /* signed byte at 0x800E53C3 */
} AeCameraFollowFrame;

/* CameraFollowUpdate. mode 0 = normal follow, 1 = the original's mode 1; other
 * modes fail closed. The caller clears frame->controls[0x47] once per frame (the
 * original's DAT_800E6917 guard): if it is already nonzero the call returns 1
 * and changes nothing, otherwise it is set to 1.
 *
 * Returns 1 on success. Returns 0 with slot, scratch and controls unchanged
 * (only frame->status gets bits) when: a pointer / table / callback required
 * (normalize, sqrt, ground, loaded math tables) is missing, the mode is not 0 / 1,
 * the original would call CameraFollowGadgetVariant (current gadget 3 and
 * controls[0x46] != 1), or a runtime math failure occurs (state is rolled back).
 * The missing-callback and mode checks run before the guard check; the gadget
 * check runs after it (the original only reaches the call when the guard is 0). */
int ae_camera_follow_update(AeCameraSlot *slot, AeCameraFollowFrame *frame,
                            const AeCameraMathTables *tables,
                            const AeCameraQueries *queries, int mode);

/* CameraSlotUpdateYaw: includes the AimAtFocus call. Returns 1, or 0 (nothing
 * changed, status bits set) on missing pointers / callbacks or a failed atan. */
int ae_camera_slot_update_yaw(AeCameraSlot *slot, AeCameraFollowFrame *frame,
                              const AeCameraMathTables *tables,
                              const AeCameraQueries *queries);

/* CameraSlotAimAtFocus: needs queries->normalize. Writes the full 32 bit results
 * to scratch 0x78 and 0x74. Returns 1, or 0 with status bits on missing input. */
int ae_camera_slot_aim_at_focus(const AeCameraSlot *slot, AeCameraFollowFrame *frame,
                                const AeCameraQueries *queries);

#ifdef __cplusplus
}
#endif

#endif
