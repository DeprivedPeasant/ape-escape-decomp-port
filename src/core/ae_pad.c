/*
 * Controller input (PadUpdate 0x8001325C and helpers) ported instruction for instruction onto an AeRam image.
 * See ae_pad.h for the data layout.  Every block names the original address it translates; integer widths,
 * signedness and the order of reads / writes follow the listings (diffed by tools/mips/check_player_input.py).
 */
#include "ae_pad.h"
#include "ae_vecnorm.h"

#define B AE_PAD_STICK_HISTORY

static int16_t s16(uint32_t v) { return (int16_t)(uint16_t)v; }

#define HOOK(h, f) ((h) ? (h)->f : 0)
#define MISSING(r, name) ae_ram_fail((r), "PadUpdate: hook %s not provided", (name))

/* ------------------------------------------------------------------------------------------------ SDK */

/* Core 0x80030210 on the three register values (already reduced to their low 16 bits by mtc2); out receives the three
 * 32-bit results.  Returns v0 (sum of squares).  A trapping `add` is flagged and writes nothing. */
static int32_t vecnorm_core(AeRam *r, const int32_t ir[3], int32_t out[3], const char *who) {
    AeVecNormPlan p = ae_vecnorm_plan(ir);
    if (p.trapped) { ae_ram_fail(r, "%s: add overflow (the original traps)", who); out[0] = out[1] = out[2] = 0; return 0; }
    int32_t ir0 = p.sum != 0 ? ae_rds16(r, AE_VECNORM_LUT_ADDR + 2u * (uint32_t)p.index) : 0;
    for (int i = 0; i < 3; ++i) out[i] = ae_vecnorm_apply(ir0, ir[i], p.shift);
    return p.sum;
}

static int32_t normss_values(AeRam *r, int32_t x, int32_t y, int32_t z, int16_t out[3]) {
    int32_t ir[3] = { (int16_t)x, (int16_t)y, (int16_t)z }, o[3];
    int32_t v0 = vecnorm_core(r, ir, o, "VectorNormalSS");
    for (int i = 0; i < 3; ++i) out[i] = (int16_t)o[i];
    return v0;
}

/* VectorNormalSS @800301E0 */
int32_t ae_pad_VectorNormalSS(AeRam *r, uint32_t in, uint32_t out) {
    int32_t x = ae_rds16(r, in), y = ae_rds16(r, in + 2), z = ae_rds16(r, in + 4);
    int16_t o[3];
    int32_t v0 = normss_values(r, x, y, z, o);
    for (uint32_t i = 0; i < 3; ++i) ae_wr16(r, out + 2 * i, (uint16_t)o[i]);
    return v0;
}

/* VectorNormal @800301B0 */
int32_t ae_pad_VectorNormal(AeRam *r, uint32_t in, uint32_t out) {
    uint32_t w[3] = { ae_rd32(r, in), ae_rd32(r, in + 4), ae_rd32(r, in + 8) };
    int32_t ir[3] = { s16(w[0]), s16(w[1]), s16(w[2]) }, o[3];
    int32_t v0 = vecnorm_core(r, ir, o, "VectorNormal");
    if (ae_ram_failed(r)) return v0;           /* a trap: the original never reaches the stores */
    for (uint32_t i = 0; i < 3; ++i) ae_wr32(r, out + 4 * i, (uint32_t)o[i]);
    return v0;
}

