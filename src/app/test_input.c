/* Unit test for native_input (no SDL, no game data). */
#include "native_input.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
int main(void) {
    AeInput g, k, m, st;
    AeGamepadSample gs;
    AeKeyboardSample ks;
    float x, y;
    memset(&gs, 0, sizeof gs);
    memset(&ks, 0, sizeof ks);
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.held == 0 && g.lx == 0x80 && g.ly == 0x80 && g.rx == 0x80 && g.ry == 0x80);
    gs.south = gs.east = gs.west = gs.north = 1;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.held == (AE_PAD_CROSS | AE_PAD_CIRCLE | AE_PAD_SQUARE | AE_PAD_TRIANGLE));
    memset(&gs, 0, sizeof gs);
    gs.left_shoulder = gs.right_shoulder = gs.start = gs.left_stick = gs.right_stick = 1;
    gs.dpad_up = gs.dpad_right = gs.dpad_down = gs.dpad_left = 1;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.held == (AE_PAD_L1 | AE_PAD_R1 | AE_PAD_START | AE_PAD_L3 | AE_PAD_R3 | AE_PAD_UP | AE_PAD_RIGHT | AE_PAD_DOWN | AE_PAD_LEFT));
    memset(&gs, 0, sizeof gs);
    gs.lt = 0.4f; gs.rt = 0.6f; gs.touchpad = 1;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.held == (AE_PAD_R2 | AE_PAD_SELECT));
    gs.touchpad = 0; gs.back = 1; gs.lt = 1.f; gs.rt = 0;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.held == (AE_PAD_L2 | AE_PAD_SELECT));
    /* sticks: dead zone, full deflection, y down = larger */
    memset(&gs, 0, sizeof gs);
    gs.lx = 0.1f; gs.ly = -0.1f;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.lx == 0x80 && g.ly == 0x80);
    gs.lx = 1.f; gs.ly = 0; gs.rx = 0; gs.ry = 1.f;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.lx == 255 && g.ly == 0x80 && g.rx == 0x80 && g.ry == 255);
    gs.lx = -1.f; gs.ly = -1.f;
    ae_input_from_gamepad(&g, &gs);
    CHECK(g.lx < 0x80 && g.ly < 0x80 && g.lx >= 1);
    /* keyboard */
    ks.w = 1; ks.space = 1; ks.l = 1;
    ae_input_from_keyboard(&k, &ks);
    CHECK(k.held == (AE_PAD_R1 | AE_PAD_CIRCLE) && k.lx == 0x80 && k.ly == 0x80 - AE_INPUT_KEY_TILT);
    ks.shift = 1; ks.d = 1;
    ae_input_from_keyboard(&k, &ks);
    CHECK(k.lx == 0x80 + AE_INPUT_KEY_HALF && k.ly == 0x80 - AE_INPUT_KEY_HALF);
    memset(&ks, 0, sizeof ks);
    ks.a = ks.d = 1; ks.up = 1; ks.right = 1;
    ae_input_from_keyboard(&k, &ks);
    CHECK(k.lx == 0x80 && k.rx == 0x80 + AE_INPUT_KEY_CAMERA && k.ry == 0x80 - AE_INPUT_KEY_CAMERA);
    memset(&ks, 0, sizeof ks);
    ks.lctrl = ks.i = ks.j = ks.k = ks.enter = ks.tab = ks.n1 = ks.n2 = 1;
    ae_input_from_keyboard(&k, &ks);
    CHECK(k.held == (AE_PAD_R2 | AE_PAD_TRIANGLE | AE_PAD_SQUARE | AE_PAD_CROSS | AE_PAD_START | AE_PAD_SELECT | AE_PAD_L1 | AE_PAD_L2));
    /* merge: OR bits, pad stick wins only off centre */
    memset(&ks, 0, sizeof ks); ks.space = 1; ks.w = 1; ks.right = 1;
    ae_input_from_keyboard(&k, &ks);
    memset(&gs, 0, sizeof gs); gs.south = 1;
    ae_input_from_gamepad(&g, &gs);
    ae_input_merge(&m, &k, &g);
    CHECK(m.held == (AE_PAD_R1 | AE_PAD_CROSS) && m.ly == k.ly && m.rx == k.rx);
    gs.lx = 1.f;
    ae_input_from_gamepad(&g, &gs);
    ae_input_merge(&m, &k, &g);
    CHECK(m.lx == 255 && m.ly == 0x80 && m.rx == k.rx);
    /* edges */
    ae_input_centered(&st);
    ae_input_tick(&st, &k);   /* R1 goes down */
    CHECK(st.changed == AE_PAD_R1 && st.pressed == AE_PAD_R1);
    ae_input_tick(&st, &k);   /* held */
    CHECK(st.held == AE_PAD_R1 && st.changed == 0 && st.pressed == 0);
    ae_input_centered(&m);
    ae_input_tick(&st, &m);   /* released */
    CHECK(st.held == 0 && st.changed == AE_PAD_R1 && st.pressed == 0);
    CHECK(ae_input_stick(0x80, 0x80, &x, &y) == 0.f);
    CHECK(ae_input_stick(255, 0x80, &x, &y) == 1.f && x == 1.f);
    if (!fails) printf("native_input: all tests passed\n");
    return fails ? 1 : 0;
}
