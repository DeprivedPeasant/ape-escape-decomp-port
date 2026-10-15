/* Command line front end of sp_player: render a sound bank's song to a 16 bit stereo WAV file.
 *
 *   gcc -std=c11 -O2 sp_player.c sp_render_wav.c -o sp_render_wav -lm
 *   sp_render_wav <bank member .bin> <SCUS_944.23> <out.wav> [loops (default 1, -1 = forever)] [max seconds] [--native]
 *
 * The output is normalised to 0.89 of full scale like tools/render_seq.py.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sp_player.h"

static uint8_t *load(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n);
    if (b && fread(b, 1, n, f) != (size_t)n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return b;
}

static void put32(FILE *f, uint32_t v) {
    uint8_t b[4] = {v & 255, v >> 8 & 255, v >> 16 & 255, v >> 24};
    fwrite(b, 1, 4, f);
}

static void put16(FILE *f, uint16_t v) {
    uint8_t b[2] = {v & 255, v >> 8};
    fwrite(b, 1, 2, f);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s bank.bin SCUS_944.23 out.wav [loops] [max seconds] [--native]\n", argv[0]);
        return 2;
    }
    size_t bs, es;
    uint8_t *bank = load(argv[1], &bs), *exe = load(argv[2], &es);
    if (!bank || !exe) {
        fprintf(stderr, "cannot read the input files\n");
        return 1;
    }
    int loops = argc > 4 ? atoi(argv[4]) : 1;
    double max_s = argc > 5 ? atof(argv[5]) : 600.0;
    int flags = argc > 6 && strcmp(argv[6], "--native") == 0 ? SP_FRAME_TIMING | SP_COUNTER_ADSR | SP_INTEGER_REVERB : 0;
    SpPlayer *p = sp_create(bank, bs, exe, es, loops, flags, 4, 10);
    if (!p) {
        fprintf(stderr, "%s is not a sound bank\n", argv[1]);
        return 1;
    }
    size_t cap = SP_RATE * 2 * 60, n = 0;
    float *pcm = malloc(sizeof(float) * cap);
    int quiet = 0;
    while (quiet < 3 && n / 2 < (size_t)(max_s * SP_RATE)) {
        if (n + 2 * SP_RATE > cap) pcm = realloc(pcm, sizeof(float) * (cap *= 2));
        int busy = sp_render(p, pcm + n, SP_RATE);
        n += 2 * SP_RATE;
        quiet = busy ? 0 : quiet + 1;
    }
    float peak = 1e-9f;
    for (size_t i = 0; i < n; i++) {
        float a = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (a > peak) peak = a;
    }
    /* trim the trailing silence */
    size_t last = n;
    while (last > 2 && pcm[last - 1] * pcm[last - 1] + pcm[last - 2] * pcm[last - 2] < 1e-8f * peak * peak) last -= 2;
    FILE *f = fopen(argv[3], "wb");
    if (!f) return 1;
    uint32_t data = (uint32_t)(last * 2);
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + data);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 2);
    put32(f, SP_RATE);
    put32(f, SP_RATE * 4);
    put16(f, 4);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, data);
    for (size_t i = 0; i < last; i++) {
        float v = pcm[i] * 0.89f / peak;
        int s = (int)(v * 32767.0f);
        put16(f, (uint16_t)(int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s));
    }
    fclose(f);
    int voices, stolen, dropped, silent;
    sp_stats(p, &voices, &stolen, &dropped, &silent);
    printf("%s: %.1f s, %d voices, %d stolen, %d dropped, %d silent notes\n", argv[3], last / 2.0 / SP_RATE, voices, stolen, dropped,
           silent);
    sp_destroy(p);
    free(pcm);
    free(bank);
    free(exe);
    return 0;
}
