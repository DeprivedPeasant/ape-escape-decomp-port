/* Bounded 44.1 kHz reverb pipeline: FIR down -> 22.05 kHz SpReverb network -> FIR up -> output volume.
 * Header-only C11. See reverb_pipeline.md for lifetime, size and integration notes. */
#ifndef AE_REVERB_PIPELINE_H
#define AE_REVERB_PIPELINE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sp_reverb.h"
#include "reverb_fir.h"

#define AE_REVERB_PIPELINE_RAM_WORDS 262144u

/* About 512 KiB + small state: allocate on the heap or statically, never on the stack.
 * network.ram points into this struct's own ram[]: do not copy or move by value;
 * call ae_reverb_pipeline_init again after any relocation. */
typedef struct AeReverbPipeline {
    int16_t ram[AE_REVERB_PIPELINE_RAM_WORDS];
    SpReverb network;
    AeReverbFir filter;
    int16_t left_volume;
    int16_t right_volume;
} AeReverbPipeline;

/* preset[0] = work-area base in 4-word units; preset[1..32] = network registers.
 * Returns 0 on success, -1 if p or preset is NULL. Clears all RAM and filter state. */
static inline int ae_reverb_pipeline_init(AeReverbPipeline *p, const uint16_t preset[33],
                                          int16_t left_volume, int16_t right_volume)
{
    int i;
    if (!p || !preset)
        return -1;
    memset(p->ram, 0, sizeof p->ram);
    memset(&p->network, 0, sizeof p->network);
    ae_reverb_fir_reset(&p->filter);
    for (i = 0; i < 32; i++)
        p->network.reg[i] = preset[i + 1];
    /* max 65535*4 = 262140 < 262144, so every uint16 base is in range */
    p->network.base = (uint32_t)preset[0] * 4u;
    p->network.cursor = p->network.base;
    p->network.ram = p->ram;
    p->left_volume = left_volume;
    p->right_volume = right_volume;
    return 0;
}

static inline int32_t ae_reverb_pipeline_volume(int16_t sample, int16_t volume)
{
    return (int32_t)ae_reverb_fir_floor_shr((int64_t)sample * (int64_t)volume, 15);
}

/* One 44.1 kHz tick. input: clamped signed16 summed wet send. output: wet return after
 * volume, int32 (may reach +32768); final mix clipping is the caller's job. */
static inline void ae_reverb_pipeline_step(AeReverbPipeline *p, const int16_t input[2],
                                           int32_t output[2])
{
    int16_t down[2] = {0, 0};
    int16_t wet[2] = {0, 0};
    int16_t out[2];
    if (ae_reverb_fir_push(&p->filter, input, down)) {
        int net[2];
        sp_reverb_step(&p->network, down[0], down[1], net);
        wet[0] = (int16_t)net[0]; /* sp_reverb_step outputs are already clipped to signed16 */
        wet[1] = (int16_t)net[1];
    }
    ae_reverb_fir_pull(&p->filter, wet, out); /* wet is only consumed on odd phase */
    output[0] = ae_reverb_pipeline_volume(out[0], p->left_volume);
    output[1] = ae_reverb_pipeline_volume(out[1], p->right_volume);
}

#endif
