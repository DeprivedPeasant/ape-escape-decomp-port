/*
 * Portable reconstruction of the original player movement update.
 * Evidence: extract/cloud_evidence/player/ (function name -> file is given per routine).
 * No game data is embedded.  Tables and constants come from PlayerInit, the member 0x008 image
 * and (optionally) the original EXE image, all supplied by the host at runtime.
 */
#include "native_player.h"
#include "../core/ae_vecnorm.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static char error_text[160];
static int fail(const char *message) { strncpy(error_text, message, sizeof(error_text) - 1); error_text[sizeof(error_text) - 1] = 0; return 0; }
const char *ae_player_error(void) { return error_text; }

static uint32_t floor_sqrt(uint32_t n) {
    uint32_t result = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) { if (n >= result + bit) { n -= result + bit; result = (result >> 1) + bit; } else result >>= 1; bit >>= 2; }
    return result;
}

static int32_t narrow16(int32_t n) { return (int16_t)(uint16_t)((uint32_t)n & 0xFFFFu); }
static int32_t clamp16(int64_t n) { return n > 32767 ? 32767 : n < -32768 ? -32768 : (int32_t)n; }
/* MIPS `sra` of a signed value: arithmetic shift. */
static int32_t sra(int32_t v, int s) { return v >> s; }
/* "if (v < 0) v += (1<<s)-1; v >> s": truncating divide by a power of two (decompiler idiom). */
static int32_t div_pow2(int32_t v, int s) { return (v < 0 ? v + ((1 << s) - 1) : v) >> s; }
static int32_t abs32(int32_t v) { return v < 0 ? -v : v; }

/* ------------------------------------------------------------------ data loading */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

void ae_player_data_set_sine(AePlayerData *d, const int16_t quarter[1025], int exact) {
    memcpy(d->sine_quarter, quarter, sizeof(d->sine_quarter));
    d->trig_loaded = 1; d->trig_exact = exact;
}

void ae_player_data_init(AePlayerData *d) {
    memset(d, 0, sizeof(*d));
    /* PlayerInit (8003272c): constants written into g_PlayerParams. */
    d->params.gravity = 0xAD;        d->params.jump_impulse = -0x672; d->params.jump_hold_boost = -0x29E;
    d->params.jump_vel2 = -0x690;    d->params.drop_vel = 0x500;      d->params.squash_frames = 1;
    d->params.unk_1c = 200;          d->params.unk_20 = 0x780;        d->params.move_speed = 0x3F2;
    d->params.jump_speed_pct = 100;  d->params.jump_speed_pct2 = 0x50; d->params.jump_speed_pct3 = 0x3C;
    /* Approximation of the SDK table; NOT verified against the original bytes (trig_exact = 0). */
    int16_t table[1025];
    for (int i = 0; i <= 0x400; ++i) table[i] = (int16_t)lround(4096.0 * sin((double)i * 3.14159265358979323846 / 2048.0));
    ae_player_data_set_sine(d, table, 0);
    d->trig_loaded = 1;
    /* VectorNormal lookup table (0x800A9C14): floor(sqrt(2^30 / (i + 64))), identical to the EXE's 192 entries (checked by
       tools/mips/check_player_input.py); ae_player_data_load_exe still reads the real bytes. */
    for (uint32_t i = 0; i < AE_VECNORM_LUT_COUNT; ++i) d->vec_lut[i] = (int16_t)floor_sqrt(1073741824u / (i + 64u));
    d->vec_lut_exact = 0;
    /* States 0x46..0x4D -> handler entry (read from the real EXE at 0x800A5A78 by the local validation,
       extract/cloud_evidence/player/local_handoff/README.md).  ae_player_data_load_exe re-reads them. */
    static const uint32_t jt[8] = { 0x80061540u, 0x80061540u, 0x8006163Cu, 0x80061520u, 0x80061520u, 0x8006163Cu, 0x80061540u, 0x80061624u };
    memcpy(d->jump_table, jt, sizeof(jt)); d->jump_loaded = 1;
}

static void set_free(AePlayerSet *s) {
    free(s->states); free(s->transitions); free(s->events);
    memset(s, 0, sizeof(*s));
}

void ae_player_data_free(AePlayerData *d) {
    set_free(&d->set0);
    for (int i = 0; i < 9; ++i) set_free(&d->gadget_sets[i]);
}

/* Layout (PlayerInit): header at +0x14 + i*0x10: u16 word offsets (x4) of animSet, stateTable, transitions, events.
   Set 0 is the no-gadget set, sets 1..9 belong to gadgets 0..8. */
static int load_set(AePlayerSet *set, const uint8_t *member, size_t size, uint32_t index) {
    if (0x24 + index * 0x10 > size) return fail("member too small");
    const uint8_t *h = member + 0x14 + index * 0x10;
    uint32_t states = (uint32_t)rd16(h + 4) * 4, trans = (uint32_t)rd16(h + 8) * 4, events = (uint32_t)rd16(h + 0xC) * 4;
    if (states >= size || trans >= size || events >= size || trans < states || events < trans) return fail("set offsets out of range");
    if ((trans - states) % 8) return fail("state table size not a multiple of 8");
    set->state_count = (trans - states) / 8;
    if (set->state_count > AE_PLAYER_STATE_COUNT) return fail("too many states");
    set->states = (AePlayerStateEntry *)calloc(set->state_count ? set->state_count : 1, sizeof(AePlayerStateEntry));
    if (!set->states) return fail("allocation failed");
    for (uint32_t i = 0; i < set->state_count; ++i) {
        const uint8_t *e = member + states + i * 8;
        set->states[i].first_frame = rd16(e); set->states[i].last_frame = rd16(e + 2);
        set->states[i].turn_tilt = e[4]; set->states[i].move_mode = e[5]; set->states[i].frame_action = e[6]; set->states[i].flags = e[7];
    }
    uint32_t n = 0;
    while (trans + (n + 1) * 4 <= size && member[trans + n * 4] != 0xFF) ++n;
    if (trans + (n + 1) * 4 > size) return fail("transition list not terminated");
    set->transitions = (AePlayerTransition *)calloc(n + 1, sizeof(AePlayerTransition));
    if (!set->transitions) return fail("allocation failed");
    for (uint32_t i = 0; i <= n; ++i) {
        const uint8_t *e = member + trans + i * 4;
        set->transitions[i].state = e[0]; set->transitions[i].cond = (int8_t)e[1]; set->transitions[i].new_state = e[2]; set->transitions[i].action = e[3];
    }
    set->transition_count = n;
    uint32_t end = events;
    for (;;) {                                  /* walk the event list to its 0xFF terminator */
        if (end >= size) return fail("event list not terminated");
        uint8_t first = member[end];
        if (first == 0xFF) { ++end; break; }
        uint8_t kind = end + 2 < size ? member[end + 2] : 0;
        end += (kind == 2 || kind == 6) ? 16 : (kind == 3 || kind == 7) ? 8 : 4;
    }
    set->event_bytes = end - events;
    set->events = (uint8_t *)malloc(set->event_bytes);
    if (!set->events) return fail("allocation failed");
    memcpy(set->events, member + events, set->event_bytes);
    return 1;
}

int ae_player_data_load_member(AePlayerData *d, const uint8_t *member, size_t size) {
    if (size < 0x24) return fail("member too small");
    ae_player_data_free(d);
    if (!load_set(&d->set0, member, size, 0)) return 0;
    for (uint32_t i = 0; i < 9; ++i)                /* gadget sets: optional, a bad set only disables that gadget */
        if (!load_set(&d->gadget_sets[i], member, size, i + 1)) set_free(&d->gadget_sets[i]);
    return 1;
}

