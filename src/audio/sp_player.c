/* Portable C player for Ape Escape sound banks; see sp_player.h. Same model as tools/render_seq.py, written as a
 * streaming renderer (events are executed at their sample time, voices are mixed sample by sample). */
#include "sp_player.h"
#include "sp_voice_pool.h"
#include "sp_adsr.h"
#include "spu_gauss.h"
#include "reverb_pipeline.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef SP_TRACE
#include <stdio.h>
#endif

#define NVOICE 24
#define MAXCH 16
#define RELEASE_CAP (SP_RATE * 4)
#define FRAME_SAMPLES (SP_RATE / 60)
#define EXE_BASE 0x80010000u
#define EXE_HDR 0x800u

static const int FILTERS[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* ------------------------------------------------------------------ samples */
typedef struct {
    uint32_t addr;
    float *pcm;
    int n;
    int loop; /* -1 = one shot */
} Sample;

static void decode_sample(const uint8_t *vb, size_t vb_size, uint32_t start, Sample *s) {
    int cap = 4096, n = 0, s1 = 0, s2 = 0, loop_start = 0, repeat = 0;
    float *out = malloc(sizeof(float) * cap);
    size_t i = start;
    while (i + 16 <= vb_size) {
        int shift = vb[i] & 15, filt = vb[i] >> 4, flags = vb[i + 1];
        if (filt > 4) filt = 4;
        if (flags & 4) loop_start = n;
        if (n + 28 > cap) {
            cap *= 2;
            out = realloc(out, sizeof(float) * cap);
        }
        for (int b = 2; b < 16; b++) {
            for (int h = 0; h < 2; h++) {
                int nib = h ? vb[i + b] >> 4 : vb[i + b] & 15;
                int v = ((nib > 7 ? nib - 16 : nib) * 4096) >> shift;
                v += (s1 * FILTERS[filt][0] + s2 * FILTERS[filt][1] + 32) >> 6;
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                out[n++] = (float)(v / 32768.0);
                s2 = s1;
                s1 = v;
            }
        }
        i += 16;
        if (flags & 1) {
            repeat = (flags & 2) != 0;
            break;
        }
    }
    if (n < 8) {
        free(out);
        out = calloc(8, sizeof(float));
        n = 8;
        repeat = 0;
    }
    s->addr = start;
    s->pcm = out;
    s->n = n;
    s->loop = repeat ? loop_start : -1;
}

/* 4 point Catmull-Rom as render_seq.cubic */
static double cubic_at(const Sample *s, double pos) {
    long i = (long)floor(pos);
    double f = pos - (double)i, y[4];
    for (int k = 0; k < 4; k++) {
        long idx = i - 1 + k;
        if (s->loop < 0) {
            y[k] = (idx < 0 || idx >= s->n) ? 0.0 : s->pcm[idx];
        } else {
            long span = s->n - s->loop;
            if (idx >= s->n) idx = s->loop + (idx - s->loop) % span;
            if (idx < 0) idx = 0;
            if (idx > s->n - 1) idx = s->n - 1;
            y[k] = s->pcm[idx];
        }
    }
    return y[1] + 0.5 * f * (y[2] - y[0] + f * (2 * y[0] - 5 * y[1] + 4 * y[2] - y[3] + f * (3 * (y[1] - y[2]) + y[3] - y[0])));
}

/* Music decoder is per voice: ADPCM predictor history survives repeat jumps.
 * Legacy cached decoding remains available to the separately validated SFX path. */
typedef struct {
    uint32_t address, repeat;
    int s1, s2, block[28], next, recent[4];
    int64_t decoded;
} MusicDecoder;

static int music_next(MusicDecoder *d, const uint8_t *vb, size_t size) {
    if (d->next == 28) {
        if ((size_t)d->address + 16 > size) return 0;
        const uint8_t *b = vb + d->address;
        int filter = b[0] >> 4, shift = b[0] & 15;
        if (filter > 4) filter = 4;
        if (b[1] & 4) d->repeat = d->address;
        for (int i = 0; i < 28; i++) {
            int nib = (b[2 + i / 2] >> ((i & 1) * 4)) & 15;
            int value = ((nib > 7 ? nib - 16 : nib) * 4096) >> shift;
            value += (d->s1 * FILTERS[filter][0] + d->s2 * FILTERS[filter][1] + 32) >> 6;
            if (value > 32767) value = 32767;
            if (value < -32768) value = -32768;
            d->block[i] = value;
            d->s2 = d->s1; d->s1 = value;
        }
        d->address = b[1] & 1 ? (b[1] & 2 ? d->repeat : (uint32_t)size) : d->address + 16;
        d->next = 0;
    }
    return d->block[d->next++];
}

static double music_at(MusicDecoder *d, const uint8_t *vb, size_t size, double pos) {
    int64_t sample = (int64_t)pos;
    while (d->decoded < sample) {
        d->recent[0] = d->recent[1]; d->recent[1] = d->recent[2]; d->recent[2] = d->recent[3];
        d->recent[3] = music_next(d, vb, size);
        d->decoded++;
    }
    unsigned fraction = ((unsigned)((pos - sample) * 4096.0) >> 4) & 255;
    int out = (ae_spu_gauss[255 - fraction] * d->recent[0]) >> 15;
    out += (ae_spu_gauss[511 - fraction] * d->recent[1]) >> 15;
    out += (ae_spu_gauss[256 + fraction] * d->recent[2]) >> 15;
    out += (ae_spu_gauss[fraction] * d->recent[3]) >> 15;
    if (out > 32767) out = 32767;
    if (out < -32768) out = -32768;
    return out / 32768.0;
}

/* ------------------------------------------------------------------ envelope (render_seq.envelope, per sample) */
enum { ENV_ATTACK_A, ENV_ATTACK_B, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE, ENV_DONE };

typedef struct {
    int phase;
    int counter_enabled;
    SpAdsr counter;
    long k;          /* sample counter inside the phase */
    double level;    /* 0..0x7FFF */
    double base;     /* level at the start of the phase */
    double slope;    /* attack slope */
    int a_exp;
    long n1, n2;
    double d_f, sus;
    int s_dec, s_exp;
    double s_rate;   /* sustain inc / cyc */
    int r_exp, r_shift;
    long r_n;
} Env;

static void rate(int shift, int step_field, int increase, double *inc, double *cyc) {
    int step = increase ? 7 - step_field : -8 + step_field;
    int sh = 11 - shift;
    *inc = (double)(sh > 0 ? step * (1 << sh) : step);
    *cyc = (double)(shift - 11 > 0 ? 1 << (shift - 11) : 1);
}

static void env_start(Env *e, uint16_t adsr1, uint16_t adsr2) {
    double inc, cyc;
    memset(e, 0, sizeof *e);
    e->a_exp = (adsr1 & 0x8000) != 0;
    rate((adsr1 >> 10) & 31, (adsr1 >> 8) & 3, 1, &inc, &cyc);
    e->slope = inc / cyc;
    e->n1 = (long)ceil((e->a_exp ? 0x6000 : 0x7FFF) / e->slope);
    e->n2 = e->a_exp ? (long)ceil((0x7FFF - 0x6000) / (e->slope / 4)) : 0;
    e->d_f = 1.0 - pow(2.0, -(double)(((adsr1 >> 4) & 15) + 1));
    e->sus = (double)(((adsr1 & 15) + 1) * 0x800);
    e->s_exp = (adsr2 & 0x8000) != 0;
    e->s_dec = (adsr2 & 0x4000) != 0;
    rate((adsr2 >> 8) & 31, (adsr2 >> 6) & 3, !e->s_dec, &inc, &cyc);
    e->s_rate = inc / cyc;
    e->r_exp = (adsr2 & 0x20) != 0;
    e->r_shift = adsr2 & 31;
    e->phase = ENV_ATTACK_A;
}

/* Counter envelopes keep the legacy phase visible to allocator/lifetime code. */
static void env_counter_sync(Env *e) {
    static const int phases[]={ENV_ATTACK_A,ENV_DECAY,ENV_SUSTAIN,ENV_RELEASE,ENV_DONE};
    e->phase=phases[e->counter.phase];e->level=e->counter.level;
}
static void env_start_counter(Env *e,uint16_t a,uint16_t b) {
    env_start(e,a,b);e->counter_enabled=1;sp_adsr_start(&e->counter,a,b);
}
static double env_counter_step(Env *e) {
    sp_adsr_step(&e->counter);env_counter_sync(e);return e->level;
}

/* enter decay / sustain after the attack (render_seq: level = 0x7FFF, decay only while level > sustain level) */
static void env_after_attack(Env *e) {
    e->level = 0x7FFF;
    e->base = 0x7FFF;
    e->k = 0;
    e->phase = ENV_SUSTAIN;
    if (e->d_f > 0 && e->level > e->sus) {
        e->n2 = e->d_f < 1 ? (long)ceil(log(e->sus / e->level) / log(e->d_f)) : 0; /* render_seq decay length */
        if (e->n2 > 0) e->phase = ENV_DECAY;
    }
}

/* advance one key-on sample; returns the level of this sample */
static double env_step_on(Env *e) {
    if(e->counter_enabled)return env_counter_step(e);
    for (;;) {
        switch (e->phase) {
        case ENV_ATTACK_A:
            if (e->k < e->n1) {
                double v = ++e->k * e->slope, cap = e->a_exp ? 0x6000 : 0x7FFF;
                e->level = v < cap ? v : cap;
                return e->level;
            }
            if (e->a_exp) {
                e->phase = ENV_ATTACK_B;
                e->k = 0;
            } else {
                env_after_attack(e);
            }
            continue;
        case ENV_ATTACK_B:
            if (e->k < e->n2) {
                double v = 0x6000 + ++e->k * (e->slope / 4);
                e->level = v < 0x7FFF ? v : 0x7FFF;
                return e->level;
            }
            env_after_attack(e);
            continue;
        case ENV_DECAY: {
            double v = e->base * pow(e->d_f, (double)++e->k);
            e->level = v;
            if (e->k >= e->n2) { /* last decay sample */
                e->phase = ENV_SUSTAIN;
                e->base = v;
                e->k = 0;
            }
            return v;
        }
        case ENV_SUSTAIN: {
            double v;
            e->k++;
            if (!e->s_dec) {
                v = e->base + e->k * e->s_rate;
                if (v > 0x7FFF) v = 0x7FFF;
            } else if (e->s_exp) {
                double f = e->s_rate != 0 ? 1.0 + e->s_rate / 32768.0 : 1.0;
                v = e->base * pow(f, (double)e->k);
            } else {
                v = e->base + e->k * e->s_rate;
                if (v < 0) v = 0;
            }
            e->level = v;
            return v;
        }
        default:
            return e->level;
        }
    }
}

static void env_key_off(Env *e) {
    if(e->counter_enabled){sp_adsr_release(&e->counter);env_counter_sync(e);return;}
    double lev = e->level;
    e->base = lev;
    e->k = 0;
    if (lev <= 1) {
        e->phase = ENV_DONE;
        e->level = 0;
        return;
    }
    e->phase = ENV_RELEASE;
    if (e->r_exp) {
        double f = 1.0 - pow(2.0, -(double)(e->r_shift + 1));
        long n = (f > 0 && f < 1) ? (long)ceil(log(1.0 / lev) / log(f)) : RELEASE_CAP;
        e->r_n = n < RELEASE_CAP ? n : RELEASE_CAP;
        e->d_f = f;
    } else {
        double inc, cyc;
        rate(e->r_shift, 0, 0, &inc, &cyc);
        double slope = -inc / cyc;
        long n = slope > 0 ? (long)ceil(lev / slope) : RELEASE_CAP;
        e->r_n = n < RELEASE_CAP ? n : RELEASE_CAP;
        e->s_rate = slope;
    }
    if (e->r_n < 1) e->r_n = 1;
}

static double env_step_release(Env *e) {
    if(e->counter_enabled)return env_counter_step(e);
    if (e->phase != ENV_RELEASE || e->k >= e->r_n) {
        e->phase = ENV_DONE;
        e->level = 0;
        return 0;
    }
    e->k++;
    if (e->r_exp) {
        e->level = e->base * pow(e->d_f, (double)e->k);
    } else {
        double v = e->base - e->k * e->s_rate;
        e->level = v > 0 ? v : 0;
    }
    return e->level;
}

/* ------------------------------------------------------------------ player state */
typedef struct {
    const uint8_t *hdr;
    const uint8_t *layers;
    int n;
} Program;

typedef struct {
    uint8_t st, d0, d1, d2;
    uint32_t delta;
} Event;

typedef struct {
    int active;     /* slot holds a voice (render_seq allocate_voices slot) */
    int sounding;   /* still mixed */
    int64_t on, end, steal_t, keyoff_t;
    int released;
    int ch, note, vel;
    const uint8_t *hdr, *L;
    Sample *smp;
    double pos;
    MusicDecoder decoder;
    Env env;
    int lfo_ph;
    long lfo_frame;
    int64_t oneshot_end;
    int age;
} Voice;

typedef struct Note {
    int64_t on;
    int ch, note, vel;
    int voice_ids[8]; /* indexes into the voice array, -1 = none */
    int nv;
    int held;   /* waiting for the sustain pedal */
    int off;    /* key off done */
    int started;
    struct Note *next;
} Note;

struct SpPlayer {
    SpMix *mix;
    const uint8_t *bank, *shd, *ssq, *vb;
    size_t vb_size;
    int flags, loops_left;
    Program progs[256];
    int nprogs;
    const uint8_t *curve, *lfo_base;
    uint8_t pan_tab[0x400], law[0x82];
    uint16_t pitch_tab[608]; /* original SeqPitchFromNote table, including fine/bend guard entries */
    /* events */
    Event *ev;
    int nev, ev_pos, loop_s, loop_e;
    double t_sec, bpm, res;
    int64_t next_ev_t; /* sample time of ev[ev_pos] */
    int32_t delta_remainder;
    uint8_t delta_round_up;
    int ended;
    int64_t total, now;
    /* channels */
    int ch_vol[MAXCH], ch_pan[MAXCH], ch_bend[MAXCH], ch_prog[MAXCH], ch_mod[MAXCH], sustain[MAXCH];
    uint8_t ch_speed[MAXCH];
    /* notes waiting for key off (FIFO per channel / key) */
    Note *active_notes; /* singly linked, in note-on order */
    /* voices */
    Voice *voices;
    int nvoices, cap_voices;
    int slot[NVOICE];
    int ages[NVOICE], cursor, count;
    /* samples */
    Sample **samples;
    int nsamples;
    int *live; /* ids of the sounding voices, in start order */
    int nlive;
    /* reverb */
    int rev_on;
    float rv[33];
    int rv_size;
    double *rbuf;
    long rstep;
    double wet_prev[2][2]; /* wet samples 2k-2, 2k-1 */
    double rout_prev[2], rout_cur[2];
    double rev_gain;
    AeReverbPipeline *integer_reverb; /* SP_INTEGER_REVERB: owned here, run by sp_render or by SpMix */
    double last_wet[2];               /* music wet send of the last rendered frame */
    int stats_voices, stats_stolen, stats_dropped, stats_silent;
};

static Sample *get_sample(SpPlayer *p, uint32_t addr) {
    for (int i = 0; i < p->nsamples; i++)
        if (p->samples[i]->addr == addr) return p->samples[i];
    p->samples = realloc(p->samples, sizeof(Sample *) * (p->nsamples + 1));
    p->samples[p->nsamples] = malloc(sizeof(Sample));
    decode_sample(p->vb, p->vb_size, addr, p->samples[p->nsamples]);
    return p->samples[p->nsamples++];
}

static const uint8_t *exe_ptr(const uint8_t *exe, size_t size, uint32_t addr, size_t n) {
    size_t o = addr - EXE_BASE + EXE_HDR;
    return o + n <= size ? exe + o : NULL;
}

static int parse_events(SpPlayer *p, const uint8_t *q, size_t qsize) {
    static const int DATA[16] = {0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 0, 1, 0};
    size_t pos = 0x110;
    int cap = 1024;
    p->ev = malloc(sizeof(Event) * cap);
    p->nev = 0;
    p->loop_s = p->loop_e = -1;
    while (pos < qsize) {
        Event e = {0};
        int n;
        e.st = q[pos];
        if (e.st < 0x80) return -1;
        if ((e.st >> 4) == 0xF) {
            if (q[pos + 1] == 0x2F) {
                e.d0 = 0x2F;
                if (p->nev == cap) p->ev = realloc(p->ev, sizeof(Event) * (cap *= 2));
                p->ev[p->nev++] = e;
                return 0;
            }
            if (q[pos + 1] != 0x51) return -1;
            n = 3;
        } else {
            n = DATA[e.st >> 4];
        }
        if (n > 0) e.d0 = q[pos + 1];
        if (n > 1) e.d1 = q[pos + 2];
        if (n > 2) e.d2 = q[pos + 3];
        pos += 1 + n;
        for (int i = 0; i < 4; i++) {
            uint8_t b = q[pos++];
            e.delta = e.delta << 7 | (b & 0x7F);
            if (!(b & 0x80)) break;
        }
        if ((e.st >> 4) == 0xB && e.d0 == 0x63) {
            if (e.d1 == 20 && p->loop_s < 0) p->loop_s = p->nev;
            else if (e.d1 == 30) p->loop_e = p->nev;
        }
        if (p->nev == cap) p->ev = realloc(p->ev, sizeof(Event) * (cap *= 2));
        p->ev[p->nev++] = e;
    }
    return -1;
}

static int64_t sec_to_sample(double t) { return (int64_t)nearbyint(t * SP_RATE); }
uint32_t sp_delta_frames(uint32_t delta,uint16_t tempo,uint16_t ppq,uint16_t hz,int32_t *remainder,uint8_t *round_up) {
    if(!tempo)return delta; /* original tempo-zero branch leaves the decoded delta untouched */
    if(!hz)return 0; /* reject invalid driver setup without division by zero */
    int32_t rate=(int32_t)((uint32_t)tempo*ppq*10)/(hz*60);
    if(!rate)return 0;
    int32_t carry=*remainder-(*round_up?rate:0);
    int32_t scaled=(int32_t)(delta*10u+(uint32_t)carry);
    int32_t rem=scaled%rate;
    *round_up=rate<rem*2;
    int32_t ticks=scaled-rem+(*round_up?rate:0);
    *remainder=rem;
    return (uint32_t)ticks/(uint32_t)rate;
}

SpPlayer *sp_create(const uint8_t *bank, size_t bank_size, const uint8_t *exe, size_t exe_size, int loops, int flags,
                    int reverb_preset, int reverb_volume) {
    if (bank_size < 0x40 || memcmp(bank + 0x34, "SShd", 4)) return NULL;
    uint32_t w[13];
    for (int i = 0; i < 13; i++) w[i] = rd32(bank + i * 4);
    if (w[7] + 16 > bank_size || memcmp(bank + w[7] + 12, "SSsq", 4)) return NULL;
    SpPlayer *p = calloc(1, sizeof *p);
    p->bank = bank;
    p->shd = bank + w[5];
    p->ssq = bank + w[7];
    p->vb = bank + w[8];
    p->vb_size = w[3];
    p->flags = flags;
    const uint8_t *h = p->shd;
    /* programs (music tone table at SShd +0x10) */
    uint32_t base = rd32(h + 0x10);
    if (base != 0xFFFFFFFFu) {
        int last = rd16(h + base);
        for (int i = 0; i <= last && i < 256; i++) {
            uint16_t off = rd16(h + base + 2 + i * 2);
            if (off == 0xFFFF) continue;
            const uint8_t *r = h + base + off;
            p->progs[i].hdr = r;
            p->progs[i].layers = r + 8;
            p->progs[i].n = r[0] == 0xFF ? r[7] - r[6] + 1 : (r[0] & 0x7F) + 1;
        }
    }
    p->curve = h + rd32(h + 0x14);
    p->lfo_base = h + rd32(h + 0x18);
    const uint8_t *pt = exe_ptr(exe, exe_size, 0x800B38D0, sizeof p->pitch_tab);
    if (!pt) { sp_destroy(p); return NULL; }
    for (int i = 0; i < 608; i++) p->pitch_tab[i] = rd16(pt + i * 2);
    memcpy(p->pan_tab, exe_ptr(exe, exe_size, 0x800B3D90, 0x400), 0x400);
    memcpy(p->law, exe_ptr(exe, exe_size, 0x800B4190, 0x82), 0x82);
    if (parse_events(p, p->ssq, w[2]) < 0) {
        sp_destroy(p);
        return NULL;
    }
    p->res = rd16(p->ssq + 2);
    p->bpm = rd16(p->ssq + 4);
    for (int c = 0; c < MAXCH; c++) {
        p->ch_vol[c] = 100;
        p->ch_pan[c] = 0x40;
        p->ch_bend[c] = 0x40;
        p->ch_prog[c] = p->ssq[0x10 + c * 16 + 2];
        p->ch_speed[c] = p->ssq[0x10 + c * 16 + 0xC];
    }
    /* loops: 1 = straight through, N = loop section N times, 0 = stop after the loop end, -1 = forever */
    p->loops_left = (p->loop_s >= 0 && p->loop_e >= 0) ? (loops < 0 ? -1 : loops - 1) : 0;
    if (loops == 0 && p->loop_s >= 0 && p->loop_e >= 0) p->loops_left = -2; /* stop at the loop end */
    for (int i = 0; i < NVOICE; i++) p->slot[i] = -1;
    p->next_ev_t = 0;
    p->total = -1;
    /* reverb */
    p->rev_on = !(flags & SP_NO_REVERB) && reverb_preset >= 1 && reverb_preset <= 9;
    if (p->rev_on && (flags & SP_INTEGER_REVERB)) {
        /* integer pipeline: preset[0] = work area base, preset[1..32] = network registers, 33 raw halfwords */
        const uint8_t *r = exe_ptr(exe, exe_size, 0x800B41D0 + (reverb_preset - 1) * 0x42, 0x42);
        uint16_t preset[33];
        int16_t gain = (int16_t)(reverb_volume * 256);
        if (!r) { sp_destroy(p); return NULL; }
        for (int i = 0; i < 33; i++) preset[i] = rd16(r + i * 2);
        p->integer_reverb = malloc(sizeof *p->integer_reverb);
        if (!p->integer_reverb || ae_reverb_pipeline_init(p->integer_reverb, preset, gain, gain) < 0) {
            sp_destroy(p);
            return NULL;
        }
    } else if (p->rev_on) {
        const uint8_t *r = exe_ptr(exe, exe_size, 0x800B41D0 + (reverb_preset - 1) * 0x42, 0x42);
        p->rv_size = (0x80000 - rd16(r) * 8) / 2;
        for (int i = 0; i < 32; i++) p->rv[i] = (float)rd16(r + 2 + i * 2);
        p->rbuf = calloc(p->rv_size, sizeof(double));
        p->rev_gain = (reverb_volume << 8) / 32768.0;
    }
    return p;
}

void sp_destroy(SpPlayer *p) {
    if (!p) return;
    for (int i = 0; i < p->nsamples; i++) {
        free(p->samples[i]->pcm);
        free(p->samples[i]);
    }
    free(p->samples);
    free(p->live);
    free(p->ev);
    free(p->voices);
    free(p->rbuf);
    free(p->integer_reverb);
    while (p->active_notes) {
        Note *n = p->active_notes;
        p->active_notes = n->next;
        free(n);
    }
    free(p);
}

void sp_stats(const SpPlayer *p, int *voices, int *stolen, int *dropped, int *silent) {
    if (voices) *voices = p->stats_voices;
    if (stolen) *stolen = p->stats_stolen;
    if (dropped) *dropped = p->stats_dropped;
    if (silent) *silent = p->stats_silent;
}

/* ------------------------------------------------------------------ voice allocation (render_seq.allocate_voices) */
static void release_slot(SpPlayer *p, int k) {
    for (int i = 0; i < NVOICE; i++)
        if (p->slot[i] >= 0 && p->ages[i] > p->ages[k]) p->ages[i]--;
    p->slot[k] = -1;
    if (p->count > 0) p->count--;
}

/* would the voice stop at sample t (its first sample not yet mixed)? the same tests as the mixer below */
static int voice_ends_at(const SpPlayer *p, const Voice *v, int64_t t) {
    if (!v->sounding || t >= v->end || t >= v->oneshot_end) return 1;
    Env e = v->env;
    double lev;
    int released = v->released;
    if (v->keyoff_t >= 0 && t >= v->keyoff_t && !released) {
        released = 1;
        env_key_off(&e);
    }
    lev = released ? env_step_release(&e) : env_step_on(&e);
    if (released && e.phase == ENV_DONE) return 1;
    return (released || e.phase >= ENV_DECAY) && lev < 0x10;
    (void)p;
}

static int mix_music_alloc(SpMix *,int64_t);
static void mix_music_install(SpMix *,int,int);
static int alloc_slot(SpPlayer *p, int64_t t) {
    if(p->mix)return mix_music_alloc(p->mix,t);
    /* free the voices that ended (render_seq frees pending voices with end <= t) */
    for (;;) {
        int best = -1;
        for (int i = 0; i < NVOICE; i++) {
            if (p->slot[i] < 0) continue;
            Voice *v = &p->voices[p->slot[i]];
            if (v->end > t && voice_ends_at(p, v, t)) v->end = t;
            if (v->end <= t && (best < 0 || v->end < p->voices[p->slot[best]].end)) best = i;
        }
        if (best < 0) break;
        release_slot(p, best);
    }
    for (int k = 0; k < NVOICE; k++) {
        p->cursor = (p->cursor + 1) % NVOICE;
        if (p->slot[p->cursor] < 0) return p->cursor;
    }
    int pick = -1;
    for (int i = 0; i < NVOICE; i++) {
        Voice *v = &p->voices[p->slot[i]];
        if (v->steal_t <= t && (pick < 0 || p->ages[i] < p->ages[pick])) pick = i;
    }
    if (pick < 0)
        for (int i = 0; i < NVOICE; i++)
            if (p->ages[i] < 24) {
                pick = i;
                break;
            }
    if (pick < 0) return -1;
    Voice *old = &p->voices[p->slot[pick]];
    if (old->end > t) old->end = t;
    old->sounding = 0;
    p->stats_stolen++;
    release_slot(p, pick);
    p->cursor = pick;
    return pick;
}

static double lfo_wave(SpPlayer *p, int index, int i) {
    uint16_t off = rd16(p->lfo_base + 2 + index * 2);
    return p->lfo_base[off + i];
}

/* SeqPitchFromNote (0x8009E594): table indexing and octave shifts are integer.
 * Keeping pow() here changes the beating of deliberately detuned paired tones. */
static unsigned music_pitch(SpPlayer *p, int root, int note, int fine, int bend, int range) {
    int delta = note - root, dist = abs(delta);
    int fraction = (range * (bend - 64)) >> 2;
    int index = 208 + fine + fraction + (delta < 0 ? 12 - dist % 12 : dist % 12) * 16;
    if (index < 0 || index >= 608) return 0; /* malformed tone/controller data */
    unsigned pitch = p->pitch_tab[index];
    if (delta < 0) pitch >>= (dist / 12 + 1) & 31;
    else pitch <<= (dist / 12) & 31;
    pitch &= 0xFFFF;
    return pitch > 0x4000 ? 0x4000 : pitch;
}

/* SeqUpdateVoices normal-music mode preserves channel bend as note/fine
 * offsets, then applies modulation with a ONE-semitone range, not tone range. */
static unsigned music_mod_pitch(SpPlayer *p, int root, int note, int fine, int bend, int range, int wave, int depth) {
    int offset = range * (bend - 64);
    int semitones = offset / 64; /* original truncates negative magnitudes toward zero */
    int remainder = offset / 4 - semitones * 16;
    int mod = wave * depth / 255 - ((depth + 1) >> 1) + 64;
    return music_pitch(p, (120 + root - note) & 255, (120 + semitones) & 255,
                       (int8_t)(fine + remainder), mod, 1);
}

static unsigned voice_pitch(SpPlayer *p, Voice *v, int64_t s) {
    const uint8_t *L = v->L;
    int rng = (L[15] & 0x10) ? v->hdr[4] : L[13];
    int bend = p->ch_bend[v->ch];
    if ((L[15] & 0x20) && !(p->flags & SP_NO_MOD)) {
        int depth = p->ch_mod[v->ch];
        long frame = (long)((s - v->on) / FRAME_SAMPLES);
        while (v->lfo_frame < frame) {
            v->lfo_frame++;
            v->lfo_ph += p->ch_speed[v->ch];
            if (v->lfo_ph > 0xEF) v->lfo_ph = p->ch_speed[v->ch] >> 1;
        }
        if (depth) {
            int idx = v->lfo_ph >> 2;
            if (idx > 63) idx = 63;
            int curve = (L[15] & 0x40) ? v->hdr[5] : L[14];
            return music_mod_pitch(p, L[2], v->note, (int8_t)L[3], bend, L[13],
                                   (int)lfo_wave(p, curve, idx), depth);
        }
    }
    return music_pitch(p, L[2], v->note, (int8_t)L[3], bend, rng);
}

static void start_voice(SpPlayer *p, Note *nt, const uint8_t *hdr, const uint8_t *L, int64_t t) {
    int k = -1;
    if (!(p->flags & SP_NO_VOICE_LIMIT)) {
        k = alloc_slot(p, t);
        if (k < 0) {
            p->stats_dropped++;
            return;
        }
    }
    if (p->nvoices == p->cap_voices) {
        p->cap_voices = p->cap_voices ? p->cap_voices * 2 : 256;
        p->voices = realloc(p->voices, sizeof(Voice) * p->cap_voices);
    }
    int id = p->nvoices++;
    Voice *v = &p->voices[id];
    memset(v, 0, sizeof *v);
    v->active = 1;
    v->sounding = 1;
    v->on = t;
    v->end = INT64_MAX;
    v->keyoff_t = -1;
    v->steal_t = (L[15] & 1) ? INT64_MAX : t;
    v->ch = nt->ch;
    v->note = nt->note;
    v->vel = nt->vel;
    v->hdr = hdr;
    v->L = L;
    v->smp = get_sample(p, rd16(L + 4) * 8u);
    v->decoder.address = v->decoder.repeat = v->smp->addr;
    v->decoder.next = 28; v->decoder.decoded = -1;
    env_start(&v->env, rd16(L + 6), rd16(L + 8));
    if(p->flags&SP_COUNTER_ADSR)env_start_counter(&v->env,rd16(L+6),rd16(L+8));
    /* All seven sustain rate bits set disable the SPU envelope counter. */
    if ((rd16(L + 8) & 0x1FC0) == 0x1FC0) v->env.s_rate = 0;
    v->oneshot_end = INT64_MAX;
    if (v->smp->loop < 0) { /* render_seq: the voice ends with a one shot sample (pitch at key on) */
        int rng = (L[15] & 0x10) ? hdr[4] : L[13];
        double st = music_pitch(p, L[2], v->note, (int8_t)L[3], p->ch_bend[v->ch], rng) / 4096.0;
        if (st == 0) st = 1.0 / 4096.0;
        v->oneshot_end = t + (int64_t)(v->smp->n / st) + 1;
    }
#ifdef SP_TRACE
    if (k >= 0) fprintf(stderr, "alloc %lld %d ch %d note %d L %02x%02x%c", (long long)t, k, nt->ch, nt->note, L[0], L[1], 10);
#endif
    if (k >= 0) {
        p->slot[k] = id;
        p->ages[k] = p->count;
        if (p->count < 24) p->count++;
        if(p->mix)mix_music_install(p->mix,k,id);
    }
    if (nt->nv < 8) nt->voice_ids[nt->nv++] = id;
    p->live = realloc(p->live, sizeof(int) * (p->nlive + 1));
    p->live[p->nlive++] = id;
    p->stats_voices++;
}

static void note_start(SpPlayer *p, Note *nt) {
    Program *pr = &p->progs[p->ch_prog[nt->ch] & 0xFF];
    nt->started = 1;
    if (!pr->hdr) {
        p->stats_silent++;
        return;
    }
    const uint8_t *hit[128];
    int nh = 0;
    static const uint8_t EMPTY[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    if (pr->hdr[0] == 0xFF) {
        int i = nt->note - pr->hdr[6];
        if (i >= 0 && i < pr->n) hit[nh++] = pr->layers + i * 16;
    } else {
        for (int i = 0; i < pr->n && nh < 128; i++) {
            const uint8_t *L = pr->layers + i * 16;
            if (L[0] <= nt->note && nt->note <= L[1]) hit[nh++] = L;
        }
    }
    int m = 0;
    for (int i = 0; i < nh; i++)
        if (memcmp(hit[i], EMPTY, 16)) hit[m++] = hit[i];
    nh = m;
    if (!nh) {
        p->stats_silent++;
        return;
    }
    if (pr->hdr[0] != 0xFF && !(pr->hdr[0] & 0x80)) nh = 1;
    for (int i = 0; i < nh; i++) start_voice(p, nt, pr->hdr, hit[i], nt->on);
}

static void note_off(SpPlayer *p, Note *nt, int64_t t) {
    nt->off = 1;
    int64_t koff = t > nt->on ? t : nt->on + 1; /* n_on >= 1 */
    for (int i = 0; i < nt->nv; i++) {
        Voice *v = &p->voices[nt->voice_ids[i]];
        v->keyoff_t = koff;
        if (v->L[15] & 1 && v->steal_t > koff) v->steal_t = koff;
    }
}

/* key offs requested at this time; at the song end every hanging note is released */
static void apply_key_offs(SpPlayer *p, int only_started) {
    for (Note **pp = &p->active_notes; *pp;) {
        Note *nt = *pp;
        if ((nt->started || !only_started) && (nt->off == 2 || (p->ended && !nt->off))) {
            note_off(p, nt, p->ended && nt->off != 2 ? p->total : p->now);
            *pp = nt->next;
            free(nt);
        } else {
            pp = &nt->next;
        }
    }
}

/* execute every event at the current sample; note-ons pick their program after all events of this time */
static void run_events(SpPlayer *p) {
    Note *started[512];
    int ns = 0;
    while (!p->ended && p->next_ev_t <= p->now) {
        Event *e = &p->ev[p->ev_pos];
        int64_t ts = p->next_ev_t;
        int c = e->st & 15, kind = e->st >> 4;
        if (kind == 9) {
            Note *nt = calloc(1, sizeof *nt);
            nt->on = ts;
            nt->ch = c;
            nt->note = e->d0;
            nt->vel = e->d1;
            Note **pp = &p->active_notes;
            while (*pp) pp = &(*pp)->next;
            *pp = nt;
            if (ns < 512) started[ns++] = nt;
        } else if (kind == 8) {
            for (Note *nt = p->active_notes; nt; nt = nt->next) {
                if (nt->ch == c && nt->note == e->d0 && !nt->off && !nt->held) {
                    if (p->sustain[c]) nt->held = 1;
                    else nt->off = 2; /* key off at ts (applied below, after the voices exist) */
                    break;
                }
            }
        } else if (kind == 0xB) {
            if (e->d0 == 7) p->ch_vol[c] = 100 * e->d1 >> 7;
            else if (e->d0 == 1) p->ch_mod[c] = e->d1;
            else if (e->d0 == 10) p->ch_pan[c] = e->d1;
            else if (e->d0 == 0x40) {
                int on = e->d1 >= 64;
                if (p->sustain[c] && !on)
                    for (Note *nt = p->active_notes; nt; nt = nt->next)
                        if (nt->ch == c && nt->held) {
                            nt->held = 0;
                            nt->off = 2;
                        }
                p->sustain[c] = on;
            }
        } else if (kind == 0xC) {
            p->ch_prog[c] = e->d0;
        } else if (kind == 0xE) {
            p->ch_bend[c] = e->d0;
        } else if (e->st == 0xFF && e->d0 == 0x51) {
            p->bpm = (double)(e->d1 | e->d2 << 8);
        }
        /* advance */
        int is_end = e->st == 0xFF && e->d0 == 0x2F;
        p->t_sec += e->delta * (60.0 / (p->bpm * p->res));
        int next = p->ev_pos + 1;
        if (p->ev_pos == p->loop_e) {
            if (p->loops_left == -2) is_end = 1;
            else if (p->loops_left != 0) {
                if (p->loops_left > 0) p->loops_left--;
                next = p->loop_s;
            }
        }
        if (is_end || next >= p->nev) {
            p->ended = 1;
            p->total = p->flags&SP_FRAME_TIMING?ts:sec_to_sample(p->t_sec);
            break;
        }
        p->ev_pos = next;
        if(p->flags&SP_FRAME_TIMING)
            p->next_ev_t=ts+(int64_t)sp_delta_frames(e->delta,(uint16_t)p->bpm,(uint16_t)p->res,60,&p->delta_remainder,&p->delta_round_up)*(SP_RATE/60);
        else p->next_ev_t = sec_to_sample(p->t_sec);
    }
    /* key offs of this time first (render_seq knows every key off in advance, so a voice released at this tick is
     * already stealable for the notes starting now), then the new notes, then the key offs of zero length notes */
    apply_key_offs(p, 1);
    for (int i = 0; i < ns; i++) note_start(p, started[i]);
    apply_key_offs(p, 0);
}

/* ------------------------------------------------------------------ SPU reverb (one 22.05 kHz step) */
enum { dAPF1, dAPF2, vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL, vAPF1, vAPF2, mLSAME, mRSAME, mLCOMB1, mRCOMB1,
       mLCOMB2, mRCOMB2, dLSAME, dRSAME, mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4, dLDIFF, dRDIFF, mLAPF1,
       mRAPF1, mLAPF2, mRAPF2, vLIN, vRIN };

static double rv_vol(const SpPlayer *p, int r) {
    int v = (int)p->rv[r];
    return (v & 0x8000 ? v - 0x10000 : v) / 32768.0;
}

static long rv_off(const SpPlayer *p, int r) { return (long)p->rv[r] * 4; }

static double *rv_cell(SpPlayer *p, long off) {
    long i = (p->rstep + off) % p->rv_size;
    if (i < 0) i += p->rv_size;
    return &p->rbuf[i];
}

static double clip1(double v) { return v > 1 ? 1 : v < -1 ? -1 : v; }

/* Integer reverb send: floor(v * 32768) clamped to signed16. The double is clamped before the integer conversion. */
static int16_t wet_to_s16(double v) {
    double f = floor(v * 32768.0);
    if (!(f > -32768.0)) return -32768; /* also catches NaN */
    if (f > 32767.0) return 32767;
    return (int16_t)f;
}

static void reverb_step(SpPlayer *p, double inl, double inr, double out[2]) {
    double lin = inl * rv_vol(p, vLIN), rin = inr * rv_vol(p, vRIN);
    double iir = rv_vol(p, vIIR), wall = rv_vol(p, vWALL);
    const int dst[4] = {mLSAME, mRSAME, mLDIFF, mRDIFF}, tap[4] = {dLSAME, dRSAME, dRDIFF, dLDIFF};
    for (int i = 0; i < 4; i++) {
        double in = (i & 1) ? rin : lin;
        double prev = *rv_cell(p, rv_off(p, dst[i]) - 1);
        *rv_cell(p, rv_off(p, dst[i])) = clip1((in + *rv_cell(p, rv_off(p, tap[i])) * wall - prev) * iir + prev);
    }
    const int comb[2][4] = {{mLCOMB1, mLCOMB2, mLCOMB3, mLCOMB4}, {mRCOMB1, mRCOMB2, mRCOMB3, mRCOMB4}};
    const int apf[2][2] = {{mLAPF1, mLAPF2}, {mRAPF1, mRAPF2}};
    for (int s = 0; s < 2; s++) {
        double a = 0;
        for (int i = 0; i < 4; i++) a += rv_vol(p, vCOMB1 + i) * *rv_cell(p, rv_off(p, comb[s][i]));
        for (int j = 0; j < 2; j++) {
            double vap = rv_vol(p, j ? vAPF2 : vAPF1);
            double old = *rv_cell(p, rv_off(p, apf[s][j]) - rv_off(p, j ? dAPF2 : dAPF1));
            a -= vap * old;
            *rv_cell(p, rv_off(p, apf[s][j])) = clip1(a);
            a = a * vap + old;
        }
        out[s] = a * p->rev_gain;
    }
    p->rstep = (p->rstep + 1) % p->rv_size;
}

/* ------------------------------------------------------------------ mixing */
int sp_render(SpPlayer *p, float *out, int frames) {
    int busy = 0;
    const int nolimit = (p->flags & SP_NO_VOICE_LIMIT) != 0;
    for (int f = 0; f < frames; f++, p->now++) {
        run_events(p);
        double dl = 0, dr = 0, wl = 0, wr = 0;
        int any = !p->ended;
        int nl = 0;
        for (int li = 0; li < p->nlive; li++) {
            Voice *v = &p->voices[p->live[li]];
            if (!v->sounding) continue;
            p->live[nl++] = p->live[li];
            int64_t s = p->now;
            if (s >= v->end) {
                v->sounding = 0;
                continue;
            }
            if (v->keyoff_t >= 0 && s >= v->keyoff_t && !v->released) {
                v->released = 1;
                env_key_off(&v->env);
            }
            double lev = v->released ? env_step_release(&v->env) : env_step_on(&v->env);
            if (v->released && v->env.phase == ENV_DONE) {
                v->sounding = 0;
                if (v->end > s) v->end = s;
                continue;
            }
            if (!nolimit) {
                /* render_seq: the voice ends after the last sample whose level is >= 0x10 */
                int post_attack = v->released || v->env.phase >= ENV_DECAY;
                if (post_attack && lev < 0x10) {
                    v->sounding = 0;
                    if (v->end > s) v->end = s;
                    continue;
                }
                if (s >= v->oneshot_end) {
                    v->sounding = 0;
                    if (v->end > s) v->end = s;
                    continue;
                }
            }
            any = 1;
            double step = voice_pitch(p, v, s) / 4096.0;
            double sig = 0;
            if (!(v->smp->loop < 0 && v->pos >= v->smp->n)) sig = music_at(&v->decoder, p->vb, p->vb_size, v->pos) * (lev / (v->env.counter_enabled?32768.0:32767.0));
            v->pos += step;
            const uint8_t *L = v->L, *hdr = v->hdr;
            int p1 = p->pan_tab[(L[12] >> 2) + (hdr[2] >> 2) * 32];
            int p2 = p->pan_tab[(p1 >> 2) + (p->ch_pan[v->ch] >> 2) * 32];
            int idx = (p2 >> 1) & 0x7E;
            long base = ((long)p->ch_vol[v->ch] * hdr[1] * p->curve[v->vel + 2] * L[11]) >> 14;
            double gl = (double)(base * p->law[idx] >> 7) / 16384.0, gr = (double)(base * p->law[idx + 1] >> 7) / 16384.0;
            dl += sig * gl;
            dr += sig * gr;
            if (L[15] & 0x80) {
                wl += sig * gl;
                wr += sig * gr;
            }
        }
        p->nlive = nl;
        p->last_wet[0] = wl;
        p->last_wet[1] = wr;
        double rl = 0, rr = 0;
        if (p->integer_reverb) {
            /* attached to an SpMix the mixer runs the pipeline once for music + effects: dry only here */
            if (!p->mix) {
                int16_t in[2] = {wet_to_s16(wl), wet_to_s16(wr)};
                int32_t ret[2];
                ae_reverb_pipeline_step(p->integer_reverb, in, ret);
                rl = ret[0] / 32768.0;
                rr = ret[1] / 32768.0;
                if (fabs(rl) + fabs(rr) > 1e-7) any = 1;
            }
        } else if (p->rev_on) {
            if ((p->now & 1) == 0) {
                double o[2];
                p->rout_prev[0] = p->rout_cur[0];
                p->rout_prev[1] = p->rout_cur[1];
                reverb_step(p, 0.5 * (p->wet_prev[0][0] + p->wet_prev[1][0]), 0.5 * (p->wet_prev[0][1] + p->wet_prev[1][1]), o);
                p->rout_cur[0] = o[0];
                p->rout_cur[1] = o[1];
                rl = 0.5 * (p->rout_prev[0] + o[0]);
                rr = 0.5 * (p->rout_prev[1] + o[1]);
            } else {
                rl = p->rout_cur[0];
                rr = p->rout_cur[1];
            }
            p->wet_prev[0][0] = p->wet_prev[1][0];
            p->wet_prev[0][1] = p->wet_prev[1][1];
            p->wet_prev[1][0] = wl;
            p->wet_prev[1][1] = wr;
            if (fabs(rl) + fabs(rr) > 1e-7) any = 1;
        }
        out[2 * f] = (float)(dl + rl);
        out[2 * f + 1] = (float)(dr + rr);
        if (any) busy = f + 1;
    }
    return busy;
}

/* ================================================================== sound effects (tools/render_sfx.py) */
#define SFX_SLOTS 4
#define SFX_TICK (1.0 / 480)
#define SFX_FRAME (1.0 / 60)
#define SFX_RAMPS 8

typedef struct {
    double t0, dur, target;
} Ramp;

typedef struct {
    int handle, prog, note, vel;
    int uid,pool_slot,discarded;
    double on_sec, off_sec; /* event times inside the effect (seconds); off_sec < 0 = no key off */
    int64_t on, keyoff;
    int released;
    const uint8_t *hdr, *L, *curve;
    Sample *smp;
    double pos;
    Env env;
    Ramp vol[SFX_RAMPS], pan[SFX_RAMPS], pitch[SFX_RAMPS];
    int nvol, npan, npitch;
} SfxVoice;

typedef struct {
    const uint8_t *shd, *vb;
    size_t shd_size, vb_size;
    Sample **samples;
    int nsamples;
} SfxBank;

struct SpSfx {
    SpMix *mix;
    const uint8_t *exe;
    size_t exe_size;
    uint8_t pan_tab[0x400], law[0x82];
    SfxBank bank[SFX_SLOTS];
    SfxVoice *v;
    int nv, cap;
    int64_t now;
    int next_handle;
    int next_uid;
    double auto_off;
    int frame_timing;
    int counter_adsr;
    double last_wet[2]; /* reverb send (layer flag L[15] & 0x80) of the last rendered frame */
};

SpSfx *sp_sfx_create(const uint8_t *exe, size_t exe_size) {
    const uint8_t *pt = exe_ptr(exe, exe_size, 0x800B3D90, 0x400), *lw = exe_ptr(exe, exe_size, 0x800B4190, 0x82);
    if (!pt || !lw) return NULL;
    SpSfx *s = calloc(1, sizeof *s);
    s->exe = exe;
    s->exe_size = exe_size;
    memcpy(s->pan_tab, pt, 0x400);
    memcpy(s->law, lw, 0x82);
    s->auto_off = -1;
    return s;
}

void sp_sfx_destroy(SpSfx *s) {
    if (!s) return;
    for (int b = 0; b < SFX_SLOTS; b++) {
        for (int i = 0; i < s->bank[b].nsamples; i++) {
            free(s->bank[b].samples[i]->pcm);
            free(s->bank[b].samples[i]);
        }
        free(s->bank[b].samples);
    }
    free(s->v);
    free(s);
}

int sp_sfx_bank(SpSfx *s, int slot, const uint8_t *shd, size_t shd_size, const uint8_t *vb, size_t vb_size) {
    if (slot < 0 || slot >= SFX_SLOTS || shd_size < 0x24 || memcmp(shd + 12, "SShd", 4)) return -1;
    if (rd32(shd + 0x1C) == 0xFFFFFFFFu || rd32(shd + 0x20) == 0xFFFFFFFFu) return -1;
    SfxBank *b = &s->bank[slot];
    b->shd = shd;
    b->shd_size = shd_size;
    b->vb = vb;
    b->vb_size = vb_size;
    return 0;
}

void sp_sfx_auto_off(SpSfx *s, double seconds) { s->auto_off = seconds; }
void sp_sfx_frame_timing(SpSfx *s,int enabled){s->frame_timing=enabled!=0;}
void sp_sfx_counter_adsr(SpSfx *s,int enabled){s->counter_adsr=enabled!=0;}

static Sample *sfx_sample(SfxBank *b, uint32_t addr) {
    for (int i = 0; i < b->nsamples; i++)
        if (b->samples[i]->addr == addr) return b->samples[i];
    b->samples = realloc(b->samples, sizeof(Sample *) * (b->nsamples + 1));
    b->samples[b->nsamples] = malloc(sizeof(Sample));
    decode_sample(b->vb, b->vb_size, addr, b->samples[b->nsamples]);
    return b->samples[b->nsamples++];
}

/* event length including the status byte for the extended controllers (notes/24) */
static int sfx_ctl_len(int ctl) {
    switch (ctl) {
    case 0x01: case 0x02: case 0x60: return 5;
    case 0x07: case 0x0A: case 0x41: return 6;
    case 0x06: case 0x40: case 0x62: case 0x63: return 3;
    default: return 0;
    }
}

static const uint8_t EMPTY_LAYER[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

/* the voices of an effect are created from its whole (tiny) sequence at once, like render_sfx.render */
int sp_sfx_play(SpSfx *s, int slot, int prog, int note) {
    if (slot < 0 || slot >= SFX_SLOTS || !s->bank[slot].shd || prog < 0 || note < 0) return -1;
    SfxBank *b = &s->bank[slot];
    const uint8_t *h = b->shd;
    uint32_t base = rd32(h + 0x1C);
    if (prog > rd16(h + base)) return -1;
    uint16_t o = rd16(h + base + 2 + prog * 2);
    if (o == 0xFFFF) return -1;
    uint32_t sub = base + (o & 0xFFFE);
    if (note > rd16(h + sub)) return -1;
    uint32_t pos = base + rd16(h + sub + 2 + note * 2);
    uint32_t tb = rd32(h + 0x20) + 0x190;
    const uint8_t *curve = h + rd32(h + 0x14);
    int handle = s->next_handle++, first = s->nv;
    double t = 0;
    int32_t delta_remainder=0;uint8_t delta_round_up=0;int64_t ticks=0;
    int64_t origin=s->frame_timing?((s->now+SP_RATE/60-1)/(SP_RATE/60))*(SP_RATE/60):s->now;
    for (int guard = 0; guard < 4096 && pos + 2 <= b->shd_size; guard++) {
        uint8_t st = h[pos];
        int n;
        if (st == 0xFF && h[pos + 1] == 0x2F) break;
        if (st == 0xA0) n = 3;
        else if (st == 0xB0 && sfx_ctl_len(h[pos + 1])) n = sfx_ctl_len(h[pos + 1]) - 1;
        else break;
        const uint8_t *d = h + pos + 1;
        pos += 1 + n;
        uint32_t delta = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t c = h[pos++];
            delta = delta << 7 | (c & 0x7F);
            if (!(c & 0x80)) break;
        }
        if (st == 0xA0) {
            int key = d[0], vel = d[1], pg = d[2];
            if (vel == 0) { /* key off of the voices playing (program, note) */
                for (int i = first; i < s->nv; i++)
                    if (s->v[i].prog == pg && s->v[i].note == key && s->v[i].off_sec < 0) s->v[i].off_sec = t;
            } else if (pg <= rd16(h + tb) && rd16(h + tb + 2 + pg * 2) != 0xFFFF) {
                const uint8_t *r = h + tb + rd16(h + tb + 2 + pg * 2);
                int cnt = r[0] == 0xFF ? r[7] - r[6] + 1 : (r[0] & 0x7F) + 1, idx = key - r[6];
                if (idx >= 0 && idx < cnt && memcmp(r + 8 + idx * 16, EMPTY_LAYER, 16)) {
                    if (s->nv == s->cap) s->v = realloc(s->v, sizeof(SfxVoice) * (s->cap = s->cap ? s->cap * 2 : 64));
                    SfxVoice *v = &s->v[s->nv++];
                    memset(v, 0, sizeof *v);
                    v->uid=s->next_uid++;v->pool_slot=-1;
                    v->handle = handle;
                    v->prog = pg;
                    v->note = key;
                    v->vel = vel;
                    v->hdr = r;
                    v->L = r + 8 + idx * 16;
                    v->curve = curve;
                    v->smp = sfx_sample(b, rd16(v->L + 4) * 8u);
                    v->on_sec = t;
                    v->off_sec = -1;
                    v->on = origin + (s->frame_timing?ticks*(SP_RATE/60):(int64_t)(t * SP_RATE));
                    env_start(&v->env, rd16(v->L + 6), rd16(v->L + 8));
                    if(s->counter_adsr)env_start_counter(&v->env,rd16(v->L+6),rd16(v->L+8));
                }
            }
        } else if (d[0] == 0x07 || d[0] == 0x0A || d[0] == 0x41) {
            int tm = d[1], val = d[2], pg = d[3], key = d[4];
            for (int i = first; i < s->nv; i++) {
                SfxVoice *v = &s->v[i];
                if (v->prog != pg || v->note != key || v->off_sec >= 0) continue;
                double t0 = t - v->on_sec, dur = tm * 4 * SFX_FRAME;
                if (d[0] == 0x07 && v->nvol < SFX_RAMPS) v->vol[v->nvol++] = (Ramp){t0, dur, val};
                else if (d[0] == 0x0A && v->npan < SFX_RAMPS) v->pan[v->npan++] = (Ramp){t0, dur, val};
                else if (d[0] == 0x41 && v->npitch < SFX_RAMPS) {
                    double acc = (v->npitch ? v->pitch[v->npitch - 1].target : 0) + (val < 0x80 ? val / 10.0 : -(256 - val) / 10.0);
                    v->pitch[v->npitch++] = (Ramp){t0, dur, acc};
                }
            }
        }
        if(s->frame_timing){ticks+=sp_delta_frames((uint32_t)delta,60,480,60,&delta_remainder,&delta_round_up);t=(double)ticks/60;}
        else t += delta * SFX_TICK;
    }
    /* key off times as render_sfx: n_on = int((off - on) * RATE); without a key off auto_off applies (testing) */
    for (int i = first; i < s->nv; i++) {
        SfxVoice *v = &s->v[i];
        int64_t n_on = -1;
        if (v->off_sec >= 0) n_on = s->frame_timing?(int64_t)nearbyint((v->off_sec-v->on_sec)*SP_RATE):(int64_t)((v->off_sec - v->on_sec) * SP_RATE);
        else if (s->auto_off >= 0)
            n_on = v->smp->loop >= 0 ? (int64_t)(((v->on_sec + s->auto_off) - v->on_sec) * SP_RATE) : (int64_t)(s->auto_off * SP_RATE);
        if (n_on >= 0 && n_on < 1) n_on = 1;
        v->keyoff = n_on >= 0 ? v->on + n_on : -1;
    }
    return handle;
}

int sp_sfx_play_id(SpSfx *s, int id) {
    const uint8_t *r = exe_ptr(s->exe, s->exe_size, 0x800B2D08 + 8 * (uint32_t)(id < 0 ? 0 : id), 8);
    if (id < 0 || id >= 198 || !r) return -1;
    return sp_sfx_play(s, (int16_t)rd16(r), (int16_t)rd16(r + 2), (int16_t)rd16(r + 4));
}

void sp_sfx_stop(SpSfx *s, int handle) {
    for (int i = 0; i < s->nv; i++) {
        SfxVoice *v = &s->v[i];
        if (v->handle == handle && (v->keyoff < 0 || v->keyoff > s->now)) v->keyoff = s->now > v->on ? s->now : v->on + 1;
    }
}

static double ramp_eval(const Ramp *r, int n, double start, double tt) {
    double v = start, cur = start;
    for (int i = 0; i < n; i++) {
        if (tt >= r[i].t0) {
            if (r[i].dur <= 0) {
                v = r[i].target;
            } else {
                double k = (tt - r[i].t0) / r[i].dur;
                k = k < 0 ? 0 : k > 1 ? 1 : k;
                v = cur + (r[i].target - cur) * k;
            }
        }
        cur = r[i].target;
    }
    return v;
}

int sp_sfx_render(SpSfx *s, float *out, int frames) {
    int busy = 0;
    for (int f = 0; f < frames; f++, s->now++) {
        double l = 0, r = 0, wl = 0, wr = 0;
        int nl = 0;
        for (int i = 0; i < s->nv; i++) {
            SfxVoice v = s->v[i];
            if(v.discarded)continue;
            if (s->now < v.on) { /* later voice of a running effect */
                s->v[nl++] = v;
                busy = f + 1;
                continue;
            }
            if (!v.released && v.keyoff >= 0 && s->now >= v.keyoff) {
                v.released = 1;
                env_key_off(&v.env);
            }
            double lev = v.released ? env_step_release(&v.env) : env_step_on(&v.env);
            if (v.released && v.env.phase == ENV_DONE) continue; /* the voice has finished */
            const uint8_t *L = v.L, *hdr = v.hdr;
            double tt = (double)(s->now - v.on) / SP_RATE;
            double key = v.note + (v.npitch ? ramp_eval(v.pitch, v.npitch, 0.0, tt) : 0.0);
            key = key < 12 ? 12 : key > 243 ? 243 : key;
            double step = pow(2.0, (key - L[2] + (int8_t)L[3] / 16.0) / 12.0);
            if (step > 4.0) step = 4.0;
            double sig = 0;
            if (!(v.smp->loop < 0 && v.pos >= v.smp->n)) sig = cubic_at(v.smp, v.pos) * (lev / (v.env.counter_enabled?32768.0:32767.0));
            v.pos += step;
            int vi = (int)(v.nvol ? ramp_eval(v.vol, v.nvol, v.vel, tt) : v.vel);
            long panv = v.npan ? (long)ramp_eval(v.pan, v.npan, 0x40, tt) : 0x40;
            panv = panv < 0 ? 0 : panv > 127 ? 127 : panv;
            int velc = v.curve[(vi < 127 ? vi : 127) + 2];
            int p1 = s->pan_tab[(L[12] >> 2) + (hdr[2] >> 2) * 32];
            int p2 = s->pan_tab[(p1 >> 2) + (panv >> 2) * 32];
            int idx = (p2 >> 1) & 0x7E;
            long gbase = ((long)127 * hdr[1] * velc * L[11]) >> 14;
            l += sig * (double)(gbase * s->law[idx] >> 7) / 16384.0;
            r += sig * (double)(gbase * s->law[idx + 1] >> 7) / 16384.0;
            if (L[15] & 0x80) { /* reverb send: same dry expressions, accumulated separately */
                wl += sig * (double)(gbase * s->law[idx] >> 7) / 16384.0;
                wr += sig * (double)(gbase * s->law[idx + 1] >> 7) / 16384.0;
            }
            busy = f + 1;
            s->v[nl++] = v;
        }
        s->nv = nl;
        s->last_wet[0] = wl;
        s->last_wet[1] = wr;
        out[2 * f] = (float)l;
        out[2 * f + 1] = (float)r;
    }
    return busy;
}

struct SpMix {
    SpPlayer *music;SpSfx *effects;SpVoicePool pool;
    int owner[24],peak,dropped;
};
static SfxVoice *mix_effect_voice(SpMix *m,int uid){
    for(int i=0;i<m->effects->nv;i++)if(m->effects->v[i].uid==uid&&!m->effects->v[i].discarded)return &m->effects->v[i];
    return NULL;
}
static void mix_evict(SpMix *m,int slot){
    SpPoolVoice *v=&m->pool.voice[slot];if(!v->active)return;
    if(v->effect){SfxVoice *e=mix_effect_voice(m,m->owner[slot]);if(e)e->discarded=1;}
    else{
        Voice *old=&m->music->voices[m->owner[slot]];old->sounding=old->active=0;old->end=m->music->now;
        m->music->slot[slot]=-1;++m->music->stats_stolen;
    }
    v->active=0;
}
static void mix_free_slot(SpMix *m,int slot){
    SpPoolVoice *v=&m->pool.voice[slot];unsigned age=v->age;
    if(v->effect&&m->pool.effects)--m->pool.effects;
    if(m->pool.count)--m->pool.count;
    for(int i=0;i<24;i++)if(m->pool.voice[i].age!=65535&&m->pool.voice[i].age>age)--m->pool.voice[i].age;
    if(!v->effect)m->music->slot[slot]=-1;
    memset(v,0,sizeof(*v));v->age=65535;
}
static int mix_music_alloc(SpMix *m,int64_t time){
    (void)time;int slot=sp_pool_music(&m->pool);if(slot==255){++m->dropped;return -1;}
    mix_evict(m,slot);return slot;
}
static void mix_count(SpMix *m){if(m->pool.count<24)++m->pool.count;if(m->pool.count>m->peak)m->peak=m->pool.count;}
static void mix_music_install(SpMix *m,int slot,int id){
    Voice *v=&m->music->voices[id];m->pool.voice[slot]=(SpPoolVoice){1,(v->L[15]&1)?0:1,m->pool.count,0,0,0};
    m->owner[slot]=id;mix_count(m);
}
SpMix *sp_mix_create(SpPlayer *p,SpSfx *s){
    if(!p||!s||p->mix||s->mix||p->now||s->now||p->nvoices||s->nv||p->flags&SP_NO_VOICE_LIMIT)return NULL;
    SpMix *m=calloc(1,sizeof(*m));if(!m)return NULL;
    m->music=p;m->effects=s;m->pool.effect_limit=8;
    for(int i=0;i<24;i++)m->pool.voice[i].age=65535;
    p->mix=s->mix=m;return m;
}
void sp_mix_destroy(SpMix *m){if(m){m->music->mix=NULL;m->effects->mix=NULL;free(m);}}
static void mix_prepare(SpMix *m){
    SpPlayer *p=m->music;SpSfx *s=m->effects;
    if(p->now%(SP_RATE/60)==0)for(int i=0;i<24;i++){
        SpPoolVoice *v=&m->pool.voice[i];if(!v->active)continue;
        if(v->effect){
            SfxVoice *e=mix_effect_voice(m,m->owner[i]);
            if(!e||(e->env.phase==ENV_DONE)||(v->freeable&&e->env.level<16&&e->released)){
                if(e)e->discarded=1;
                mix_free_slot(m,i);
            }else if(e->keyoff>=0&&s->now>=e->keyoff)v->freeable=1;
        }else{
            Voice *e=&p->voices[m->owner[i]];
            if(voice_ends_at(p,e,p->now)){e->sounding=e->active=0;mix_free_slot(m,i);}
            else if(e->steal_t<=p->now)v->freeable=1;
        }
    }
    /* Original tick handles music track before effect tracks, then commits SPU. */
    run_events(p);
    for(int i=0;i<s->nv;i++){
        SfxVoice *e=&s->v[i];if(e->discarded||e->pool_slot>=0||s->now<e->on)continue;
        int slot=sp_pool_effect(&m->pool,e->L[0],e->L[1]);
        if(slot==255){e->discarded=1;++m->dropped;continue;}
        mix_evict(m,slot);
        m->pool.voice[slot]=(SpPoolVoice){1,(e->L[15]&1)?0:1,m->pool.count,1,e->L[1],e->L[0]};
        m->owner[slot]=e->uid;e->pool_slot=slot;mix_count(m);
    }
}
int sp_mix_render(SpMix *m,float *out,int frames){
    int busy=0;
    for(int i=0;i<frames;i++){
        mix_prepare(m);float music[2],effects[2];
        if(sp_render(m->music,music,1)|sp_sfx_render(m->effects,effects,1))busy=i+1;
        out[i*2]=music[0]+effects[0];out[i*2+1]=music[1]+effects[1];
        if(m->music->integer_reverb){
            /* one pipeline frame per output frame, fed by the summed music + effect sends */
            int16_t in[2]={wet_to_s16(m->music->last_wet[0]+m->effects->last_wet[0]),
                           wet_to_s16(m->music->last_wet[1]+m->effects->last_wet[1])};
            int32_t ret[2];
            double rl,rr;
            ae_reverb_pipeline_step(m->music->integer_reverb,in,ret);
            rl=ret[0]/32768.0;rr=ret[1]/32768.0;
            out[i*2]=(float)((double)out[i*2]+rl);out[i*2+1]=(float)((double)out[i*2+1]+rr);
            if(fabs(rl)+fabs(rr)>1e-7)busy=i+1;
        }
    }
    return busy;
}
void sp_mix_stats(const SpMix *m,int *active,int *effects,int *peak,int *dropped){
    if(active)*active=m->pool.count;
    if(effects)*effects=m->pool.effects;
    if(peak)*peak=m->peak;
    if(dropped)*dropped=m->dropped;
}
