#ifndef AE_PAD_H
#define AE_PAD_H
/*
 * Controller input of Ape Escape (PS1): PadUpdate (0x8001325C) and its helpers, ported instruction for
 * instruction onto a byte-exact RAM image (AeRam, src/core/ae_ram.h).  Diffed against the original MIPS code by
 * tools/mips/check_player_input.py (byte-identical RAM, return values and hook-call sequences).
 *
 * Which bytes are which stick (PSX analog packet, g_PadBuf1 at 0x800E52D0, from PadSampleStickHistory):
 *     byte 0   status (0xFF = no pad)       byte 1  id (low nibble < 3 = digital pad: sticks are forced to 0x80)
 *     byte 2/3 buttons, active low          byte 4  right stick X    byte 5  right stick Y
 *                                           byte 6  left  stick X    byte 7  left  stick Y
 *   history sample (4 x s8): { raw4 - 0x80, 0x7F - raw5, raw6 - 0x80, 0x7F - raw7 }  (y up positive)
 * PadUpdate calls the first pair "L" (g_LStick*) and the second pair "R" (g_RStick*).  So, in this project's names
 * (and in the original code): g_LStick* is the GADGET stick = the physical RIGHT stick (bytes 4 / 5) and g_RStick* is
 * the MOVEMENT stick = the physical LEFT stick (bytes 6 / 7), whose outputs feed PlayerScratch.leftStick*.
 *
 * Globals live at their original addresses (all in the stick block 0x800E5320 + n):
 */