#define EXE_FILE_OFFSET(addr) ((size_t)((addr) - 0x80010000u + 0x800u))
int ae_player_data_load_exe(AePlayerData *d, const uint8_t *exe, size_t size) {
    size_t surface = EXE_FILE_OFFSET(0x800AEF60u), sine = EXE_FILE_OFFSET(0x800A9DB0u), lut = EXE_FILE_OFFSET(AE_VECNORM_LUT_ADDR);
    if (size < surface + 0x80 || size < sine + 2050 || size < EXE_FILE_OFFSET(0x800A5A78u) + 32 || size < lut + 2 * AE_VECNORM_LUT_COUNT)
        return fail("EXE image too small for player tables");
    int16_t table[1025];
    for (int i = 0; i <= 0x400; ++i) table[i] = (int16_t)rd16(exe + sine + (size_t)i * 2);
    if (table[0] != 0 || table[0x400] != 0x1000) return fail("sine table endpoints invalid; wrong EXE?");
    for (int i = 1; i <= 0x400; ++i) if (table[i] < table[i - 1]) return fail("sine table not monotonic; wrong EXE?");
    ae_player_data_set_sine(d, table, 1);
    memcpy(d->surface, exe + surface, sizeof(d->surface));
    d->surface_loaded = 1;
    for (int i = 0; i < AE_VECNORM_LUT_COUNT; ++i) d->vec_lut[i] = (int16_t)rd16(exe + lut + (size_t)i * 2);
    d->vec_lut_exact = 1;
    size_t jt = EXE_FILE_OFFSET(0x800A5A78u);
    if (size >= jt + 32) { for (int i = 0; i < 8; ++i) d->jump_table[i] = rd32(exe + jt + (size_t)i * 4); d->jump_loaded = 1; }
    return 1;
}

/* ------------------------------------------------------------------ math (SDK routines) */

/* rcos @ 8002fc20, derived from its listing: quarter-wave table lookup. */
int32_t ae_player_rcos(const AePlayerData *d, int32_t a) {
    const int16_t *t = d->sine_quarter;
    if (a < 0) a = -a;
    a &= 0xFFF;
    if (a <= 0x800) return a <= 0x400 ? t[0x400 - a] : -t[a - 0x400];
    return a <= 0xC00 ? -t[0xC00 - a] : t[a - 0xC00];
}
/* rsin @ 8002fb50: sin_1(a & 0xFFF), negated for negative angles (sin(a) = cos(a - 0x400)). */
int32_t ae_player_rsin(const AePlayerData *d, int32_t a) {
    if (a >= 0) return ae_player_rcos(d, (a & 0xFFF) - 0x400);
    return -ae_player_rcos(d, ((-a) & 0xFFF) - 0x400);
}

/* IntSqrt @ 80046584, ported instruction by instruction (rounded square root with the GTE LZCR). */
int32_t ae_player_int_sqrt(int32_t n) {
    if (n <= 0) return 0;
    int lz = 0; uint32_t tmp = (uint32_t)n;
    while (!(tmp & 0x80000000u)) { ++lz; tmp <<= 1; }
    int32_t total = (0x21 - lz) >> 1, count = total + 1;
    uint32_t root = 0; int32_t rem = 0;
    for (int32_t i = 1; i <= count; ++i) {
        root = root * 2;
        rem = rem * 4 + (int32_t)(((uint32_t)n >> ((uint32_t)((total - i) * 2) & 0x1F)) & 3u);
        int32_t trial = (int32_t)(root * 2 + 1);
        if (rem >= trial) { rem -= trial; root += 1; }
    }
    return (int32_t)(root >> 1) + (int32_t)(root & 1);
}

/* VecLength3 @ 80046618: GTE SQR of three 16-bit values then IntSqrt of their sum. */
int32_t ae_player_vec_length3(int32_t x, int32_t y, int32_t z) {
    x = narrow16(x); y = narrow16(y); z = narrow16(z);
    return ae_player_int_sqrt((int32_t)((uint32_t)(x * x) + (uint32_t)(y * y) + (uint32_t)(z * z)));
}

/* Number of VectorNormal* calls whose sum of squares overflowed a signed 32-bit word: the original `add` traps there. */
static uint32_t normalize_traps;
uint32_t ae_player_normalize_traps(void) { return normalize_traps; }

/*
 * VectorNormalSS @ 800301e0 -> core at 0x80030210, translated from the listing (see src/core/ae_vecnorm.h):
 * GTE SQR of the three s16 inputs, v0 = the SUM OF SQUARES (the return value, never a length: $v0 is not
 * overwritten after the two `add`s), LZCR, lookup table entry (192 s16 at 0x800A9C14), GPF, srav.
 * out[] are the low 16 bits of the three results (the `sh` stores); a trapping `add` writes nothing.
 */
int32_t ae_player_normalize(const AePlayerData *d, const int16_t in[3], int16_t out[3]) {
    int32_t ir[3] = { in[0], in[1], in[2] };
    AeVecNormPlan p = ae_vecnorm_plan(ir);
    if (p.trapped) { ++normalize_traps; return 0; }
    int32_t ir0 = p.sum != 0 ? d->vec_lut[p.index] : 0;
    for (int i = 0; i < 3; ++i) out[i] = (int16_t)ae_vecnorm_apply(ir0, ir[i], p.shift);
    return p.sum;
}

/* TurnTowardsY @ 80066c90 (+ SDK RotMatrixY / ApplyMatrixSV on the y-axis rotation matrix). */
void ae_player_turn_towards_y(const AePlayerData *d, const int16_t target[4], int16_t dir[4], int32_t max_angle) {
    int32_t dot = ((int32_t)target[0] * dir[0] + (int32_t)target[2] * dir[2]);
    if (sra(dot, 12) < ae_player_rcos(d, max_angle)) {
        int32_t cross = (int32_t)target[0] * dir[2] - (int32_t)target[2] * dir[0];
        if (div_pow2(cross, 12) < 1) max_angle = -max_angle;
        int32_t c = ae_player_rcos(d, max_angle), s = ae_player_rsin(d, max_angle);
        int32_t x = dir[0], z = dir[2];
        dir[0] = (int16_t)clamp16(((int64_t)c * x + (int64_t)s * z) >> 12);
        dir[2] = (int16_t)clamp16(((int64_t)-s * x + (int64_t)c * z) >> 12);
        /* y is multiplied by the matrix's 0x1000 centre element: unchanged. */
    } else {
        for (int i = 0; i < 4; ++i) dir[i] = target[i];
    }
}

/* PlayerAccelXZ @ 8006699c: limits the change of move_vel (desired) away from target_move_vel (current). */
int ae_player_accel_xz(AePlayer *p, int32_t max_step) {
    int32_t dx = p->move_vel[0] - p->target_move_vel[0];
    if (dx < 0) dx += 0x3F;
    int32_t dz = p->move_vel[2] - p->target_move_vel[2];
    if (dz < 0) dz += 0x3F;
    int32_t len = ae_player_vec_length3(sra(dx, 6), 0, sra(dz, 6)) * 0x40;
    int32_t sx = sra(dx, 6) * max_step;
    if (max_step < len) {
        int32_t q = sra(len < 0 ? len + 0x3F : len, 6);
        int32_t sz = sra(dz, 6) * max_step;
        if (q == 0) return len;                    /* original traps on a zero divisor */
        p->move_vel[0] = p->target_move_vel[0] + sx / q;
        p->move_vel[2] = p->target_move_vel[2] + sz / q;
        return len;
    }
    return 0;
}