/* ApplyMatrixSV @800300B0: ctc2 x5 (the matrix), lwc2 VXY0 / VZ0, MVMVA sf=1 rotation * V0 with no translation. */
uint32_t ae_pad_ApplyMatrixSV(AeRam *r, uint32_t matrix, uint32_t in, uint32_t out) {
    uint32_t w[5];
    for (int i = 0; i < 5; ++i) w[i] = ae_rd32(r, matrix + 4u * (uint32_t)i);
    int32_t m[3][3] = { { s16(w[0]), s16(w[0] >> 16), s16(w[1]) },
                        { s16(w[1] >> 16), s16(w[2]), s16(w[2] >> 16) },
                        { s16(w[3]), s16(w[3] >> 16), s16(w[4]) } };
    uint32_t vxy = ae_rd32(r, in), vz0 = ae_rd32(r, in + 4);
    int32_t v[3] = { s16(vxy), s16(vxy >> 16), s16(vz0) };
    for (int i = 0; i < 3; ++i) {
        int64_t mac = ((int64_t)m[i][0] * v[0] + (int64_t)m[i][1] * v[1] + (int64_t)m[i][2] * v[2]) >> 12;
        int32_t ir = mac > 32767 ? 32767 : mac < -32768 ? -32768 : (int32_t)mac;      /* IRn, lm = 0 */
        ae_wr16(r, out + 2u * (uint32_t)i, (uint16_t)ir);
    }
    return out;
}

/* QuadrantFromSigns @8001413C */
int32_t ae_pad_QuadrantFromSigns(int32_t a, int32_t b) {
    if (a > 0) return b >= 0 ? 0 : 3;
    return b > 0 ? 1 : 2;
}

/* ------------------------------------------------------------------------------------------------ pad driver */

/* PadNormalizeSticks @800130E0 */
void ae_pad_PadNormalizeSticks(AeRam *r, uint32_t buf) {
    uint32_t v1 = ae_rd8(r, buf);
    if (v1 == 0xFF) { ae_wr8(r, buf + 2, v1); ae_wr8(r, buf + 3, v1); }
    if ((ae_rd8(r, buf + 1) & 0xF) < 3) {
        for (uint32_t i = 4; i < 8; ++i) ae_wr8(r, buf + i, 0x80);
    }
}

/* PadSampleStickHistory @8001300C */
void ae_pad_PadSampleStickHistory(AeRam *r) {
    uint32_t idx = (ae_rd32(r, AE_PAD_HISTORY_POS) + 1) % 40u;
    if (ae_rd32(r, AE_PAD_DEMO_FLAG) != 0) return;
    if ((ae_rd8(r, AE_PAD_BUF1 + 1) & 0xF) < 3) for (uint32_t i = 4; i < 8; ++i) ae_wr8(r, AE_PAD_BUF1 + i, 0x80);
    uint32_t e = B + idx * 4;
    ae_wr8(r, e + 0, ae_rd8(r, AE_PAD_BUF1 + 4) + 0x80);
    ae_wr8(r, e + 1, 0x7F - ae_rd8(r, AE_PAD_BUF1 + 5));
    ae_wr8(r, e + 2, ae_rd8(r, AE_PAD_BUF1 + 6) + 0x80);
    ae_wr8(r, e + 3, 0x7F - ae_rd8(r, AE_PAD_BUF1 + 7));
    ae_wr16(r, AE_PAD_STICK_DONE + idx * 2, 0);
    ae_wr32(r, AE_PAD_HISTORY_POS, idx);
}

/* Repeat bookkeeping of one port (0x80013420 .. 0x800134D4). */
static void repeat_update(AeRam *r, uint32_t held_a, uint32_t changed_a, uint32_t repeat_a, uint32_t timer_a) {
    uint32_t held = ae_rd32(r, held_a), repeat = held & ae_rd32(r, changed_a);
    if (repeat == 0) {
        int32_t t = ae_rds16(r, timer_a);
        if (t < 0x11) { ae_wr16(r, timer_a, (uint32_t)t + 1); ae_wr32(r, repeat_a, 0); }
        else { ae_wr32(r, repeat_a, held & 0xF000); ae_wr16(r, timer_a, (uint32_t)t - 4); }
    } else {
        ae_wr16(r, timer_a, 0);
        ae_wr32(r, repeat_a, repeat);
    }
}

/* The rotation sense over the last 30 samples (0x80013A4C / 0x80013B7C): sums +-(0x1E - k) / 4 whenever the
 * quadrant of the sample k frames ago differs by one step from the one after it. */
