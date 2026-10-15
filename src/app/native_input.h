/* Controller-independent input state in the PlayStation digital/analog pad convention.
 * Pure C, no SDL: the viewer fills the sample structs from SDL and merges keyboard and gamepad here.
 * Bits are the PsyQ pad word as the game uses it in g_PadHeld (active high). Sticks are 0..255 with 0x80 centre,
 * y down = larger. Mapping table: notes/port/controls.md. */
#ifndef AE_NATIVE_INPUT_H
#define AE_NATIVE_INPUT_H
#include <stdint.h>

enum {
    AE_PAD_L2 = 0x0001, AE_PAD_R2 = 0x0002, AE_PAD_L1 = 0x0004, AE_PAD_R1 = 0x0008,
    AE_PAD_TRIANGLE = 0x0010, AE_PAD_CIRCLE = 0x0020, AE_PAD_CROSS = 0x0040, AE_PAD_SQUARE = 0x0080,
    AE_PAD_SELECT = 0x0100, AE_PAD_L3 = 0x0200, AE_PAD_R3 = 0x0400, AE_PAD_START = 0x0800,
    AE_PAD_UP = 0x1000, AE_PAD_RIGHT = 0x2000, AE_PAD_DOWN = 0x4000, AE_PAD_LEFT = 0x8000
};
#define AE_INPUT_DEAD_ZONE 0.18f
#define AE_INPUT_KEY_TILT 127   /* keyboard stick deflection */
#define AE_INPUT_KEY_HALF 64    /* Shift: half tilt (walk) */
#define AE_INPUT_KEY_CAMERA 64  /* arrow keys: camera stick deflection */

typedef struct {
    uint16_t held, changed, pressed; /* changed = held ^ previous held, pressed = held & changed */
    uint8_t lx, ly, rx, ry;
} AeInput;

/* SDL standard gamepad, by position (SDL3 names): south = Cross, east = Circle, west = Square, north = Triangle. */
typedef struct {
    int south, east, west, north, left_shoulder, right_shoulder, back, touchpad, start, left_stick, right_stick;
    int dpad_up, dpad_down, dpad_left, dpad_right;
    float lx, ly, rx, ry; /* -1..1, y down positive */
    float lt, rt;         /* 0..1 */
} AeGamepadSample;

typedef struct {
    int w, a, s, d, shift, up, down, left, right, space, lctrl, i, j, k, l, enter, tab, n1, n2;
} AeKeyboardSample;

void ae_input_centered(AeInput *in);                                /* held 0, sticks 0x80 */
void ae_input_from_gamepad(AeInput *out, const AeGamepadSample *g); /* held + sticks; edges zero */
void ae_input_from_keyboard(AeInput *out, const AeKeyboardSample *k);
/* OR the buttons; each stick comes from the gamepad when it is off centre, else from the keyboard. */
void ae_input_merge(AeInput *out, const AeInput *keyboard, const AeInput *gamepad);
/* One 30 Hz tick: take held and sticks from sample, derive changed/pressed against the state's previous held. */
void ae_input_tick(AeInput *state, const AeInput *sample);
/* Stick byte -> -1..1 (0x80 = 0). */
float ae_input_axis(uint8_t v);
/* Stick bytes -> x/y components (right/down positive) and magnitude 0..1. Returns the magnitude. */
float ae_input_stick(uint8_t x, uint8_t y, float *sx, float *sy);
#endif
