/* Portable integration adapter for the instruction-verified native camera
 * follow code (native_camera_follow.h). See native_camera_host.md.
 * The host owns the math tables and binds the normalize / SquareRoot0 /
 * GetGroundHeight callbacks to the caller's ground and collision objects.
 * No game data is embedded; the EXE image is only read during init. */
#ifndef AE_NATIVE_CAMERA_HOST_H
#define AE_NATIVE_CAMERA_HOST_H

#include <stddef.h>
#include <stdint.h>

#include "native_camera_follow.h"
#include "../collision/native_collision.h"
#include "../collision/native_ground.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host status bits (AeCameraHost.status). The low bits are the
 * AE_CAMERA_FOLLOW_* bits of the failed follow call. */
enum {
    AE_CAMERA_HOST_MATH_DOMAIN = 0x100  /* a normalize callback failed */
};

typedef struct AeCameraHost {
    AeCameraSlot slot;
    AeCameraFollowFrame frame;
    AeCameraMathTables math;
    AeCameraQueries queries;   /* queries.user points at this host: do not copy or move a bound host */
    AeGround *ground;          /* borrowed */
    AeCollision *collision;    /* borrowed */
    uint8_t snapshot_shadow[16]; /* original globals 0x800E693C..0x800E694B: render8, then facing8 */
    uint32_t status;
    int ready;
} AeCameraHost;

/* The player fields the original CameraSnapshotPlayer reads. render8 is the
 * original bytes at player actor + 0x1C (8 bytes) and must be supplied by the
 * caller; it is copied verbatim and never derived from pos. */
typedef struct AeCameraPlayerSnapshot {
    uint8_t render8[8];
    uint8_t facing8[8];   /* g_Player.facing: s16 vx, vy, vz, pad (little endian) */
    int32_t pos[3];
    int32_t vel[3];
    int32_t ground_y;
} AeCameraPlayerSnapshot;

/* Zeroes the host, loads the math tables from the EXE image, binds the
 * callbacks and resets the camera from the snapshot. Returns 1 on success.
 * Returns 0 (ready = 0) on a null argument, a missing ground / collision, a
 * math load failure or a reset failure. */
int ae_camera_host_init(AeCameraHost *h, const uint8_t *exe, size_t exe_size,
                        AeGround *ground, AeCollision *collision,
                        const AeCameraPlayerSnapshot *snapshot);

/* CameraSlotInitFollow(kind 1) from the snapshot. Needs a ready host. Zeroes
 * the slot, frame scratch and frame controls, snapshots, clears the status.
 * The other frame inputs (current_area, state, ...) are left as they are.
 * Returns 0 with the slot unchanged if the atan fails or a pointer is missing. */
int ae_camera_host_reset(AeCameraHost *h, const AeCameraPlayerSnapshot *snapshot);

/* CameraSnapshotPlayer: fills frame scratch / snapshot_shadow only. */
void ae_camera_host_snapshot(AeCameraHost *h, const AeCameraPlayerSnapshot *snapshot);

/* One CameraFollowUpdate (mode 0 or 1) after a snapshot. The caller sets the
 * non-snapshot frame inputs beforehand. Returns 1 on success; on failure
 * slot, frame and shadow are rolled back, the failure bits are in h->status
 * (ORed into frame.status) and 0 is returned. */
int ae_camera_host_step(AeCameraHost *h, const AeCameraPlayerSnapshot *snapshot, int mode);

/* PlayerCanExitStage (0x8005DC78). stage_exit_code = byte g_StageExitCode,
 * state = halfword g_Player.state, state_flags = g_Player.stateFlags. */
int ae_camera_can_exit_stage(uint32_t stage_exit_code, uint32_t state, uint32_t state_flags);

/* PlayerIsSubmerged (0x8006B9F8). water_active = byte at 0x800BAD7B,
 * water_y = halfword g_Player.waterY, player_y = word g_Player.pos[1]. */
int ae_camera_player_submerged(int water_active, int32_t water_y, int32_t player_y);

#ifdef __cplusplus
}
#endif

#endif