uint16_t ae_player_stick_class(uint16_t mag) {
    if (mag == 0) return 0;
    if (mag < 0x47) return 2;       /* PadUpdate: g_RStickPushState */
    if (mag < 0x7D) return 4;
    return 8;
}

void ae_player_stick_cam(const AePlayer *p, const AePlayerData *d, const int16_t cam_dir[4], int16_t out[2]) {
    int16_t f[3] = { p->facing[0], 0, p->facing[2] }, n[3];
    ae_player_normalize(d, f, n);
    int32_t side = (int32_t)n[0] * cam_dir[2] - (int32_t)n[2] * cam_dir[0];
    int32_t fwd = (int32_t)n[0] * cam_dir[0] + (int32_t)n[2] * cam_dir[2];
    out[0] = (int16_t)div_pow2(fwd, 12);
    out[1] = (int16_t)div_pow2(side, 12);
}

/* ------------------------------------------------------------------ helpers */

static void emit(AePlayer *p, uint8_t kind, int32_t a, int32_t b, int32_t c) {
    if (p->event_count < AE_PLAYER_EVENT_MAX) { AePlayerEvent *e = &p->events[p->event_count++]; e->kind = kind; e->a = a; e->b = b; e->c = c; }
}
static int16_t surface_s16(const AePlayerData *d, int kind, int off) { return (int16_t)rd16(d->surface[kind & 7].raw + off); }
static int32_t surface_s32(const AePlayerData *d, int kind, int off) { return (int32_t)rd32(d->surface[kind & 7].raw + off); }

static const AePlayerSet *gadget_set(const AePlayerData *d, int gadget) {
    return gadget >= 0 && gadget < 9 && d->gadget_sets[gadget].states ? &d->gadget_sets[gadget] : NULL;
}
static const AePlayerStateEntry *entry_for_g(const AePlayerData *d, int gadget, uint32_t state) {
    uint32_t i = state & 0x7F;
    if (state < 0x80) return i < d->set0.state_count ? &d->set0.states[i] : NULL;
    const AePlayerSet *g = gadget_set(d, gadget);
    return g && i < g->state_count ? &g->states[i] : NULL;
}
/* every caller has the player in scope as `p` */
#define entry_for(d, state) entry_for_g((d), p->current_gadget, (state))
const AePlayerStateEntry *ae_player_state_entry(const AePlayer *p, const AePlayerData *d, uint32_t state) { return entry_for(d, state); }

/* PlayerDamage @ 8005e6f0 */
static void player_damage(AePlayer *p, int amount) {
    if (amount && p->damage_enabled) {
        p->health = (int16_t)(p->health - amount < 0 ? 0 : p->health - amount);
        p->invuln_timer = 0x3C; p->hurt = 1;
        emit(p, AE_EV_DAMAGE, amount, 0, 0);
    }
}

/* PlayerSetState @ 80064208 */
void ae_player_set_state(AePlayer *p, const AePlayerData *d, uint32_t state) {
    const AePlayerStateEntry *cur = entry_for(d, p->state);
    p->anim_speed = 0x1000; p->anim_lerp = 0;
    if ((state == 0 && p->state != 0 && cur && (uint32_t)p->anim_frame != (uint32_t)cur->last_frame + 1u) ||
        (state == 0x24 && p->state != 0x24 && p->state != 0xE && p->state != 0x8D) ||
        (p->state == 0x46 && state == 0x47) || (p->state == 0x47 && state == 0x46)) {
        p->anim_blend = 5; p->anim_blend_from = p->anim_frame;
    } else if (state != 0x24 || p->state != 0x24) {
        p->anim_blend = 0;
    }
    const AePlayerStateEntry *next = entry_for(d, state);
    p->state = (uint16_t)state;
    if (!next) { p->unsupported_total |= AE_UNSUP_GADGET_STATE; return; }
    p->anim_frame = next->first_frame;
    p->state_flags = (uint32_t)(next->flags & 0xF) | ((uint32_t)(next->flags >> 4) << 4);
}

/* ------------------------------------------------------------------ conditions */

/* in = raw host values (g_RStick* / DAT_800e54xx); sc = the PlayerScratch copy (zeroed when input is locked or health is 0). */
typedef struct { AePlayer *p; const AePlayerData *d; const AePlayerInput *in; AePlayerInput sc; uint32_t *unsup; const AePlayerInput *frame; } Ctx;

/* Return values: 0 / 1, or -1 when the condition is not modelled. Indices from state_tables.json (g_PlayerCondFns). */
static int cond_eval(Ctx *c, int index) {
    AePlayer *p = c->p; const AePlayerInput *in = &c->sc;
    uint32_t held = in->held, changed = in->changed;
    uint16_t cls = in->stick_class, mag = in->stick_mag;
    int16_t cam0 = in->stick_cam[0], cam1 = in->stick_cam[1];
    switch (index) {
    case 0:  return 0;                                                                   /* PlayerCond_False (80064af0) */
    case 4:  return in->right_stick_gesture == 0 && (held & 0x400) != 0;                    /* NoGestureR3Held */
    case 8:  return in->right_stick_gesture == 0;                                           /* RStickNoGesture */
    case 11: return cls == 0 && (held & 0x200) != 0;                                     /* LStickIdleL3Held */
    case 12: return cls == 0;                                                            /* LStickIdle */
    case 13: return (cls & 4) != 0;
    case 14: return (cls & 8) != 0;
    case 15: return mag > 0x7C;                                                          /* LStickFull */
    case 17: return (uint32_t)(mag - 1) < 0x7B;                                          /* LStickPartial */
    case 19: return p->ground_y <= p->pos[1] && p->has_ground != 0;                    /* PlayerIsOnGround (800652e4) */
    case 21:                                                                             /* FacingContactWall (800653c0) */
        return (p->contact >> 28) == 10 && (p->contact & 0xF000) != 0x3000 && (p->contact & 0xF000) != 0x9000 &&
               (int32_t)p->scratch_input[0] * p->facing[0] + (int32_t)p->scratch_input[2] * p->facing[2] < -0xDDB000;
    case 22: return ((p->contact >> 24) & 0xF) == 8;                                     /* ContactKind8 */
    case 23: return ((p->contact >> 24) & 0xF) != 8;
    case 24: return p->pos[1] + 0x40 < p->ground_y && 0x10 < p->vel[1];                  /* FallingHigh */
    case 26: return (changed & held & 10) != 0;                                          /* JumpPressed */
    case 30: { const AePlayerStateEntry *e = entry_for(c->d, p->state);                  /* AnimDone (80065b6c) */
               return e && (uint32_t)e->last_frame < (uint32_t)p->anim_frame + (p->anim_lerp != 0); }
    case 31: return 0;                                                                   /* False2 */
    case 32: return (uint32_t)(p->vel[0] + 4) < 9 && (uint32_t)(p->vel[2] + 4) < 9 &&   /* Still (80065bb0) */
                    (uint16_t)(p->slope_z + 0xFF) < 0x1FF && (uint16_t)(p->slope_x + 0xFF) < 0x1FF;
    case 37: return cam0 < -0xB50 && cls != 0;                                           /* LStickBack */
    case 38:                                                                             /* JumpPressedAtApex (800656dc) */
        if ((held & changed & 10) == 0) return 0;
        if (abs32(p->vel[1]) < 0x26) { emit(p, AE_EV_FX, 2, 0, 0); return 1; }          /* PlayerFxJumpBurst */
        return 0;
    case 41: return (uint16_t)(cam1 + 0xB50) > 0x16A0;                                   /* LStickSideAny */
    case 43: return (in->gesture_events & 0xC) != 0;                                     /* GestureEvent0C (800657e0) */
    case 49: {                                                                           /* CanJump (80065a24) */
        if ((changed & held & 10) == 0) return 0;
        if ((p->ground_type >> 12) == 1 && (p->contact >> 28) == 10) return 1;
        if (p->surface_kind < 0 || !c->d->surface_loaded) return 0;
        int32_t limit = surface_s16(c->d, p->surface_kind, 0xC);
        return abs32(p->slope_z) <= limit && abs32(p->slope_x) <= limit; }
    case 50: return cls == 0 && (held & changed & 0x200) != 0;                           /* LStickIdleL3Pressed */
    case 55: return (cls & 2) != 0;
    case 60: { uint16_t t = p->attach_timer; p->attach_timer = (uint16_t)(t - 1); return t == 1; }  /* AttachTimerDone */
    case 64: return cls == 0 && (held & 0x20A) == 0;                                     /* NoMoveInput */
    case 65: return cam0 > 0xB50 && cls != 0;                                            /* LStickFwd */
    default: return -1;
    }
}