#include "ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_PAD_BUF1 0x800E52D0u              /* g_PadBuf1: 8 raw bytes of port 1 (status, id, buttons x2, 4 sticks) */
#define AE_PAD_BUF2 0x800E52F2u              /* g_PadBuf2 */
#define AE_PAD_STICK_HISTORY 0x800E5320u     /* 40 samples x 4 bytes (s8): L x, L y, R x, R y (ring, newest = g_StickHistoryPos) */
#define AE_PAD_STICK_LATEST 0x800E53C0u      /* DAT_800e53c0: copy of the newest sample (u32) */
#define AE_PAD_STICK_DONE 0x800E53C4u        /* g_StickHistoryDone: 40 x u16, 0 = not processed yet, else the gesture class */
#define AE_PAD_STICK_LMAG_HIST 0x800E5414u   /* 40 x u8: g_LStickMag per sample */
#define AE_PAD_STICK_FLICK_HIST 0x800E543Cu  /* 40 x u8: g_LStickFlickState per sample */
#define AE_PAD_ID1 0x800E5464u               /* u16 (buf1[0] << 8 | buf1[1]) */
#define AE_PAD_ID2 0x800E5466u
#define AE_PAD_RUMBLE_TIMER0 0x800E546Au     /* u8 */
#define AE_PAD_RUMBLE_TIMER1 0x800E546Bu     /* u8 */
#define AE_PAD_HELD 0x800E5478u              /* g_PadHeld (u32, 0xFFFFxxxx: the upper half is set by the inversion) */
#define AE_PAD2_HELD 0x800E547Cu
#define AE_PAD_CHANGED 0x800E5480u
#define AE_PAD2_CHANGED 0x800E5484u
#define AE_PAD_REPEAT 0x800E5488u
#define AE_PAD2_REPEAT 0x800E548Cu
#define AE_PAD_HISTORY_POS 0x800E5490u       /* g_StickHistoryPos (u32, 0..39) */
#define AE_PAD_DUALSHOCK_STATE 0x800E5494u   /* g_PadDualShockState */
#define AE_PAD_RSTICK_PUSH 0x800E5498u       /* g_RStickPushState u16: 0 / 2 / 4 / 8 (movement stick; PlayerScratch.leftStickClass) */
#define AE_PAD_LSTICK_FLICK 0x800E549Au      /* g_LStickFlickState u16: 0, 4 push, 8 hard flick, 0x10 / 0x20 spin, 0x80 held (gadget stick) */
#define AE_PAD_GESTURE_EVENTS 0x800E549Eu    /* g_StickGestureEvents u16: bit 2 push, bit 3 hard flick (cleared each PadUpdate) */
#define AE_PAD_RSTICK_MAG 0x800E54A0u        /* g_RStickMag u16 0..0x7F (movement) */
#define AE_PAD_LSTICK_MAG 0x800E54A2u        /* g_LStickMag u16 0..0x7F (gadget) */
#define AE_PAD_RSTICK_DIR 0x800E54A4u        /* g_RStickDir SVECTOR (x, 0, z) direction * 0x1000 */
#define AE_PAD_LSTICK_DIR 0x800E54ACu        /* g_LStickDir SVECTOR */
#define AE_PAD_RSTICK_CAMDIR 0x800E54B4u     /* g_RStickCamDir SVECTOR: movement direction rotated by g_CameraRotMatrix (flags bit 0) */
#define AE_PAD_LSTICK_CAMDIR 0x800E54BCu     /* DAT_800e54bc SVECTOR: gadget direction rotated by g_CameraRotMatrix */
#define AE_PAD_LSTICK_SPIN 0x800E54C4u       /* g_LStickSpin s32: cross product of the last two gadget samples */
#define AE_PAD_RSTICK_SPIN 0x800E54C8u       /* g_RStickSpin s32 */
#define AE_PAD_LCAM_FWD 0x800E54CCu          /* DAT_800e54cc s16: gadget stick along the player's facing (PlayerScratch.rightStickCam[0]) */
#define AE_PAD_LCAM_SIDE 0x800E54CEu         /* DAT_800e54ce s16: gadget stick across the facing ([1]) */
#define AE_PAD_RCAM_FWD 0x800E54D0u          /* DAT_800e54d0 s16: movement stick along the facing (PlayerScratch.leftStickCam[0]) */
#define AE_PAD_RCAM_SIDE 0x800E54D2u         /* DAT_800e54d2 s16: movement stick across the facing ([1]) */
#define AE_PAD_STICK_ROTATION 0x800E54D4u    /* DAT_800e54d4 s16: gadget spin amount of the 0x10 / 0x20 / 0x80 states */
#define AE_PAD_REPEAT_TIMER 0x800E54D8u      /* s16 */
#define AE_PAD2_REPEAT_TIMER 0x800E54DAu
#define AE_PAD_TRAVEL_L 0x800E54DCu          /* DAT_800e54dc s16: sum of |delta| over the history, gadget stick (PlayerScratch.stickTravelR) */
#define AE_PAD_TRAVEL_R 0x800E54DEu          /* DAT_800e54de s16: movement stick (stickTravelL) */
#define AE_PAD_AIM_ALT 0x800E54E0u           /* DAT_800e54e0 (u32): non-zero = gadget cam values come from g_LStickDir (set 0 by InitSystem) */
#define AE_PAD_DEMO_FLAG 0x800A8A58u         /* DAT_800a8a58: demo playback active */
#define AE_G_PLAYER_FACING 0x800EC240u       /* g_Player.facing (SVECTOR; PadUpdate reads vx and vz) */
#define AE_G_CAMERA_ROT_MATRIX 0x800EC330u   /* g_CameraRotMatrix (MATRIX: 3 x 3 s16, then s32[3]) */
#define AE_G_CURRENT_AREA 0x800F4476u
#define AE_G_ENTRY_INDEX 0x800F447Au

/* PadUpdate flag bits (its argument) */
#define AE_PADUPDATE_CAMERA_RELATIVE 1u      /* bit 0: camera-relative stick vectors */
#define AE_PADUPDATE_STICK_BUTTONS 2u        /* bit 1: StickToButtons replaces the port 1 button word */

/* Functions outside this module (the pad driver, demo playback, stage requests).  A NULL hook that the original
 * calls sets the AeRam error flag ("hook not provided") and returns 0: fail closed. */
