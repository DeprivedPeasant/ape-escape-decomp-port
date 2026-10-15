/* Monkey Net swing on the real player data: gadget sets load, a full gadget-stick push starts the swing from
 * standing (0 -> 0x80 -> 0x81 held), releasing it ends the swing (0x83 -> 0).
 * Usage: test_gadget <member 008.bin> <SCUS_944.23>   (skips with exit 0 when the files are missing) */
#include "native_player.h"
#include "native_player_pad.h"
#include "native_gadget_slots.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned char *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); long len; unsigned char *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); len = ftell(f); rewind(f);
    b = malloc((size_t)len);
    if (b && fread(b, 1, (size_t)len, f) != (size_t)len) { free(b); b = NULL; }
    fclose(f); *n = (size_t)len; return b;
}

/* one frame on flat ground at y = 0: raw pad packet (gadget stick = right stick bytes 4 / 5) -> ported PadUpdate -> player,
   then FrameEndUpdate's stick sample (processed by the next PadUpdate, the original's one-frame stick latency) */
static void frame(AePlayer *p, const AePlayerData *d, AePlayerPad *pad, int8_t gx, int8_t gy, uint32_t held) {
    AePlayerInput in; AePlayerGround gr; AePlayerPadRaw raw;
    memset(&in, 0, sizeof(in)); memset(&gr, 0, sizeof(gr));
    raw.held = (uint16_t)held; raw.rx = (uint8_t)(0x80 + gx); raw.ry = (uint8_t)(0x80 + gy); raw.lx = raw.ly = 0x80; raw.digital = 0;
    CHECK(ae_player_pad_update(pad, &raw, p->facing, &in), "pad update: %s", ae_ram_error(pad->ram));
    gr.ground_y = 0; gr.surface_flags = 0;
    ae_player_update(p, d, &in, &gr);
    int32_t pos[3] = { p->pos[0], 0, p->pos[2] }, vel[3] = { 0, 0, 0 };
    ae_player_apply_motion(p, pos, vel, 0);
    ae_player_pad_frame_end(pad);
}

int main(int argc, char **argv) {
    size_t mn = 0, en = 0;
    unsigned char *member = argc > 2 ? slurp(argv[1], &mn) : NULL, *exe = argc > 2 ? slurp(argv[2], &en) : NULL;
    if (!member || !exe) { printf("gadget: skipped (member 008 / EXE not found)\n"); return 0; }
    AePlayerData d; ae_player_data_init(&d);
    CHECK(ae_player_data_load_member(&d, member, mn), "load member: %s", ae_player_error());
    CHECK(ae_player_data_load_exe(&d, exe, en), "load exe: %s", ae_player_error());
    /* decoded counts (state_tables.json): net = 4 states / 14 transitions, club = 14 / 33, radar and RC car none */
    CHECK(d.gadget_sets[1].state_count == 4 && d.gadget_sets[1].transition_count == 14, "net set %u / %u", d.gadget_sets[1].state_count, d.gadget_sets[1].transition_count);
    CHECK(d.gadget_sets[0].state_count == 14 && d.gadget_sets[0].transition_count == 33, "club set %u / %u", d.gadget_sets[0].state_count, d.gadget_sets[0].transition_count);
    CHECK(d.gadget_sets[2].state_count == 0, "radar has no states");

    AePlayer p; int32_t pos[3] = { 0, 0, 0 }; int16_t facing[3] = { 0, 0, 0x1000 };
    ae_player_init(&p, pos, facing);
    p.gadget_slots[3] = 1;                       /* net on Cross */
    AePlayerPad pad;
    CHECK(ae_player_pad_init(&pad, exe, en), "pad init");
    for (int i = 0; i < 5; ++i) frame(&p, &d, &pad, 0, 0, 0);
    CHECK(p.state == 0, "standing before the swing (state %#x)", p.state);
    frame(&p, &d, &pad, 0, 0, 0x40);       /* Cross: equip the net */
    frame(&p, &d, &pad, 0, 0, 0);
    CHECK(p.current_gadget == 1, "net equipped (gadget %d)", p.current_gadget);
    int saw80 = 0, saw81 = 0, saw83 = 0, back0 = 0;
    for (int i = 0; i < 40; ++i) {               /* push the gadget stick fully to the right and hold it */
        frame(&p, &d, &pad, 127, 0, 0);
        saw80 |= p.state == 0x80; saw81 |= p.state == 0x81;
    }
    CHECK(saw80 && saw81, "swing start 0x80 then hold 0x81 (saw %d %d, state %#x)", saw80, saw81, p.state);
    CHECK(p.desired_dir[0] > 0xF00, "FaceStick turns toward the stick (desired x %d)", p.desired_dir[0]);
    for (int i = 0; i < 40; ++i) {               /* release */
        frame(&p, &d, &pad, 0, 0, 0);
        saw83 |= p.state == 0x83; back0 |= p.state == 0 && saw83;
    }
    CHECK(saw83 && back0, "release ends the swing 0x83 -> 0 (saw %d %d, state %#x)", saw83, back0, p.state);
    printf("gadget: %d checks, %d failures\n", checks, failures);
    ae_player_pad_free(&pad); ae_player_data_free(&d); free(member); free(exe);
    return failures != 0;
}