/* ------------------------------------------------------------------ actions */

static int act_run(Ctx *c, int index) {
    AePlayer *p = c->p; const AePlayerData *d = c->d; const AePlayerInput *in = c->in;
    uint32_t held = c->sc.held;
    switch (index) {
    case 1:  /* ScaleMoveVel 80065c20 (gadget 4 branch not modelled: only gadget 9 supported) */
        p->move_vel[1] = (d->params.gravity + d->params.jump_impulse) * 0x80;
        p->target_move_vel[0] = (p->target_move_vel[0] * d->params.jump_speed_pct) / 100; p->move_vel[0] = p->target_move_vel[0];
        p->target_move_vel[2] = (p->target_move_vel[2] * d->params.jump_speed_pct) / 100; p->move_vel[2] = p->target_move_vel[2];
        return 1;
    case 2:  /* JumpHoldBoost 80065d1c */
        if (held & 10) p->move_vel[1] += d->params.jump_hold_boost * 0x80;
        return 1;
    case 4:  /* VerticalOnly 80065dec */
        p->move_vel[0] = p->target_move_vel[0] = p->move_vel[2] = p->target_move_vel[2] = 0;
        p->move_vel[1] = d->params.drop_vel * 128;
        return 1;
    case 10: /* NegateInputXZ 8006611c: scratch dir = -scratch input */
        p->scratch_dir[1] = 0; p->scratch_dir[0] = (int16_t)-p->scratch_input[0]; p->scratch_dir[2] = (int16_t)-p->scratch_input[2];
        return 1;
    case 12: /* Land 80066770 */
        if (p->state == 0x45) {
            emit(p, AE_EV_FX, 1, 0, 0);                          /* EffectGroundImpactAtPlayer */
            if (p->fall_frames < 6) { emit(p, AE_EV_RUMBLE, 0, 1, 2); emit(p, AE_EV_SFX, 0x42, 0, 0); }
            else { player_damage(p, 1); emit(p, AE_EV_RUMBLE, 1, 0xF0, 8); ae_player_set_state(p, d, 0x18); emit(p, AE_EV_VOICE, 9, 0, 0); emit(p, AE_EV_SFX, 0x43, 0, 0); }
        } else if (p->fall_frames < 6) {
            if (p->vel[1] < 0x21) emit(p, AE_EV_SFX, 0x12, 0, 0);
            else { emit(p, AE_EV_RUMBLE, 0, 1, 1); emit(p, AE_EV_SFX, 0x13, 0, 0); }
        } else {
            player_damage(p, 1); emit(p, AE_EV_RUMBLE, 1, 0xF0, 8); p->squash_timer = 0x3C; emit(p, AE_EV_FX, 1, 0, 0);
            ae_player_set_state(p, d, 0x4B); emit(p, AE_EV_VOICE, 9, 0, 0); emit(p, AE_EV_SFX, 0x43, 0, 0);
        }
        p->attach_flags |= 0x4000000;
        return 1;
    case 19: /* JumpFromLStick 800661d4 */
        if (in->stick_class == 0) { for (int i = 0; i < 4; ++i) p->desired_dir[i] = p->facing[i]; }
        else {   /* raw push state, scratch magnitude */
            p->target_move_vel[0] = ((((int32_t)in->cam_dir[0] * c->sc.stick_mag) >> 7) * d->params.move_speed * d->params.jump_speed_pct3) >> 12;
            p->desired_dir[1] = 0; p->desired_dir[0] = in->cam_dir[0]; p->desired_dir[2] = in->cam_dir[2];
            p->target_move_vel[2] = ((((int32_t)in->cam_dir[2] * c->sc.stick_mag) >> 7) * d->params.move_speed * d->params.jump_speed_pct3) >> 12;
        }
        p->move_vel[1] = d->params.jump_vel2 * 128;
        p->move_vel[0] = p->target_move_vel[0]; p->move_vel[2] = p->target_move_vel[2];
        return 1;
    case 15: /* FaceStick 80066198: desiredDir = gadget stick direction when pushed past 0x40 */
        if (in->gadget_mag > 0x40) { p->desired_dir[1] = 0; p->desired_dir[0] = in->gadget_cam_dir[0]; p->desired_dir[2] = in->gadget_cam_dir[1]; }
        return 1;
    case 37: /* StartHurtBlink 80066018 */
        p->hurt_blink = 15;
        return 1;
    default: return 0;
    }
}

/* PlayerRunTransitions @ 800649d8 */
static void run_transitions(Ctx *c, const AePlayerTransition *t) {
    AePlayer *p = c->p; uint16_t state = p->state;
    for (uint32_t i = 0; t[i].state != 0xFF; ++i) {
        if (state < t[i].state) return;
        if (t[i].state != state) continue;
        int cond = t[i].cond, negated = cond <= 0, idx = negated ? -cond : cond;
        int r = cond_eval(c, idx);
        if (r < 0) { *c->unsup |= AE_UNSUP_COND; continue; }       /* fail closed: unmodelled conditions never fire */
        if (negated ? r == 0 : r != 0) {
            ae_player_set_state(p, c->d, t[i].new_state);
            if (t[i].action && !act_run(c, t[i].action)) *c->unsup |= AE_UNSUP_ACTION;
            return;
        }
    }
}

/* PlayerRunAnimEvents @ 80064454 (SFX / voice / callback index only; spawning kinds are skipped). */
static void run_events(Ctx *c, const AePlayerSet *set) {
    AePlayer *p = c->p;
    for (uint32_t off = 0; off + 4 <= set->event_bytes; ) {
        const uint8_t *e = set->events + off;
        if (e[0] == 0xFF) return;
        const AePlayerStateEntry *se = entry_for(c->d, e[0]);
        if (e[0] == p->state && se && (uint32_t)e[1] + se->first_frame == p->anim_frame && p->anim_lerp == 0) {
            switch (e[2]) {
            case 1: emit(p, AE_EV_SFX, e[3], 0, 0); break;
            case 8: emit(p, AE_EV_VOICE, e[3], 0, 0); break;
            case 4: emit(p, AE_EV_CALL, e[3], 0, 0); break;
            default: *c->unsup |= AE_UNSUP_EVENT; break;
            }
        }
        off += (e[2] == 2 || e[2] == 6) ? 16 : (e[2] == 3 || e[2] == 7) ? 8 : 4;
    }
}

/* ------------------------------------------------------------------ player */