typedef struct AePadHooks {
    void *user;
    int32_t (*pad_get_state)(void *user, AeRam *r, int32_t port);                       /* PadGetState 8002B52C */
    int32_t (*pad_info_mode)(void *user, AeRam *r, int32_t port, int32_t a, int32_t b); /* PadInfoMode 8002B5F8 */
    void (*pad_setup_dualshock)(void *user, AeRam *r, int32_t arg);                      /* PadSetupDualShock 80012CA0 */
    int32_t (*pad_demo_step)(void *user, AeRam *r);                                      /* PadDemoPlaybackStep 80013128 */
    void (*stage_request)(void *user, AeRam *r, int32_t a, int32_t area, int32_t entry); /* StageRequest 8005D100 */
    uint32_t (*stick_to_buttons)(void *user, AeRam *r);                                  /* StickToButtons 800140B4 */
    void (*pad_rumble_reset)(void *user, AeRam *r, int32_t port);                        /* PadRumbleReset 80014208 */
} AePadHooks;

/* ---- SDK routines (ported from the listings) ---------------------------------------------------- */
/* VectorNormalSS 800301E0: in / out are SVECTOR addresses (3 x s16, in place allowed). Returns v0 = x*x + y*y + z*z
 * (the SUM OF SQUARES, not a length).  A signed overflow of the sum traps in the original: the port flags it. */
int32_t ae_pad_VectorNormalSS(AeRam *r, uint32_t in, uint32_t out);
/* VectorNormal 800301B0: VECTOR (3 x s32 words, x / y / z used as their low 16 bits), out = 3 x s32. Returns v0. */
int32_t ae_pad_VectorNormal(AeRam *r, uint32_t in, uint32_t out);
/* ApplyMatrixSV 800300B0: out = (M * in) >> 12 per component (GTE MVMVA sf=1, no translation), saturated to s16. Returns out. */
uint32_t ae_pad_ApplyMatrixSV(AeRam *r, uint32_t matrix, uint32_t in, uint32_t out);
/* QuadrantFromSigns 8001413C: 0..3 from the signs of two values. */
int32_t ae_pad_QuadrantFromSigns(int32_t a, int32_t b);

/* ---- pad driver ---------------------------------------------------------------------------------- */
/* PadNormalizeSticks 800130E0: buf[0] == 0xFF -> buttons 0xFF; digital id -> sticks 0x80. Modifies the buffer in RAM. */
void ae_pad_PadNormalizeSticks(AeRam *r, uint32_t buf);
/* PadSampleStickHistory 8001300C (called by FrameEndUpdate, at the END of a frame): pushes the stick bytes of port 1
 * into the ring; the newest sample is processed by the next PadUpdate. */
void ae_pad_PadSampleStickHistory(AeRam *r);
/* PadUpdate 8001325C. flags: AE_PADUPDATE_*. Returns nothing (the original's $v0 is the history word, unused). */
void ae_pad_PadUpdate(AeRam *r, const AePadHooks *h, uint32_t flags);

/* CameraLookAt 80046670, the part that builds g_CameraRotMatrix (its 4th argument): a pure yaw matrix from the XZ
 * direction eye -> target:  d = (target.x - eye.x, 0, target.z - eye.z) (z = 0x1000 when both are 0), n = VectorNormal(d),
 * m = {{n.z, 0, n.x}, {0, 0x1000, 0}, {-n.x, 0, n.z}}.  Roll (the 5th argument) and the view matrix do not touch it.
 * eye / target are s16[3] (the original g_CameraEye / g_CameraTarget). Writes the matrix at `matrix`
 * (AE_G_CAMERA_ROT_MATRIX); the translation part is not written. The second VectorNormal of CameraLookAt (with the y
 * difference) runs before any store, so a sum-of-squares overflow there also means no matrix is written (flagged). */
void ae_pad_CameraRotMatrix(AeRam *r, const int16_t eye[3], const int16_t target[3], uint32_t matrix);

#ifdef __cplusplus
}
#endif
#endif
