#ifndef AE_NATIVE_PLAYER_H
#define AE_NATIVE_PLAYER_H
/*
 * Portable C99 reconstruction of the original player movement update (PlayerUpdate, mode 0,
 * no-gadget state set).  Every behaviour is traced to a function in
 * extract/cloud_evidence/player/; see notes/70-native-player.md for the evidence map,
 * the list of unsupported behaviours and the missing original inputs.
 *
 * Units are the game's: integer, Y negative-up, 0x1000 = 1.0 for directions, velocities in
 * 1/0x1000 world units per frame in move_vel and whole units per frame in vel.
 *
 * Per frame the host does:
 *     AePlayerGround g = <probe the collision engine at p->pos>;
 *     ae_player_update(p, &data, &input, &g);
 *     <collision/integration: resolve p->pos + p->vel against the world>
 *     ae_player_apply_motion(p, new_pos, new_vel, contact);
 * The original runs the position integration, ground clamp and contact recording in
 * CollisionUpdate / BodyLandOnGround, which are NOT in the committed evidence; the host (or the
 * synthetic integrator used by the tests) supplies them.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_PLAYER_NO_GADGET 9           /* currentGadget value that skips the gadget transition set */
#define AE_PLAYER_STATE_COUNT 0x80      /* states < 0x80 live in the no-gadget set */
#define AE_PLAYER_EVENT_MAX 32

/* ---- decoded data (same layout as archive member 0x008) ---- */
typedef struct { uint16_t first_frame, last_frame; uint8_t turn_tilt, move_mode, frame_action, flags; } AePlayerStateEntry;
typedef struct { uint8_t state; int8_t cond; uint8_t new_state; uint8_t action; } AePlayerTransition;

typedef struct {
    AePlayerStateEntry *states; uint32_t state_count;
    AePlayerTransition *transitions; uint32_t transition_count;   /* terminated by state == 0xFF, count excludes it */
    uint8_t *events; uint32_t event_bytes;                        /* raw event records (see PlayerRunAnimEvents) */
} AePlayerSet;

/* Tunables initialised by PlayerInit (constants visible in 8003272c_PlayerInit.c). */
typedef struct {
    int32_t gravity, jump_impulse, jump_hold_boost, jump_vel2, drop_vel, squash_frames, unk_1c, unk_20,
            move_speed, jump_speed_pct, jump_speed_pct2, jump_speed_pct3;
} AePlayerParams;

/* g_SurfaceTable (0x800AEF60): eight 0x10-byte entries indexed by surface kind. */
typedef struct { uint8_t raw[16]; } AeSurfaceEntry;

typedef struct {
    AePlayerSet set0;               /* no-gadget set (states 0..0x7F) */
    AePlayerSet gadget_sets[9];     /* member sets 1..9 = gadgets 0..8 (states 0x80.., indexed state - 0x80) */
    AePlayerParams params;
    AeSurfaceEntry surface[8];
    int16_t sine_quarter[1025];     /* SDK sine table: sin(i * 2pi / 4096) * 4096 for i = 0..0x400 */
    uint32_t jump_table[8]; int jump_loaded;   /* 0x800A5A78: per-state handler addresses for states 0x46..0x4D */
    int surface_loaded, trig_loaded;
    int trig_exact;                 /* 1 when the table came from the original EXE */
    int16_t vec_lut[192];           /* VectorNormal lookup table (0x800A9C14): floor(sqrt(2^30 / (i + 64))) by default, the EXE's bytes when loaded */
    int vec_lut_exact;              /* 1 when vec_lut came from the original EXE */
} AePlayerData;

/* ---- host input: the PlayerScratch block + the raw values PlayerUpdate reads ---- */
typedef struct {
    uint32_t held, changed;         /* PsyQ pad bits: 0x0A jump, 0x200 L3, 0x400 R3 */
    uint16_t stick_class;           /* g_RStickPushState: 0 / 2 / 4 / 8 (movement stick; see ae_player_stick_class) */
    uint16_t stick_mag;             /* g_RStickMag 0..0x7F */
    int16_t cam_dir[4];             /* g_RStickCamDir + DAT_800e54b8: camera-relative stick direction (x, y, z, pad), 0x1000 scale */
    int16_t stick_cam[2];           /* DAT_800e54d0 (forward) / DAT_800e54d2 (side): stick relative to facing, +-0x1000 */
    uint16_t right_stick_gesture;   /* PlayerScratch.rightStickGesture = g_LStickFlickState (gadget stick; 0 = none) */
    uint16_t gesture_events;        /* PlayerScratch.gestureEvents = g_StickGestureEvents (bit 2 push, bit 3 hard flick) */
    uint16_t gadget_mag;            /* g_LStickMag 0..0x7F: magnitude of the gadget stick (PlayerScratch.rightStickMag) */
    int16_t gadget_cam_dir[2];      /* DAT_800e54bc / DAT_800e54c0: camera-relative gadget stick direction (x, z), 0x1000 scale */
    int16_t gadget_cam[2];          /* DAT_800e54cc / DAT_800e54ce (PlayerScratch.rightStickCam): gadget stick along / across the facing */
    int32_t gadget_spin, move_spin; /* g_LStickSpin / g_RStickSpin (PlayerScratch.unk_20 / unk_24): cross product of the last two samples */
    int16_t stick_rotation;         /* DAT_800e54d4: spin amount of the 0x10 / 0x20 / 0x80 gesture states */
    int16_t stick_travel_gadget, stick_travel_move;   /* DAT_800e54dc / DAT_800e54de: sum of |delta| over the 40 sample history */
    uint16_t player_mode;           /* g_PlayerMode (0x800F44A0): 0 normal; CanSwitchGadget reads it */
    int locked;                     /* input lock (DAT_800f44a2 == 0xb etc.): scratch input is zeroed, raw stick values still visible */
    uint32_t frame_counter;         /* g_FrameCounter, used for the death voice and surface SFX parity */
} AePlayerInput;