static int32_t spin_count(AeRam *r, uint32_t i) {
    int32_t s1 = 0;
    int32_t s3 = ae_pad_QuadrantFromSigns(ae_rds8(r, B + i * 4), ae_rds8(r, B + i * 4 + 1));
    for (int32_t s0 = 1; s0 < 0x1E; ++s0) {
        uint32_t idx = (uint32_t)((int32_t)i - s0 + 0x28) % 40u;
        int32_t a0 = ae_pad_QuadrantFromSigns(ae_rds8(r, B + idx * 4), ae_rds8(r, B + idx * 4 + 1));
        int32_t d = a0 - s3;
        if (d == 1 || d == -3) {
            int32_t v0 = 0x1E - s0;
            if (v0 < 0) v0 += 3;
            s1 += v0 >> 2;
            d = a0 - s3;
        }
        if (d == -1 || d == 3) {
            int32_t v0 = 0x1E - s0;
            if (v0 < 0) v0 += 3;
            s1 -= v0 >> 2;
        }
        s3 = a0;
    }
    return s1;
}

/* mag = ((v >> 7) - 0x15 with a dead zone) * 0x7F / 0x6A, clamped to 0x7F, stored as u16 (0x800135F0 .. 0x80013650). */
static void stick_magnitude(AeRam *r, uint32_t mag_a, int32_t v0) {
    int32_t v1 = (v0 < 0 ? v0 + 0x7F : v0) >> 7;
    ae_wr16(r, mag_a, (uint32_t)v1);
    if (((uint32_t)v1 & 0xFFFF) < 0x15) ae_wr16(r, mag_a, 0);
    else ae_wr16(r, mag_a, (uint32_t)(v1 - 0x15));
    int32_t q = (int32_t)(ae_rd16(r, mag_a) * 127u) / 0x6A;
    ae_wr16(r, mag_a, (uint32_t)q);
    if (((uint32_t)q & 0xFFFF) >= 0x80) ae_wr16(r, mag_a, 0x7F);
}

/* (a * b) with the "if (v < 0) v += 0xFFF; v >> 12" rounding toward zero. */
static int32_t div4096(int32_t v) { return (v < 0 ? v + 0xFFF : v) >> 12; }