void ae_player_init(AePlayer *p, const int32_t pos[3], const int16_t facing[3]) {
    memset(p, 0, sizeof(*p));
    for (int i = 0; i < 3; ++i) p->pos[i] = pos[i];
    p->facing[0] = facing[0]; p->facing[1] = facing[1]; p->facing[2] = facing[2];
    p->desired_dir[0] = facing[0]; p->desired_dir[2] = facing[2];
    p->health = 5; p->current_gadget = AE_PLAYER_NO_GADGET;
    for (int i = 0; i < 4; ++i) p->gadget_slots[i] = -1;
    p->body_radius = 0x40; p->body_height = 0x100;       /* PlayerSpawnAtEntry / structs.txt */
    p->surface_kind = -1; p->ground_y = pos[1];
    p->anim_speed = 0x1000; p->damage_enabled = 1;
    p->scratch_dir[0] = facing[0]; p->scratch_dir[2] = facing[2];
}

void ae_player_apply_motion(AePlayer *p, const int32_t pos[3], const int32_t vel[3], uint32_t contact) {
    for (int i = 0; i < 3; ++i) { p->pos[i] = pos[i]; p->vel[i] = vel[i]; }
    p->contact = contact;
}

/* PlayerUpdate (80060780), mode 0, no-gadget set.  Returns the AE_UNSUP_* bits reached this frame. */
uint32_t ae_player_update(AePlayer *p, const AePlayerData *d, const AePlayerInput *in, const AePlayerGround *g) {
    uint32_t unsup = 0;
    Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.p = p; ctx.d = d; ctx.in = in; ctx.unsup = &unsup; ctx.sc = *in;
    p->event_count = 0;
    /* PlayerUpdate head: locked input or health 0 zeroes the scratch copy (raw g_ values stay visible). */
    if (in->locked || p->health == 0) {
        ctx.sc.held = ctx.sc.changed = 0; ctx.sc.stick_class = 0; ctx.sc.stick_mag = 0;
        ctx.sc.stick_cam[0] = 0x1000; ctx.sc.stick_cam[1] = 0; ctx.sc.right_stick_gesture = 0;
        ctx.sc.gesture_events = 0; ctx.sc.gadget_mag = 0; ctx.sc.gadget_cam[0] = 0x1000; ctx.sc.gadget_cam[1] = 0;
        ctx.sc.gadget_spin = ctx.sc.move_spin = 0; ctx.sc.stick_rotation = 0; ctx.sc.stick_travel_gadget = ctx.sc.stick_travel_move = 0;
    }
    const uint16_t cls = ctx.sc.stick_class, mag = ctx.sc.stick_mag;
    /* States 0x46..0x4D dispatch through a jump table (0x800A5A78) to one of four gravity handlers, then rejoin the
       normal flow (80060780_PlayerUpdate.asm.txt 865-958: every handler ends with `j 0x8006165c`). */
    int handler = 4;
    if (p->state >= 0x80 && !entry_for(d, p->state)) { unsup |= AE_UNSUP_GADGET_STATE; p->unsupported_total |= unsup; return unsup; }
    if (p->state >= 0x46 && p->state <= 0x4D) {
        uint32_t target = d->jump_loaded ? d->jump_table[p->state - 0x46] : 0;
        handler = target == 0x80061520u ? 1 : target == 0x80061540u ? 2 : target == 0x80061624u ? 3 : target == 0x8006163Cu ? 4 : 0;
        if (!handler) { unsup |= AE_UNSUP_WATER; p->unsupported_total |= unsup; return unsup; }   /* fail closed */
    }

    /* ground probe results (GetGroundHeightEx) */
    p->ground_y = (int16_t)g->ground_y;
    p->slope_x = g->slope_x; p->slope_z = g->slope_z;
    p->has_ground = (g->surface_flags & 0xF) != 1;

    if ((p->contact >> 28) == 8) {                         /* area exit trigger: stage code consumes it */
        p->contact = 0;
    } else if (p->health > 0) {
        if (p->attach_flags & (2 | 4 | 0x10)) unsup |= AE_UNSUP_ATTACH;   /* climb / rope / special actor */
    }
    p->prev_attach_flags = p->attach_flags;

    /* normalised velocity-change direction (read by PlayerCond_FacingContactWall) */
    int16_t delta[3] = { (int16_t)(p->vel[0] - p->target_move_vel[0]), 0, (int16_t)(p->vel[2] - p->target_move_vel[2]) };
    int32_t dlen = ae_player_normalize(d, delta, p->scratch_input);
    if (0x100 < dlen) { p->vel[0] = div_pow2(p->vel[0], 2); p->vel[2] = div_pow2(p->vel[2], 2); }
    if (7 < p->target_move_vel[1] - p->vel[1] && p->vel[1] < 0) p->vel[1] = p->target_move_vel[1] - 7;

    if (!(p->attach_flags & 8)) {
        if (!(p->attach_flags & 0x40)) {
            if (p->attach_flags & 0x20) {                   /* auto / air jump push */
                if (p->state == 0x36) unsup |= AE_UNSUP_ATTACH;
                else if (p->state < 0x37) {
                    if (p->state == 0) { p->vel[1] = -0x3C; p->pos[1] -= 1; ae_player_set_state(p, d, 8); }
                    else if (p->state == 9 && abs32(p->vel[1]) < 0x26) {
                        for (int i = 0; i < 4; ++i) p->desired_dir[i] = p->facing[i];
                        p->vel[1] -= 0x14; ae_player_set_state(p, d, 0x35);
                    }
                }
            }
            p->target_move_vel[0] = p->vel[0] * 0x1000 + p->vel_frac[0];
            p->target_move_vel[2] = p->vel[2] * 0x1000 + p->vel_frac[2];
        } else {                                            /* spring */
            p->vel[1] = -0x6E; p->pos[1] -= 1; ae_player_set_state(p, d, 0x2C); emit(p, AE_EV_FX, 1, 0, 0);
            if (in->stick_class == 0) { for (int i = 0; i < 4; ++i) p->desired_dir[i] = p->facing[i]; }
            else {
                p->target_move_vel[0] = ((int32_t)in->cam_dir[0] * in->stick_mag) >> 2;
                p->target_move_vel[2] = ((int32_t)in->cam_dir[2] * in->stick_mag) >> 2;
                for (int i = 0; i < 4; ++i) p->desired_dir[i] = in->cam_dir[i];
            }
        }
    } else {                                                /* conveyor / boost */
        p->facing[0] = p->attach_dir[0]; p->facing[1] = p->attach_dir[1]; p->facing[2] = p->attach_dir[2]; p->facing[3] = (int16_t)p->attach_timer;
        p->target_move_vel[0] = (int32_t)p->attach_dir[0] * 32; p->target_move_vel[2] = (int32_t)p->attach_dir[2] * 32;
        ae_player_set_state(p, d, 6); emit(p, AE_EV_SFX, 0x27, 0, 0);
    }

    p->prev_ground_type = p->ground_type; p->ground_type = g->ground_type;
    if ((g->ground_type >> 12) == 1) unsup |= AE_UNSUP_WATER;       /* air meter, swimming, entry */

    /* gravity and the base velocity */
    int32_t gravity_step;
    if (handler == 1) {                                     /* H1 @80061520: stop, then buoyancy */
        p->vel[0] = p->vel[1] = p->vel[2] = 0; p->vel_frac[1] = 0; p->target_move_vel[0] = p->target_move_vel[2] = 0;
        handler = 2;
    }
    if (handler == 2) {                                     /* H2 @80061540: buoyancy keyed on groundType & 0xF */
        uint32_t gt = p->ground_type & 0xF;
        if (gt == 1) { p->vel[1] = div_pow2(p->vel[1], 1); if (p->pos[1] - 0x9C < g->water_y) p->vel_frac[1] += 0x2000; }
        else if (gt == 0) { if (p->state != 0x47) p->vel_frac[1] -= 0x800; }
        else if (gt == 2) p->vel_frac[1] += 0x5000;
        p->vel[1] = p->vel[1] > 0x10 ? 0x10 : p->vel[1] < -0x10 ? -0x10 : p->vel[1];
        gravity_step = p->vel_frac[1];
    } else if (handler == 3) {                              /* H3 @80061624: pinned, no gravity */
        p->vel[0] = p->vel[1] = p->vel[2] = 0; gravity_step = p->vel_frac[1];
    } else gravity_step = p->vel_frac[1] + d->params.gravity * 0x80;   /* H4 @8006163c / normal states */
    p->move_vel[0] = p->move_vel[2] = 0; p->vel_frac[0] = p->vel_frac[1] = p->vel_frac[2] = 0;
    p->target_move_vel[1] = p->vel[1] * 0x1000 + gravity_step;
    p->move_vel[1] = p->target_move_vel[1];
    /* PlayerCheckGadgetSelect (80067974): CanSwitchGadget (80066f50) + the face buttons of the scratch input. */
    {
        int gadget;
        if (ae_player_gadget_select_request(in->player_mode, p->state, ctx.sc.held, ctx.sc.changed, p->gadget_slots, &gadget))
            ae_player_select_gadget(p, d, gadget);
    }

    const AePlayerStateEntry *se = entry_for(d, p->state);
    if (!se) { unsup |= AE_UNSUP_GADGET_STATE; p->unsupported_total |= unsup; return unsup; }
    uint8_t turn = se->turn_tilt & 0xF, move_mode = se->move_mode;
    int16_t *dir = p->scratch_dir;
    for (int i = 0; i < 4; ++i) dir[i] = p->facing[i];
    int16_t target[4];
    switch (turn) {
    case 0:
        if (cls != 0) {
            for (int i = 0; i < 4; ++i) dir[i] = in->cam_dir[i];
            if (in->cam_dir[0] == 0 && in->cam_dir[2] == 0) for (int i = 0; i < 4; ++i) dir[i] = p->facing[i];
        }
        break;
    case 1: case 2:
        if (cls != 0) { for (int i = 0; i < 4; ++i) target[i] = in->cam_dir[i]; ae_player_turn_towards_y(d, target, dir, turn == 1 ? 0x71 : 0x155); }
        break;
    case 7:
        ae_player_turn_towards_y(d, p->desired_dir, p->facing, 0x155);
        for (int i = 0; i < 4; ++i) dir[i] = p->facing[i];
        break;
    case 10: {
        dir[0] = p->facing[0] >= 0x800 ? 0x1000 : p->facing[0] < -0x7FF ? -0x1000 : 0;
        dir[2] = p->facing[2] >= 0x800 ? 0x1000 : p->facing[2] < -0x7FF ? -0x1000 : 0;
        dir[1] = p->facing[1]; dir[3] = p->facing[3];
        int16_t tmp[3] = { dir[0], dir[1], dir[2] }, nrm[3];
        ae_player_normalize(d, tmp, nrm); dir[0] = nrm[0]; dir[1] = nrm[1]; dir[2] = nrm[2];
        break; }
    case 3: break;
    default: unsup |= AE_UNSUP_TURN_MODE; break;
    }
    { int16_t n[3]; ae_player_normalize(d, dir, n); p->facing[0] = n[0]; p->facing[1] = n[1]; p->facing[2] = n[2]; }
    /* PlayerApplyGadgetStance (80067a1c): GadgetHasAltStance (80065694) is true for gadgets 2, 5 and 7. */
    if (p->current_gadget == 2 || p->current_gadget == 5 || p->current_gadget == 7) {
        switch (p->state) { case 0: ae_player_set_state(p, d, 1); break; case 3: ae_player_set_state(p, d, 4); break;
                            case 5: ae_player_set_state(p, d, 7); break; case 0x13: ae_player_set_state(p, d, 0x14); break; default: break; }
    } else {
        switch (p->state) { case 1: ae_player_set_state(p, d, 0); break; case 4: ae_player_set_state(p, d, 3); break;
                            case 7: ae_player_set_state(p, d, 5); break; case 0x14: ae_player_set_state(p, d, 0x13); break; default: break; }
    }

    /* move mode: derive move_vel from the stick */
    se = entry_for(d, p->state);                               /* re-read: the stance fold may have changed the state */
    move_mode = se ? se->move_mode : 0;
    const int32_t pad_class = in->stick_class, stick_mag = mag;   /* raw push state, scratch magnitude */
    int32_t a, b;
    switch (move_mode) {
    case 0: p->move_vel[0] = p->move_vel[2] = 0; p->anim_frame++; break;
    case 1:
        p->move_vel[0] = div_pow2(p->target_move_vel[0] * 4000, 12);
        p->move_vel[2] = div_pow2(p->target_move_vel[2] * 4000, 12);
        p->unk_ee = 0; p->anim_frame++; break;
    case 2: {
        int32_t fx = p->facing[0], fz = p->facing[2];
        p->anim_frame++;
        p->move_vel[0] = (((fx - (fx >> 31)) >> 1) * stick_mag) >> 2;
        p->move_vel[2] = (((fz - (fz >> 31)) >> 1) * stick_mag) >> 2;
        break; }
    case 3:
        if (pad_class == 0 || in->stick_cam[0] < 1) { p->move_vel[0] = p->move_vel[2] = 0; }
        else {
            p->move_vel[0] = div_pow2(p->facing[0] * stick_mag, 3);
            p->move_vel[2] = div_pow2(p->facing[2] * stick_mag, 3);
        }
        p->anim_lerp = (uint16_t)(p->anim_lerp + in->stick_mag * 0x20);
        if (p->anim_lerp > 0xFFF) { p->anim_lerp -= 0x1000; p->anim_frame++; }
        break;
    case 4: a = p->facing[0] * stick_mag; b = p->facing[2] * stick_mag; goto speed_move;
    case 7: a = in->cam_dir[0] * stick_mag; b = in->cam_dir[2] * stick_mag;
    speed_move:
        p->anim_frame++;
        p->move_vel[0] = sra(sra(a, 7) * d->params.move_speed, 5);
        p->move_vel[2] = sra(sra(b, 7) * d->params.move_speed, 5);
        break;
    case 5:
        p->anim_frame++;
        p->move_vel[0] = sra(stick_mag * in->cam_dir[0] * d->params.unk_1c, 10);
        p->move_vel[2] = sra(stick_mag * in->cam_dir[2] * d->params.unk_1c, 10);
        break;
    case 6:
        p->move_vel[0] = p->move_vel[2] = 0; p->unk_ee = 0; p->anim_blend = 0xFFFF;
        p->anim_frame = (uint16_t)(p->anim_frame + ((uint16_t)(p->anim_lerp + p->anim_speed) >> 12));
        p->anim_lerp = (uint16_t)((p->anim_lerp + p->anim_speed) & 0xFFF);
        break;
    case 8: p->unk_ee = 0; p->move_vel[0] = p->move_vel[2] = 0; p->anim_frame++; break;
    case 9: p->move_vel[1] = 0; p->move_vel[0] = p->move_vel[2] = 0; p->anim_frame++; break;
    case 10: p->move_vel[0] = (int32_t)in->cam_dir[0] * 8; p->move_vel[2] = (int32_t)in->cam_dir[2] * 8; p->anim_frame++; break;
    case 0xB:
        if (pad_class == 0) p->move_vel[0] = p->move_vel[2] = 0;
        else { p->move_vel[0] = div_pow2(p->facing[0] * stick_mag, 3); p->move_vel[2] = div_pow2(p->facing[2] * stick_mag, 3); }
        p->anim_frame++; break;
    case 0xE:
        p->move_vel[0] = div_pow2(p->target_move_vel[0] * 0xED8, 12);
        p->move_vel[2] = div_pow2(p->target_move_vel[2] * 0xED8, 12);
        p->move_vel[1] = div_pow2(p->move_vel[1] * 0xED8, 12);
        p->anim_frame++; break;
    case 0xF:
        p->move_vel[0] = sra(p->facing[0] * d->params.move_speed, 3); p->move_vel[2] = sra(p->facing[2] * d->params.move_speed, 3);
        ae_player_accel_xz(p, 0x6000); p->anim_frame++; break;
    case 0x10:
        p->move_vel[0] = sra(p->facing[0] * d->params.move_speed, 4); p->move_vel[2] = sra(p->facing[2] * d->params.move_speed, 4);
        if (p->dash_boost) { p->dash_boost = 0; p->target_move_vel[0] = p->move_vel[0]; p->target_move_vel[2] = p->move_vel[2]; emit(p, AE_EV_SFX, 0x27, 0, 0); }
        p->anim_frame++; break;
    default: unsup |= AE_UNSUP_MOVE_MODE; break;
    }

    int16_t normal[3] = { 0, 0x200, 0 };
    int skip_transitions = 0;
    if (p->health < 1) {
        if ((p->state_flags & 0xF0) != 0xE0) {
            ae_player_set_state(p, d, 0x22);
            emit(p, AE_EV_XA, (in->frame_counter & 1) ? 0xD : 0xF, 0, 0);
        }
    } else {
        int16_t slope_n[3] = { (int16_t)-p->slope_x, 0x200, (int16_t)-p->slope_z };
        ae_player_normalize(d, slope_n, normal);
        if (p->pos[1] < p->ground_y) {                                       /* airborne */
            p->surface_kind = -1;
            ae_player_accel_xz(p, p->state == 0x2B ? 0x3000 : 0x1000);
        } else if (!d->surface_loaded) {
            unsup |= AE_UNSUP_COND; /* surface table missing: ground movement cannot be reproduced */
        } else {
            uint16_t flags = g->surface_flags;
            int kind = (flags >> 1) & 7;
            int32_t l = ae_player_vec_length3(p->slope_x, 0x200, 0);
            if (l != 0) p->move_vel[0] = (p->move_vel[0] * 512) / l;
            l = ae_player_vec_length3(p->slope_z, 0x200, 0);
            if (l != 0) p->move_vel[2] = (p->move_vel[2] * 512) / l;
            if ((flags & 1) == 0) {
                if (kind >= 6) {                                             /* steep / sliding surface kinds 6 and 7 */
                    ae_player_accel_xz(p, 0);
                    if ((p->state_flags & 0xF0) != 0xA0) {
                        /* PlayerOnWallBump (800668a0) */
                        if (p->health > 0) {
                            if (p->move_vel[1] < 0) p->move_vel[1] = 0;
                            emit(p, AE_EV_SFX, 0x2B, 0, 0);
                            if ((int32_t)p->facing[0] * normal[0] + (int32_t)p->facing[2] * normal[2] < 1) {
                                ae_player_set_state(p, d, 0x42);
                                p->move_vel[0] /= 2; p->move_vel[2] /= 2;
                            } else {
                                ae_player_set_state(p, d, 0x2F);
                                p->move_vel[0] = sra(p->move_vel[0] + (p->move_vel[0] < 0 ? 3 : 0), 2);
                                p->move_vel[2] = sra(p->move_vel[2] + (p->move_vel[2] < 0 ? 3 : 0), 2);
                            }
                            p->unk_ee = 0;
                        }
                    }
                    if (p->vel[1] < 0) { p->target_move_vel[0] = p->move_vel[0] >> 1; p->target_move_vel[2] = p->move_vel[2] >> 1; }
                    p->move_vel[0] += p->slope_x * 0x38; p->move_vel[2] += p->slope_z * 0x38;
                    ae_player_accel_xz(p, 0x4000);
                } else {
                    int32_t limit = surface_s16(d, kind, 0xE);
                    int run_walk_block = 1;
                    if (p->unk_ee != 0 && !(limit < p->slope_x || p->slope_x < -limit || limit < p->slope_z || p->slope_z < -limit))
                        run_walk_block = 0;
                    if (run_walk_block) {
                        ae_player_accel_xz(p, surface_s32(d, kind, 4));
                        p->unk_ee = 0;
                        p->move_vel[0] += p->slope_x * 0x38; p->move_vel[2] += p->slope_z * 0x38;
                    }
                    if (((p->attach_flags & 0x4000000) || p->surface_kind == -1) && (p->state_flags & 0xF0) != 0xA0 && (p->state_flags & 0xF0) != 0xC0) {
                        int32_t wall = surface_s16(d, kind, 0xA);
                        if (!(abs32(p->slope_x) <= wall && abs32(p->slope_z) <= wall)) {
                            if (p->health > 0) {                              /* PlayerOnWallBump */
                                if (p->move_vel[1] < 0) p->move_vel[1] = 0;
                                emit(p, AE_EV_SFX, 0x2B, 0, 0);
                                if ((int32_t)p->facing[0] * normal[0] + (int32_t)p->facing[2] * normal[2] < 1) {
                                    ae_player_set_state(p, d, 0x42); p->move_vel[0] /= 2; p->move_vel[2] /= 2;
                                } else {
                                    ae_player_set_state(p, d, 0x2F);
                                    p->move_vel[0] = sra(p->move_vel[0] + (p->move_vel[0] < 0 ? 3 : 0), 2);
                                    p->move_vel[2] = sra(p->move_vel[2] + (p->move_vel[2] < 0 ? 3 : 0), 2);
                                }
                                p->unk_ee = 0;
                            }
                        }
                    }
                    if (p->unk_ee == 0) {
                        /* PlayerSpawnDust: cosmetic */
                        if ((p->state_flags & 0xF0) == 0xA0 && p->vel[1] < 0) { p->target_move_vel[0] = p->move_vel[0] >> 1; p->target_move_vel[2] = p->move_vel[2] >> 1; }
                        if (ae_player_accel_xz(p, surface_s32(d, kind, 4)) == 0) p->unk_ee = 1;
                    } else {
                        if (ae_player_accel_xz(p, surface_s32(d, kind, 0)) != 0) {
                            p->unk_ee = 0;
                            if (kind == 5 && (p->state_flags & 0xF0) == 0x30 && (in->frame_counter & 0x20)) emit(p, AE_EV_SFX, 0x83, 0, 0);
                        }
                    }
                }
            } else if (kind == 0) {                                           /* death floor */
                player_damage(p, 5); ae_player_set_state(p, d, 0x2D); emit(p, AE_EV_VOICE, 10, 0, 0); skip_transitions = 1;   /* original jumps straight to the tail */
            } else if (kind < 4 && p->state != 0x4D) {                        /* damaging floors */
                ae_player_set_state(p, d, 0x4D); emit(p, AE_EV_FX, 4, kind, 0); emit(p, AE_EV_RUMBLE, 1, 0x96, 0xC);
                p->contact_cooldown = 0xF; player_damage(p, 1);
                emit(p, AE_EV_SFX, kind == 2 ? 0x33 : kind == 1 ? 0x34 : 0x35, 0, 0);
            }
            if (!skip_transitions) p->surface_kind = (int16_t)kind;     /* the death-floor path jumps over this store */
        }
    }
    (void)normal;

    if (!skip_transitions) {
        run_transitions(&ctx, d->set0.transitions);
        /* the gadget transition set only runs for gadgets other than 9 */
        const AePlayerSet *gs = gadget_set(d, p->current_gadget);
        if (gs && gs->transitions) run_transitions(&ctx, gs->transitions);
    }
    se = entry_for(d, p->state);
    if (se && se->frame_action && !act_run(&ctx, se->frame_action)) unsup |= AE_UNSUP_FRAME_ACTION;

    /* PlayerOnContact (80066fc8): the timer and the contact classes that need no object table. */
    {
        int32_t timer = p->invuln_timer - 1;
        p->invuln_timer = (int16_t)(timer < 0 ? 0 : timer > 0x96 ? 0x96 : timer);
        if (p->state == 0x4D) {                                   /* timed launch: held pose, then a launch into 0x2B */
            if (--p->contact_cooldown == 0) { p->move_vel[1] = -0x80000; ae_player_set_state(p, d, 0x2B); emit(p, AE_EV_VOICE, 0xB, 0, 0); emit(p, AE_EV_FX, 1, 0, 0); }
        } else if ((p->state_flags & 0xF0) == 0xD0 || p->state == 0x4C || p->state == 0x22 || p->hurt_blink > 0) p->hurt = 0;
        else if (p->health > 0) {
            uint32_t type = p->contact >> 28;
            if (type == 4 || type == 5) {                                           /* object push-off: needs the object table */
                if (!((p->ground_type >> 12) == 1 || p->ground_y < p->pos[1] + 0x20)) unsup |= AE_UNSUP_CONTACT;
            } else if (type == 0xB) unsup |= AE_UNSUP_CONTACT;                    /* hazard objects */
            else p->hurt = 0;
        }
    }
    se = entry_for(d, p->state);
    if (se) p->state_flags = (uint32_t)(se->flags & 0xF) | ((uint32_t)(se->flags >> 4) << 4);
    if (se && (se->turn_tilt >> 4) != 0) unsup |= AE_UNSUP_TILT;
    for (int i = 0; i < 4; ++i) p->prev_facing[i] = p->facing[i];

    /* common tail: clamp, fall counter, integrate to vel / vel_frac */
    int32_t mx = p->move_vel[0] + p->vel_frac[0], my = p->move_vel[1] + p->vel_frac[1], mz = p->move_vel[2] + p->vel_frac[2];
    p->move_vel[0] = mx < -0x40000 ? -0x40000 : mx > 0x40000 ? 0x40000 : mx;
    p->move_vel[2] = mz < -0x40000 ? -0x40000 : mz > 0x40000 ? 0x40000 : mz;
    p->move_vel[1] = my < -0x80000 ? -0x80000 : my > 0x80000 ? 0x80000 : my;
    p->fall_frames = p->move_vel[1] == 0x80000 ? (int16_t)(p->fall_frames + 1) : 0;
    p->attach_flags = 0;
    for (int i = 0; i < 3; ++i) {
        p->vel[i] = div_pow2(p->move_vel[i], 12);
        p->vel_frac[i] = p->move_vel[i] + p->vel[i] * -0x1000;
    }
    p->target_move_vel[0] = p->vel[0]; p->target_move_vel[1] = p->vel[1]; p->target_move_vel[2] = p->vel[2];
    /* PlayerAnimateAndDraw (0x80063628) is called here, after the attach_flags clear and before the event
       scripts and the blend decrement.  Only its scalar side effect is modelled (no decode/matrix/draw):
       when (state_flags & 0xF0) == 0x20 or anim_blend == 0xFFFF it stores next_frame = anim_frame + 1
       (sh at 0x800636A4).  The 0xFFFE, 0 and counting-blend paths only read next_frame.  The original
       returns early when g_PlayerMode == 0xC; that mode is not reached by this function. */
    if ((p->state_flags & 0xF0) == 0x20 || p->anim_blend == 0xFFFF) p->next_frame = (int16_t)(uint16_t)(p->anim_frame + 1);
    run_events(&ctx, &d->set0);
    { const AePlayerSet *gs = gadget_set(d, p->current_gadget); if (gs) run_events(&ctx, gs); }
    if (p->anim_blend < 0xFFFE) { if (p->anim_blend) p->anim_blend--; } else p->anim_blend = 0;
    if (p->squash_timer > 0) p->squash_timer--;
    if (p->hurt_blink > 0) p->hurt_blink--;
    p->unsupported_total |= unsup;
    return unsup;
}

