/* Standalone integer FIR stage for PS1 reverb (44.1kHz <-> 22.05kHz). C11, header-only. */
#ifndef AE_REVERB_FIR_H
#define AE_REVERB_FIR_H

#include <stdint.h>
#include <string.h>

#define AE_REVERB_FIR_TAPS 39
#define AE_REVERB_FIR_DOWN_HIST 64
#define AE_REVERB_FIR_UP_HIST 32
#define AE_REVERB_FIR_UP_TAPS 20

typedef struct AeReverbFir {
    uint32_t phase;                                  /* 0..63 */
    int16_t down_hist[2][AE_REVERB_FIR_DOWN_HIST];   /* 44.1kHz input history */
    int16_t up_hist[2][AE_REVERB_FIR_UP_HIST];       /* 22.05kHz wet history */
} AeReverbFir;

static const int16_t ae_reverb_fir_coef[AE_REVERB_FIR_TAPS] = {
    -0x1, 0, 0x2, 0, -0xA, 0, 0x23, 0, -0x67, 0, 0x10A, 0, -0x268, 0, 0x534, 0,
    -0xB90, 0, 0x2806, 0x4000, 0x2806, 0, -0xB90, 0, 0x534, 0, -0x268, 0, 0x10A, 0,
    -0x67, 0, 0x23, 0, -0xA, 0, 0x2, 0, -0x1
};

/* Even-index coefficients of the 39-tap list (middle 0x4000 sits at odd index 19). */
static inline int32_t ae_reverb_fir_up_coef(int k)
{
    return ae_reverb_fir_coef[k * 2];
}

/* Floor (arithmetic) right shift without relying on implementation-defined behavior. */
static inline int64_t ae_reverb_fir_floor_shr(int64_t v, int s)
{
    return v >= 0 ? (v >> s) : ~((~v) >> s);
}

static inline int16_t ae_reverb_fir_clamp16(int64_t v)
{
    return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
}

static inline void ae_reverb_fir_reset(AeReverbFir *f)
{
    memset(f, 0, sizeof *f);
}

/* Store input at current phase; if phase is odd, write the 22.05kHz sample to down and return 1. */
static inline int ae_reverb_fir_push(AeReverbFir *f, const int16_t input[2], int16_t down[2])
{
    uint32_t p = f->phase & 63u;
    int ch;
    for (ch = 0; ch < 2; ch++)
        f->down_hist[ch][p] = input[ch];
    if (!(p & 1u))
        return 0;
    for (ch = 0; ch < 2; ch++) {
        int64_t sum = 0;
        int k;
        for (k = 0; k < AE_REVERB_FIR_TAPS; k++)
            sum += (int64_t)ae_reverb_fir_coef[k] *
                   f->down_hist[ch][(p - 38u + (uint32_t)k) & 63u];
        down[ch] = ae_reverb_fir_clamp16(ae_reverb_fir_floor_shr(sum, 15));
    }
    return 1;
}

/* On odd phase store wet (network output); produce one 44.1kHz output; advance phase. */
static inline void ae_reverb_fir_pull(AeReverbFir *f, const int16_t wet[2], int16_t out[2])
{
    uint32_t p = f->phase & 63u;
    uint32_t h = p >> 1;
    int ch;
    if (p & 1u) {
        for (ch = 0; ch < 2; ch++)
            f->up_hist[ch][h & 31u] = wet[ch];
        for (ch = 0; ch < 2; ch++) {
            int64_t sum = 0;
            int k;
            for (k = 0; k < AE_REVERB_FIR_UP_TAPS; k++)
                sum += (int64_t)ae_reverb_fir_up_coef(k) *
                       f->up_hist[ch][(h - 19u + (uint32_t)k) & 31u];
            out[ch] = ae_reverb_fir_clamp16(ae_reverb_fir_floor_shr(sum, 14));
        }
    } else {
        for (ch = 0; ch < 2; ch++)
            out[ch] = f->up_hist[ch][(h - 10u) & 31u];
    }
    f->phase = (p + 1u) & 63u;
}

#endif