/* ---- host ground probe at p->pos: GetGroundHeightEx + GetSurfaceType ---- */
typedef struct {
    int32_t ground_y;               /* floor height below the player (s16 in the original) */
    int16_t slope_x, slope_z;       /* DAT_1f8003fc / DAT_1f8003fe */
    uint16_t surface_flags;         /* DAT_1f8003f8: cell attribute (bit0 hazard, bits1-3 surface kind, low nibble 1 = no floor) */
    uint16_t ground_type;           /* GetSurfaceType; >>12 == 1 is water */
    int16_t water_y;
} AePlayerGround;

enum { AE_EV_SFX = 1, AE_EV_VOICE, AE_EV_RUMBLE, AE_EV_DAMAGE, AE_EV_FX, AE_EV_XA, AE_EV_CALL };
typedef struct { uint8_t kind; int32_t a, b, c; } AePlayerEvent;

/* Bits returned by ae_player_update: behaviours reached that this module does not model. */
enum {
    AE_UNSUP_GADGET_STATE = 1u << 0,    /* state >= 0x80 (gadget set) */
    AE_UNSUP_WATER        = 1u << 1,    /* states 0x46..0x4D (jump-table handlers) / water ground */
    AE_UNSUP_TURN_MODE    = 1u << 2,    /* turn mode 4/5/6/8/9 (swim / gesture) */
    AE_UNSUP_MOVE_MODE    = 1u << 3,    /* move mode 0xC (swim) or unknown */
    AE_UNSUP_COND         = 1u << 4,    /* transition condition not modelled (treated as false) */
    AE_UNSUP_ACTION       = 1u << 5,    /* transition action not modelled (skipped) */
    AE_UNSUP_ATTACH       = 1u << 6,    /* climb / rope / object attach flags */
    AE_UNSUP_CONTACT      = 1u << 7,    /* contact types 4/5/0xB (object push / hazard) */
    AE_UNSUP_FRAME_ACTION = 1u << 8,
    AE_UNSUP_EVENT        = 1u << 9,
    AE_UNSUP_TILT         = 1u << 10    /* model tilt (upVector) not updated */
};

typedef struct {
    int32_t pos[3], vel[3], vel_frac[3], target_move_vel[3], move_vel[3];
    int16_t facing[4], desired_dir[4], prev_facing[4], saved_facing[4];
    uint32_t contact;               /* >>28 type, & 0xFFF object index (from the host collision pass) */
    int16_t slope_x, slope_z;
    int32_t ground_y;
    int16_t has_ground;
    uint16_t ground_type, prev_ground_type;
    int16_t surface_kind;           /* -1 airborne */
    uint16_t anim_frame, anim_lerp, anim_speed, anim_blend, state;
    int16_t next_frame;
    int32_t anim_blend_from;        /* frame the blend starts from (original: pointer to its decoded frame) */
    uint32_t state_flags, attach_flags, prev_attach_flags;
    uint16_t attach_timer;
    int16_t attach_dir[3];
    int16_t health, invuln_timer, fall_frames, squash_timer, hurt_blink;   /* hurt_blink = g_PlayerHurtBlink */
    uint8_t unk_ee, hurt, contact_frames, contact_cooldown;
    int8_t current_gadget, saved_gadget;   /* saved_gadget = the second argument of the last PlayerSelectGadget */
    int8_t gadget_slots[4];         /* g_GadgetSlots: gadget ids on Triangle / Square / Circle / Cross, -1 = empty */
    int16_t body_radius, body_height;
    int16_t dash_boost;             /* DAT_800f5126 (state 6 dash start flag) */
    int16_t scratch_dir[4];         /* 0x1F800008: working direction vector */
    int16_t scratch_input[3];       /* 0x1F800000/4: normalised velocity-change direction (x, y, z) */
    int damage_enabled;             /* PlayerDamage is a no-op on levels 0x1F / 0x20 */
    AePlayerEvent events[AE_PLAYER_EVENT_MAX]; int event_count;
    uint32_t unsupported_total;     /* OR of every unsupported bit ever seen */
} AePlayer;