void ae_pad_PadUpdate(AeRam *r, const AePadHooks *h, uint32_t flags) {
    /* --- DualShock setup (0x8001325C .. 0x8001332C) */
    {
        int32_t state;
        if (!HOOK(h, pad_get_state)) { MISSING(r, "pad_get_state"); state = 0; }
        else state = h->pad_get_state(h->user, r, 0);
        int do_setup = 0;
        int32_t arg = 0;
        if (state == 1) {
            if ((ae_rd32(r, AE_PAD_DUALSHOCK_STATE) & 1) == 0) do_setup = 1;
        } else if (state < 2) {
            if (state == 0) do_setup = 1;
        } else if (state == 6) {
            if (ae_rd32(r, AE_PAD_DUALSHOCK_STATE) != 3) { do_setup = 1; arg = 1; }
            else {
                int32_t mode = 0;
                if (!HOOK(h, pad_info_mode)) MISSING(r, "pad_info_mode");
                else mode = h->pad_info_mode(h->user, r, 0, 1, 0);
                if (mode != 7) do_setup = 1;
            }
        }
        if (do_setup) {
            if (!HOOK(h, pad_setup_dualshock)) MISSING(r, "pad_setup_dualshock");
            else h->pad_setup_dualshock(h->user, r, arg);
        }
    }
    /* --- demo playback */
    if (ae_rd32(r, AE_PAD_DEMO_FLAG) != 0) {
        int32_t ret = 0;
        if (!HOOK(h, pad_demo_step)) MISSING(r, "pad_demo_step");
        else ret = h->pad_demo_step(h->user, r);
        if (ret != 0) {
            if (!HOOK(h, stage_request)) MISSING(r, "stage_request");
            else h->stage_request(h->user, r, ret, (int32_t)ae_rd16(r, AE_G_CURRENT_AREA), (int32_t)ae_rd16(r, AE_G_ENTRY_INDEX));
        }
    }
    /* --- port 2 */
    ae_wr16(r, AE_PAD_ID2, (ae_rd8(r, AE_PAD_BUF2) << 8) | ae_rd8(r, AE_PAD_BUF2 + 1));
    ae_pad_PadNormalizeSticks(r, AE_PAD_BUF2);
    {
        uint32_t v = ~(ae_rd8(r, AE_PAD_BUF2 + 3) | (ae_rd8(r, AE_PAD_BUF2 + 2) << 8));
        uint32_t old = ae_rd32(r, AE_PAD2_HELD);
        ae_wr32(r, AE_PAD2_HELD, v);
        ae_wr32(r, AE_PAD2_CHANGED, old ^ v);
    }
    /* --- port 1 */
    if (ae_rd32(r, AE_PAD_DEMO_FLAG) == 0) {
        ae_wr16(r, AE_PAD_ID1, (ae_rd8(r, AE_PAD_BUF1) << 8) | ae_rd8(r, AE_PAD_BUF1 + 1));
        ae_pad_PadNormalizeSticks(r, AE_PAD_BUF1);
        uint32_t v = ~(ae_rd8(r, AE_PAD_BUF1 + 3) | (ae_rd8(r, AE_PAD_BUF1 + 2) << 8));
        if (flags & AE_PADUPDATE_STICK_BUTTONS) {
            if (!HOOK(h, stick_to_buttons)) { MISSING(r, "stick_to_buttons"); v = 0; }
            else v = h->stick_to_buttons(h->user, r);
        }
        uint32_t old = ae_rd32(r, AE_PAD_HELD);
        ae_wr32(r, AE_PAD_HELD, v);
        ae_wr32(r, AE_PAD_CHANGED, old ^ v);
    }
    repeat_update(r, AE_PAD_HELD, AE_PAD_CHANGED, AE_PAD_REPEAT, AE_PAD_REPEAT_TIMER);
    repeat_update(r, AE_PAD2_HELD, AE_PAD2_CHANGED, AE_PAD2_REPEAT, AE_PAD2_REPEAT_TIMER);

    /* --- stick history: find the newest processed sample, then process every sample after it */
    uint32_t pos = ae_rd32(r, AE_PAD_HISTORY_POS);
    int32_t s5 = 0x28;
    {
        int32_t a0 = (int32_t)pos + s5;
        for (;;) {
            uint32_t idx = (uint32_t)a0 % 40u;
            if (ae_rd16(r, AE_PAD_STICK_DONE + idx * 2) != 0) break;
            --s5;
            if (!(s5 > 0)) break;
            a0 = (int32_t)pos + s5;
        }
    }
    ++s5;
    ae_wr16(r, AE_PAD_GESTURE_EVENTS, 0);
    while (s5 < 0x29) {
        uint32_t i = (pos + (uint32_t)s5) % 40u;                      /* s4 */
        if (ae_rd16(r, AE_PAD_STICK_DONE + i * 2) != 0) break;
        uint32_t ent = B + i * 4;
        /* gadget stick ("L": bytes 0 / 1) */
        int32_t v0;
        {   /* the SVECTOR result goes to g_LStickDir (x, y, z) */
            int16_t o[3];
            v0 = normss_values(r, ae_rds8(r, ent), 0, ae_rds8(r, ent + 1), o);
            for (uint32_t k = 0; k < 3; ++k) ae_wr16(r, AE_PAD_LSTICK_DIR + 2 * k, (uint16_t)o[k]);
        }
        stick_magnitude(r, AE_PAD_LSTICK_MAG, v0);
        ae_wr8(r, AE_PAD_STICK_LMAG_HIST + i, ae_rd8(r, AE_PAD_LSTICK_MAG));
        /* movement stick ("R": bytes 2 / 3) */
        {
            int16_t o[3];
            v0 = normss_values(r, ae_rds8(r, ent + 2), 0, ae_rds8(r, ent + 3), o);
            for (uint32_t k = 0; k < 3; ++k) ae_wr16(r, AE_PAD_RSTICK_DIR + 2 * k, (uint16_t)o[k]);
        }
        stick_magnitude(r, AE_PAD_RSTICK_MAG, v0);
        ae_wr16(r, AE_PAD_STICK_DONE + i * 2, 1);
        /* the stored sample becomes direction * magnitude */
        {
            uint32_t lm = ae_rd16(r, AE_PAD_LSTICK_MAG), rm = ae_rd16(r, AE_PAD_RSTICK_MAG);
            ae_wr8(r, ent + 0, (uint32_t)(div4096(ae_rds16(r, AE_PAD_LSTICK_DIR) * (int32_t)lm)));
            ae_wr8(r, ent + 1, (uint32_t)(div4096(ae_rds16(r, AE_PAD_LSTICK_DIR + 4) * (int32_t)lm)));
            ae_wr8(r, ent + 2, (uint32_t)(div4096(ae_rds16(r, AE_PAD_RSTICK_DIR) * (int32_t)rm)));
            ae_wr8(r, ent + 3, (uint32_t)(div4096(ae_rds16(r, AE_PAD_RSTICK_DIR + 4) * (int32_t)rm)));
        }
        uint32_t prev = (i + 0x27) % 40u;
        uint32_t pe = B + prev * 4;
        ae_wr32(r, AE_PAD_RSTICK_SPIN, (uint32_t)(ae_rds8(r, ent + 2) * ae_rds8(r, pe + 3) - ae_rds8(r, ent + 3) * ae_rds8(r, pe + 2)));
        /* g_RStickPushState */
        {
            uint32_t st = ae_rd16(r, AE_PAD_RSTICK_PUSH), mag = ae_rd16(r, AE_PAD_RSTICK_MAG);
            uint32_t ns = 0;
            if (st == 0 || st == 2 || st == 4 || st == 8) ns = mag == 0 ? 0 : mag < 0x7D ? (mag < 0x47 ? 2u : 4u) : 8u;
            ae_wr16(r, AE_PAD_RSTICK_PUSH, ns);
        }
        ae_wr32(r, AE_PAD_LSTICK_SPIN, (uint32_t)(ae_rds8(r, ent) * ae_rds8(r, pe + 1) - ae_rds8(r, ent + 1) * ae_rds8(r, pe)));
        /* g_LStickFlickState machine (flick / push / spin / hold) */
        {
            uint32_t a0 = ae_rd16(r, AE_PAD_LSTICK_FLICK);
            if (a0 == 8 || a0 == 4) {
                ae_wr16(r, AE_PAD_LSTICK_FLICK, 0x80);
                ae_wr16(r, AE_PAD_STICK_ROTATION, 0);
            } else if (a0 == 0) {
                if (ae_rd16(r, AE_PAD_LSTICK_MAG) >= 0x7D) {
                    uint32_t pp = (i + 0x26) % 40u, ppe = B + pp * 4;
                    int32_t ax = ae_rds8(r, ent) - ae_rds8(r, pe), ay = ae_rds8(r, ent + 1) - ae_rds8(r, pe + 1);
                    int32_t bx = ae_rds8(r, ppe) - ae_rds8(r, pe), by = ae_rds8(r, ppe + 1) - ae_rds8(r, pe + 1);
                    int32_t cur = ax * ax + ay * ay, old = bx * bx + by * by;
                    if (old < cur) old = cur;
                    if (old < 0x385) { ae_wr16(r, AE_PAD_LSTICK_FLICK, 4); ae_wr16(r, AE_PAD_GESTURE_EVENTS, ae_rd16(r, AE_PAD_GESTURE_EVENTS) | 4); }
                    else { ae_wr16(r, AE_PAD_LSTICK_FLICK, 8); ae_wr16(r, AE_PAD_GESTURE_EVENTS, ae_rd16(r, AE_PAD_GESTURE_EVENTS) | 8); }
                }
                ae_wr16(r, AE_PAD_STICK_ROTATION, 0);
            } else if (a0 == 0x10 || a0 == 0x20) {
                int32_t s1 = spin_count(r, i);
                uint32_t ns = s1 >= 8 ? 0x10u : s1 < -7 ? 0x20u : 0x80u;
                ae_wr16(r, AE_PAD_LSTICK_FLICK, ns);
                ae_wr16(r, AE_PAD_STICK_ROTATION, (uint32_t)s1);
            } else if (a0 == 0x80) {
                int32_t ax = ae_rds8(r, ent), ay = ae_rds8(r, ent + 1);
                if (ax < 0) ax = -ax;
                if (ay < 0) ay = -ay;
                if (ax + ay < 0x46) {
                    ae_wr16(r, AE_PAD_LSTICK_FLICK, 0);
                } else {
                    int32_t s1 = spin_count(r, i);
                    int32_t m = s1 < 0 ? -s1 : s1;
                    uint32_t ns = m < 0xF ? 0x80u : s1 > 0 ? 0x10u : 0x20u;
                    ae_wr16(r, AE_PAD_LSTICK_FLICK, ns);
                    ae_wr16(r, AE_PAD_STICK_ROTATION, (uint32_t)s1);
                }
            } else {
                ae_wr16(r, AE_PAD_LSTICK_FLICK, 0);
            }
        }
        /* camera-relative vectors and the per-sample gesture class (flags bit 0) */
        if (flags & AE_PADUPDATE_CAMERA_RELATIVE) {
            ae_pad_ApplyMatrixSV(r, AE_G_CAMERA_ROT_MATRIX, AE_PAD_RSTICK_DIR, AE_PAD_RSTICK_CAMDIR);
            ae_pad_ApplyMatrixSV(r, AE_G_CAMERA_ROT_MATRIX, AE_PAD_LSTICK_DIR, AE_PAD_LSTICK_CAMDIR);
            int16_t n[3];
            normss_values(r, ae_rd16(r, AE_G_PLAYER_FACING), 0, ae_rd16(r, AE_G_PLAYER_FACING + 4), n);
            int32_t nx = n[0], nz = n[2];
            int32_t camx = ae_rds16(r, AE_PAD_RSTICK_CAMDIR), camz = ae_rds16(r, AE_PAD_RSTICK_CAMDIR + 4);
            ae_wr16(r, AE_PAD_RCAM_SIDE, (uint32_t)div4096(nx * camz - nz * camx));
            ae_wr16(r, AE_PAD_RCAM_FWD, (uint32_t)div4096(nx * camx + nz * camz));
            if (ae_rd32(r, AE_PAD_AIM_ALT) == 0) {
                int32_t lcx = ae_rds16(r, AE_PAD_LSTICK_CAMDIR), lcz = ae_rds16(r, AE_PAD_LSTICK_CAMDIR + 4);
                ae_wr16(r, AE_PAD_LCAM_SIDE, (uint32_t)div4096(nx * lcz - nz * lcx));
                ae_wr16(r, AE_PAD_LCAM_FWD, (uint32_t)div4096(nx * lcx + nz * lcz));
            } else {
                ae_wr16(r, AE_PAD_LCAM_SIDE, (uint32_t)(0u - ae_rd16(r, AE_PAD_LSTICK_DIR)));
                ae_wr16(r, AE_PAD_LCAM_FWD, ae_rd16(r, AE_PAD_LSTICK_DIR + 4));
            }
            int32_t cc = ae_rds16(r, AE_PAD_LCAM_FWD), ce = ae_rds16(r, AE_PAD_LCAM_SIDE);
            int ev = (ae_rd16(r, AE_PAD_LSTICK_FLICK) & 0xC) != 0;
            uint32_t cls;
            if (cc >= 0xB51 && ev) cls = 2;
            else if (ce < -0xB50 && ev) cls = 4;
            else if (ce >= 0xB51 && ev) cls = 8;
            else if (cc < -0xB50 && ev) cls = 0x40;
            else {
                int32_t spin = ae_rds32(r, AE_PAD_LSTICK_SPIN);
                cls = spin >= 0xB1E ? 0x10u : spin < -0xB1D ? 0x20u : 1u;
            }
            ae_wr16(r, AE_PAD_STICK_DONE + i * 2, cls);
        }
        ae_wr8(r, AE_PAD_STICK_FLICK_HIST + i, ae_rd8(r, AE_PAD_LSTICK_FLICK));
        ++s5;
    }
    /* --- stick travel over the whole ring */
    {
        int32_t a3 = 0, t0 = 0;
        for (uint32_t k = 0; k < 0x27; ++k) {
            uint32_t e1 = B + ((pos + k) % 40u) * 4, e2 = B + ((pos + k + 1) % 40u) * 4;
            int32_t d;
            d = ae_rds8(r, e1) - ae_rds8(r, e2);         a3 += d < 0 ? -d : d;
            d = ae_rds8(r, e1 + 1) - ae_rds8(r, e2 + 1); a3 += d < 0 ? -d : d;
            d = ae_rds8(r, e1 + 2) - ae_rds8(r, e2 + 2); t0 += d < 0 ? -d : d;
            d = ae_rds8(r, e1 + 3) - ae_rds8(r, e2 + 3); t0 += d < 0 ? -d : d;
        }
        ae_wr16(r, AE_PAD_TRAVEL_L, (uint32_t)a3);
        ae_wr16(r, AE_PAD_TRAVEL_R, (uint32_t)t0);
    }
    /* --- rumble timers */
    for (uint32_t port = 0; port < 2; ++port) {
        uint32_t a = AE_PAD_RUMBLE_TIMER0 + port;
        uint32_t c = ae_rd8(r, a);
        int reset = 1;
        if (c != 0) {
            ae_wr8(r, a, c - 1);
            reset = ((c - 1) & 0xFF) == 0;
        }
        if (reset) {
            if (!HOOK(h, pad_rumble_reset)) MISSING(r, "pad_rumble_reset");
            else h->pad_rumble_reset(h->user, r, (int32_t)port);
        }
    }
    ae_wr32(r, AE_PAD_STICK_LATEST, ae_rd32(r, B + ae_rd32(r, AE_PAD_HISTORY_POS) * 4));
}