/* IsPlayerSwimming @80068B7C: states 0x46 0x47 0x49 0x4A 0x4C. */
int ae_player_is_swimming(uint16_t state) {
    return (uint32_t)(state - 0x46u) < 2u || state == 0x49 || state == 0x4A || state == 0x4C;
}

/* CanSwitchGadget @80066F50: g_PlayerMode (u16, 0x800F44A0) 9 / 0xB, 1..3 never; otherwise not while swimming. */
int ae_player_can_switch_gadget(uint16_t player_mode, uint16_t state) {
    if (player_mode == 9) return 0;
    if (player_mode < 10) {
        if (player_mode < 4 && player_mode != 0) return 0;
    } else if (player_mode == 0xB) return 0;
    return !ae_player_is_swimming(state);
}

/* PlayerCheckGadgetSelect @80067974 (decision part): face buttons Triangle / Square / Circle / Cross = 0x10 / 0x80 / 0x20 / 0x40
 * pick slot 0 / 1 / 2 / 3 (a later test wins); an empty slot (< 0) does nothing.  Returns 1 and the gadget id when the
 * original calls PlayerSelectGadget(slots[slot], g_Player.currentGadget). */
int ae_player_gadget_select_request(uint16_t player_mode, uint16_t state, uint32_t held, uint32_t changed, const int8_t slots[4], int *gadget) {
    if (!ae_player_can_switch_gadget(player_mode, state)) return 0;
    uint32_t pressed = changed & held;
    int slot = -1;
    if (pressed & 0x10) slot = 0;
    if (pressed & 0x80) slot = 1;
    if (pressed & 0x20) slot = 2;
    if (pressed & 0x40) slot = 3;
    if (slot < 0 || slots[slot] < 0) return 0;
    *gadget = slots[slot];
    return 1;
}

/* PlayerSelectGadget @8007AC64 (state part; the per-gadget globals, sounds and effects are host work): current / saved
 * gadget, then when the gadget changed: the 0x24 equip SFX request (not for a first selection from -1 or for "none"),
 * a gadget state (>= 0x80) falls back to state 0x24 in the air / 0 on the ground, then PlayerApplyGadgetStance runs in
 * ae_player_update. Returns 1 when the gadget changed. */
int ae_player_select_gadget_from(AePlayer *p, const AePlayerData *d, int gadget, int old) {
    if (gadget != AE_PLAYER_NO_GADGET && !gadget_set(d, gadget)) return 0;
    p->current_gadget = (int8_t)gadget;
    p->saved_gadget = (int8_t)old;
    if (old == gadget) return 0;
    if (old != -1 && gadget != AE_PLAYER_NO_GADGET) emit(p, AE_EV_SFX, 0x24, 0, 0);
    if (p->state >= 0x80) ae_player_set_state(p, d, p->pos[1] < p->ground_y ? 0x24 : 0);
    return 1;
}
int ae_player_select_gadget(AePlayer *p, const AePlayerData *d, int gadget) {
    return ae_player_select_gadget_from(p, d, gadget, p->current_gadget);
}