/* ---- data loading ---- */
void ae_player_data_init(AePlayerData *d);                       /* PlayerInit constants, approximate trig, no surface table */
void ae_player_data_free(AePlayerData *d);
/* Parse the original archive member 0x008 (as extracted to 0x80103000). Copies the no-gadget set. */
int ae_player_data_load_member(AePlayerData *d, const uint8_t *member, size_t size);
/* Read the surface table, the SDK sine table and the 0x46..0x4D handler table from the original SCUS_944.23 image (0x800AEF60 / 0x800A9DB0 / 0x800A5A78). */
int ae_player_data_load_exe(AePlayerData *d, const uint8_t *exe, size_t size);
/* Replace the approximate sine table (used by tests; ae_player_data_load_exe sets the exact one). */
void ae_player_data_set_sine(AePlayerData *d, const int16_t quarter[1025], int exact);
const char *ae_player_error(void);

/* ---- player ---- */
void ae_player_init(AePlayer *p, const int32_t pos[3], const int16_t facing[3]);   /* PlayerInit + spawn defaults */
uint32_t ae_player_update(AePlayer *p, const AePlayerData *d, const AePlayerInput *in, const AePlayerGround *g);
void ae_player_apply_motion(AePlayer *p, const int32_t pos[3], const int32_t vel[3], uint32_t contact);

/* ---- evidence-backed helpers ---- */
uint16_t ae_player_stick_class(uint16_t mag);                    /* PadUpdate's g_RStickPushState ladder: 0 / 2 / 4 / 8 (for tests; the host gets the real value from src/core/ae_pad.c) */
/* PadUpdate camera-relative components (DAT_800e54d0 / d2) from the previous frame's facing; test helper, the host reads them from ae_pad. */
void ae_player_stick_cam(const AePlayer *p, const AePlayerData *d, const int16_t cam_dir[4], int16_t out[2]);
int32_t ae_player_rsin(const AePlayerData *d, int32_t a);
int32_t ae_player_rcos(const AePlayerData *d, int32_t a);
int32_t ae_player_int_sqrt(int32_t n);                           /* 0x80046584 */
int32_t ae_player_vec_length3(int32_t x, int32_t y, int32_t z);  /* 0x80046618 */
/* 0x800301E0 VectorNormalSS (core 0x80030210), exact: returns the SUM OF SQUARES x*x + y*y + z*z, out = the low 16 bits of the
 * three GPF results. A signed overflow of the sum traps in the original: nothing is written and ae_player_normalize_traps counts it. */
int32_t ae_player_normalize(const AePlayerData *d, const int16_t in[3], int16_t out[3]);
uint32_t ae_player_normalize_traps(void);
void ae_player_turn_towards_y(const AePlayerData *d, const int16_t target[4], int16_t dir[4], int32_t max_angle); /* 0x80066C90 */
int ae_player_accel_xz(AePlayer *p, int32_t max_step);           /* 0x8006699C: returns remaining distance */
void ae_player_set_state(AePlayer *p, const AePlayerData *d, uint32_t state); /* 0x80064208 */
/* PlayerSelectGadget (0x8007AC64), state part only: equips gadget 0..8 or 9 (none); old = its second argument (the
 * previous gadget, -1 for the first selection at a game / area start). Returns 1 when the gadget changed, 0 if it did
 * not or the set is missing. ae_player_select_gadget uses p->current_gadget as `old`. */
int ae_player_select_gadget(AePlayer *p, const AePlayerData *d, int gadget);
int ae_player_select_gadget_from(AePlayer *p, const AePlayerData *d, int gadget, int old);
/* IsPlayerSwimming 0x80068B7C / CanSwitchGadget 0x80066F50 / PlayerCheckGadgetSelect 0x80067974 (decision part: which gadget
 * PlayerSelectGadget gets, if any). Exact ports, diffed by tools/mips/check_player_input.py. */
int ae_player_is_swimming(uint16_t state);
int ae_player_can_switch_gadget(uint16_t player_mode, uint16_t state);
int ae_player_gadget_select_request(uint16_t player_mode, uint16_t state, uint32_t held, uint32_t changed, const int8_t slots[4], int *gadget);
/* State table entry of the current state (set 0 or the equipped gadget's set); NULL if unknown. */
const AePlayerStateEntry *ae_player_state_entry(const AePlayer *p, const AePlayerData *d, uint32_t state);

#ifdef __cplusplus
}
#endif
#endif
