#ifndef AE_NATIVE_PLAYER_PAD_H
#define AE_NATIVE_PLAYER_PAD_H
/*
 * Glue between the exact controller port (src/core/ae_pad.c, PadUpdate on a RAM image) and the native player:
 * owns the AeRam image that holds the pad globals, feeds it the raw pad bytes, the player's facing and the camera
 * rotation matrix, runs PadUpdate and reads the values PlayerUpdate copies into PlayerScratch.
 *
 * Per 30 Hz tick the host does, in the original order:
 *     ae_player_pad_set_camera(...)   when the camera changed (g_CameraRotMatrix is written by CameraLookAt late in
 *                                     the frame, so PadUpdate sees the PREVIOUS frame's matrix)
 *     ae_player_pad_update(...)       PadUpdate(1): buttons of this frame, stick sample pushed by the previous frame end
 *     ae_player_update / body step ...
 *     ae_player_pad_frame_end(...)    FrameEndUpdate -> PadSampleStickHistory: the sticks of this frame, processed by
 *                                     the NEXT PadUpdate (the original's one-frame stick latency)
 * Hooked (not ported, see ae_pad.h): PadGetState / PadInfoMode / PadSetupDualShock (a DualShock in analog mode is
 * emulated), demo playback (never active), StageRequest, StickToButtons (flag bit 1 is not used), PadRumbleReset (no rumble).
 */
#include "native_player.h"
#include "../core/ae_pad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t held;                  /* PsyQ pad word, active high (AE_PAD_* bits of src/app/native_input.h) */
    uint8_t rx, ry, lx, ly;         /* raw analog bytes 4..7 of the pad packet: right stick X / Y, left stick X / Y (0x80 centre, y down = larger) */
    int digital;                    /* 1 = a digital pad (no sticks) */
} AePlayerPadRaw;

typedef struct {
    AeRam *ram;
    AePadHooks hooks;
} AePlayerPad;

/* Creates the RAM image from the original EXE (the stick history lookup table is read from it) and runs 41 idle frames so that
 * the 40 sample history is filled like it is during a running game. Returns 1 on success. */
int ae_player_pad_init(AePlayerPad *pp, const uint8_t *exe, size_t exe_size);
void ae_player_pad_free(AePlayerPad *pp);
/* g_CameraRotMatrix from the camera eye / target (s16[3], the original g_CameraEye / g_CameraTarget). */
void ae_player_pad_set_camera(AePlayerPad *pp, const int16_t eye[3], const int16_t target[3]);
/* Writes the pad packet (buttons active low, sticks), g_Player.facing (x, z), runs PadUpdate(1) and fills `in`
 * (held, changed, the stick values; player_mode and frame_counter are left to the caller). Returns 0 if the image reported an error. */
int ae_player_pad_update(AePlayerPad *pp, const AePlayerPadRaw *raw, const int16_t facing[3], AePlayerInput *in);
/* FrameEndUpdate: PadSampleStickHistory. */
void ae_player_pad_frame_end(AePlayerPad *pp);
/* Reads the PlayerScratch input block out of the RAM image. */
void ae_player_pad_read(AePlayerPad *pp, AePlayerInput *in);

#ifdef __cplusplus
}
#endif
#endif
