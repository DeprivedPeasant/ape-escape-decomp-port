#ifndef AE_PLAYER_HOST_H
#define AE_PLAYER_HOST_H
#include "../player/native_player_body.h"
#include "../player/native_player_volume.h"
#include "../player/native_player_pad.h"
#include "../player/native_gadget_slots.h"
#include "../collision/native_collision.h"
typedef struct {
    AePlayerData data;
    AePlayer player;
    AePlayerBodyState body;
    AePlayerBodyWorld world;
    AeVolume volume;
    AeGround *ground;
    AeCollision *collision;
    uint32_t update_flags,body_flags;
    int failed;
    AePlayerPad pad;          /* exact PadUpdate on a RAM image (src/core/ae_pad.c): raw pad bytes in, PlayerScratch input out */
    uint32_t gadget_unlock_mask; /* g_GadgetUnlockMask (0x800F51C4) after GadgetSlotsInitForExit: 3 in a new game */
} AePlayerHost;
int ae_player_host_init(AePlayerHost *h,AeGround *g,AeCollision *c,const char *member,
                        const char *exe,uint32_t area,const int32_t pos[3],const int16_t facing[3]);
void ae_player_host_free(AePlayerHost *h);
/* Read-only observer called with the player position after ae_player_update and before
 * ae_player_body_step. It must not modify the player. Returning 0 stops the step: body
 * integration and the frame counter do not advance and the step returns 0. The player update
 * has already happened by then (there is no rollback). */
typedef int (*AePlayerHostWorldTick)(void *user,const int32_t player_position[3]);
/* Raw controller input of one 30 Hz tick, exactly what the original PadUpdate sees (see src/core/ae_pad.h): `held` is the PsyQ
 * pad word (active high, as in g_PadHeld; `changed` and the repeat words are derived by PadUpdate) and the four analog bytes of
 * the pad packet (bytes 4..7: right stick X / Y, left stick X / Y; 0x80 centre, y down = larger). The stick magnitude, push
 * class, direction, camera-relative vectors, flick / spin gesture state and the gadget cam values are all computed by the
 * ported PadUpdate. The left stick (lx, ly) is the MOVEMENT stick (g_RStick*), the right stick (rx, ry) the GADGET stick
 * (g_LStick*) of the original's naming. */
typedef struct {
    uint16_t held;
    uint8_t rx,ry,lx,ly;
} AePlayerHostPad;
/* g_CameraRotMatrix for the NEXT PadUpdate, from the camera eye / target (s16[3], PSX units; see ae_pad_CameraRotMatrix). */
void ae_player_host_set_camera(AePlayerHost *h,const int16_t eye[3],const int16_t target[3]);
int ae_player_host_step_input(AePlayerHost *h,const AePlayerHostPad *pad,AePlayerHostWorldTick tick,void *user);
int ae_player_host_step_game_input(AePlayerHost *h,AeRam *ram);
#endif
