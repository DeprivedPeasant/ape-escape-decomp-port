#include "native_player_pad.h"
#include <string.h>

/* The pad driver is hardware: emulate a DualShock that is ready and already configured (g_PadDualShockState 3, mode 7). */
static int32_t hook_get_state(void *u, AeRam *r, int32_t port) { (void)u; (void)r; (void)port; return 6; }
static int32_t hook_info_mode(void *u, AeRam *r, int32_t port, int32_t a, int32_t b) { (void)u; (void)r; (void)port; (void)a; (void)b; return 7; }
static void hook_setup(void *u, AeRam *r, int32_t arg) { (void)u; (void)arg; ae_wr32(r, AE_PAD_DUALSHOCK_STATE, 3); }
static void hook_rumble_reset(void *u, AeRam *r, int32_t port) { (void)u; (void)r; (void)port; }

static void write_packet(AeRam *r, const AePlayerPadRaw *raw) {
    uint32_t held = ~(uint32_t)raw->held & 0xFFFFu;
    ae_wr8(r, AE_PAD_BUF1 + 0, 0x00);                         /* status: pad present */
    ae_wr8(r, AE_PAD_BUF1 + 1, raw->digital ? 0x41 : 0x73);   /* id: digital / analog (DualShock, 3 words) */
    ae_wr8(r, AE_PAD_BUF1 + 2, held >> 8);
    ae_wr8(r, AE_PAD_BUF1 + 3, held & 0xFF);
    ae_wr8(r, AE_PAD_BUF1 + 4, raw->rx); ae_wr8(r, AE_PAD_BUF1 + 5, raw->ry);
    ae_wr8(r, AE_PAD_BUF1 + 6, raw->lx); ae_wr8(r, AE_PAD_BUF1 + 7, raw->ly);
    /* port 2: no pad */
    ae_wr8(r, AE_PAD_BUF2 + 0, 0xFF);
}

int ae_player_pad_init(AePlayerPad *pp, const uint8_t *exe, size_t exe_size) {
    memset(pp, 0, sizeof(*pp));
    pp->ram = ae_ram_create();
    if (!pp->ram) return 0;
    if (!ae_ram_load_exe(pp->ram, exe, exe_size)) { ae_ram_destroy(pp->ram); pp->ram = NULL; return 0; }
    pp->hooks.pad_get_state = hook_get_state; pp->hooks.pad_info_mode = hook_info_mode;
    pp->hooks.pad_setup_dualshock = hook_setup; pp->hooks.pad_rumble_reset = hook_rumble_reset;
    /* a camera looking along +Z until the host says otherwise (eye behind the target) */
    int16_t eye[3] = { 0, 0, 0 }, target[3] = { 0, 0, 0x100 };
    ae_player_pad_set_camera(pp, eye, target);
    AePlayerPadRaw idle = { 0, 0x80, 0x80, 0x80, 0x80, 0 };
    int16_t facing[3] = { 0, 0, 0x1000 };
    AePlayerInput in;
    for (int i = 0; i < 41; ++i) { if (!ae_player_pad_update(pp, &idle, facing, &in)) return 0; ae_player_pad_frame_end(pp); }
    return 1;
}

void ae_player_pad_free(AePlayerPad *pp) { if (pp->ram) ae_ram_destroy(pp->ram); pp->ram = NULL; }

void ae_player_pad_set_camera(AePlayerPad *pp, const int16_t eye[3], const int16_t target[3]) {
    ae_pad_CameraRotMatrix(pp->ram, eye, target, AE_G_CAMERA_ROT_MATRIX);
}

void ae_player_pad_read(AePlayerPad *pp, AePlayerInput *in) {
    AeRam *r = pp->ram;
    in->held = ae_rd32(r, AE_PAD_HELD); in->changed = ae_rd32(r, AE_PAD_CHANGED);
    in->stick_class = (uint16_t)ae_rd16(r, AE_PAD_RSTICK_PUSH);
    in->stick_mag = (uint16_t)ae_rd16(r, AE_PAD_RSTICK_MAG);
    for (int i = 0; i < 4; ++i) in->cam_dir[i] = (int16_t)ae_rds16(r, AE_PAD_RSTICK_CAMDIR + 2u * (uint32_t)i);
    in->stick_cam[0] = (int16_t)ae_rds16(r, AE_PAD_RCAM_FWD); in->stick_cam[1] = (int16_t)ae_rds16(r, AE_PAD_RCAM_SIDE);
    in->right_stick_gesture = (uint16_t)ae_rd16(r, AE_PAD_LSTICK_FLICK);
    in->gesture_events = (uint16_t)ae_rd16(r, AE_PAD_GESTURE_EVENTS);
    in->gadget_mag = (uint16_t)ae_rd16(r, AE_PAD_LSTICK_MAG);
    in->gadget_cam_dir[0] = (int16_t)ae_rds16(r, AE_PAD_LSTICK_CAMDIR); in->gadget_cam_dir[1] = (int16_t)ae_rds16(r, AE_PAD_LSTICK_CAMDIR + 4);
    in->gadget_cam[0] = (int16_t)ae_rds16(r, AE_PAD_LCAM_FWD); in->gadget_cam[1] = (int16_t)ae_rds16(r, AE_PAD_LCAM_SIDE);
    in->gadget_spin = ae_rds32(r, AE_PAD_LSTICK_SPIN); in->move_spin = ae_rds32(r, AE_PAD_RSTICK_SPIN);
    in->stick_rotation = (int16_t)ae_rds16(r, AE_PAD_STICK_ROTATION);
    in->stick_travel_gadget = (int16_t)ae_rds16(r, AE_PAD_TRAVEL_L); in->stick_travel_move = (int16_t)ae_rds16(r, AE_PAD_TRAVEL_R);
}

int ae_player_pad_update(AePlayerPad *pp, const AePlayerPadRaw *raw, const int16_t facing[3], AePlayerInput *in) {
    AeRam *r = pp->ram;
    write_packet(r, raw);
    ae_wr16(r, AE_G_PLAYER_FACING, (uint16_t)facing[0]);
    ae_wr16(r, AE_G_PLAYER_FACING + 4, (uint16_t)facing[2]);
    ae_pad_PadUpdate(r, &pp->hooks, AE_PADUPDATE_CAMERA_RELATIVE);
    ae_player_pad_read(pp, in);
    return !ae_ram_failed(r);
}

void ae_player_pad_frame_end(AePlayerPad *pp) { ae_pad_PadSampleStickHistory(pp->ram); }
