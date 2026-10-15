#include "native_input.h"
#include <math.h>

static uint8_t to_byte(float v) {
    long r = 128 + lroundf(v * 127.f);
    return (uint8_t)(r < 1 ? 1 : r > 255 ? 255 : r);
}
/* Radial dead zone with rescaling, as the original viewer did for the SDL sticks. */
static void stick_bytes(float x, float y, uint8_t *ox, uint8_t *oy) {
    float len = sqrtf(x * x + y * y);
    if (!(len > AE_INPUT_DEAD_ZONE)) { *ox = *oy = 0x80; return; }
    float scale = fminf(1.f, (len - AE_INPUT_DEAD_ZONE) / (1.f - AE_INPUT_DEAD_ZONE)) / len;
    *ox = to_byte(x * scale);
    *oy = to_byte(y * scale);
}
void ae_input_centered(AeInput *in) {
    in->held = in->changed = in->pressed = 0;
    in->lx = in->ly = in->rx = in->ry = 0x80;
}
void ae_input_from_gamepad(AeInput *out, const AeGamepadSample *g) {
    uint16_t h = 0;
    ae_input_centered(out);
    if (g->south) h |= AE_PAD_CROSS;
    if (g->east) h |= AE_PAD_CIRCLE;
    if (g->west) h |= AE_PAD_SQUARE;
    if (g->north) h |= AE_PAD_TRIANGLE;
    if (g->left_shoulder) h |= AE_PAD_L1;
    if (g->right_shoulder) h |= AE_PAD_R1;
    if (g->lt > 0.5f) h |= AE_PAD_L2;
    if (g->rt > 0.5f) h |= AE_PAD_R2;
    if (g->back || g->touchpad) h |= AE_PAD_SELECT;
    if (g->start) h |= AE_PAD_START;
    if (g->left_stick) h |= AE_PAD_L3;
    if (g->right_stick) h |= AE_PAD_R3;
    if (g->dpad_up) h |= AE_PAD_UP;
    if (g->dpad_down) h |= AE_PAD_DOWN;
    if (g->dpad_left) h |= AE_PAD_LEFT;
    if (g->dpad_right) h |= AE_PAD_RIGHT;
    out->held = h;
    stick_bytes(g->lx, g->ly, &out->lx, &out->ly);
    stick_bytes(g->rx, g->ry, &out->rx, &out->ry);
}
void ae_input_from_keyboard(AeInput *out, const AeKeyboardSample *k) {
    uint16_t h = 0;
    int tilt = k->shift ? AE_INPUT_KEY_HALF : AE_INPUT_KEY_TILT;
    ae_input_centered(out);
    if (k->space) h |= AE_PAD_R1;
    if (k->lctrl) h |= AE_PAD_R2;
    if (k->i) h |= AE_PAD_TRIANGLE;
    if (k->j) h |= AE_PAD_SQUARE;
    if (k->k) h |= AE_PAD_CROSS;
    if (k->l) h |= AE_PAD_CIRCLE;
    if (k->enter) h |= AE_PAD_START;
    if (k->tab) h |= AE_PAD_SELECT;
    if (k->n1) h |= AE_PAD_L1;
    if (k->n2) h |= AE_PAD_L2;
    out->held = h;
    out->lx = (uint8_t)(0x80 + (k->d ? tilt : 0) - (k->a ? tilt : 0));
    out->ly = (uint8_t)(0x80 + (k->s ? tilt : 0) - (k->w ? tilt : 0));
    out->rx = (uint8_t)(0x80 + (k->right ? AE_INPUT_KEY_CAMERA : 0) - (k->left ? AE_INPUT_KEY_CAMERA : 0));
    out->ry = (uint8_t)(0x80 + (k->down ? AE_INPUT_KEY_CAMERA : 0) - (k->up ? AE_INPUT_KEY_CAMERA : 0));
}
void ae_input_merge(AeInput *out, const AeInput *kb, const AeInput *pad) {
    AeInput r;
    int pl = pad->lx != 0x80 || pad->ly != 0x80, pr = pad->rx != 0x80 || pad->ry != 0x80;
    r.held = (uint16_t)(kb->held | pad->held);
    r.changed = r.pressed = 0;
    r.lx = pl ? pad->lx : kb->lx; r.ly = pl ? pad->ly : kb->ly;
    r.rx = pr ? pad->rx : kb->rx; r.ry = pr ? pad->ry : kb->ry;
    *out = r;
}
void ae_input_tick(AeInput *state, const AeInput *sample) {
    uint16_t prev = state->held;
    *state = *sample;
    state->changed = (uint16_t)(sample->held ^ prev);
    state->pressed = (uint16_t)(sample->held & state->changed);
}
float ae_input_axis(uint8_t v) {
    float f = ((int)v - 128) / 127.f;
    return f < -1.f ? -1.f : f > 1.f ? 1.f : f;
}
float ae_input_stick(uint8_t x, uint8_t y, float *sx, float *sy) {
    float fx = ae_input_axis(x), fy = ae_input_axis(y), m = sqrtf(fx * fx + fy * fy);
    *sx = fx; *sy = fy;
    return m > 1.f ? 1.f : m;
}