/* CameraLookAt @80046670, up to the stores into its 4th argument (see ae_pad.h). */
void ae_pad_CameraRotMatrix(AeRam *r, const int16_t eye[3], const int16_t target[3], uint32_t matrix) {
    int32_t dx = (int32_t)target[0] - eye[0], dz = (int32_t)target[2] - eye[2];
    if (dx == 0 && dz == 0) dz = 0x1000;
    int32_t ir[3] = { (int16_t)dx, 0, (int16_t)dz }, o[3];
    vecnorm_core(r, ir, o, "VectorNormal");
    if (ae_ram_failed(r)) return;
    /* The second VectorNormal (with the y difference, for the view matrix) runs before any store: its sum of squares can
       trap too, and then the original never writes the matrix. */
    int32_t ir2[3] = { (int16_t)dx, (int16_t)((int32_t)target[1] - eye[1]), (int16_t)dz }, o2[3];
    vecnorm_core(r, ir2, o2, "VectorNormal");
    if (ae_ram_failed(r)) return;
    int32_t nx = o[0], nz = o[2];
    ae_wr16(r, matrix + 0x10, (uint16_t)nz);
    ae_wr16(r, matrix + 0x00, (uint16_t)nz);
    ae_wr16(r, matrix + 0x0E, 0);
    ae_wr16(r, matrix + 0x0A, 0);
    ae_wr16(r, matrix + 0x06, 0);
    ae_wr16(r, matrix + 0x02, 0);
    ae_wr16(r, matrix + 0x04, (uint16_t)nx);
    ae_wr16(r, matrix + 0x0C, (uint16_t)(0 - nx));
    ae_wr16(r, matrix + 0x08, 0x1000);
}
