# Native demo controls

The demo (`src/app/native_viewer.c`) builds one pad state per 30 Hz tick in `src/app/native_input.{c,h}` (pure C, tested by
`src/app/test_input.c`): a PSX pad word (`held`, `changed = held ^ previous`, `pressed = held & changed`) plus four 0..255
stick bytes (0x80 centre, y down = larger). Keyboard and gamepad are merged: the buttons are ORed; a gamepad stick wins
over the keyboard when it is off centre. `pressed` goes to the message box. The native player gets the raw pad packet: the `held`
word (as the active-low bytes 2 / 3) and the four stick bytes (4 / 5 = right stick X / Y, 6 / 7 = left stick X / Y), which go
through the instruction-for-instruction port of the original `PadUpdate` (`src/core/ae_pad.c`, diffed against the original code by
`tools/mips/check_player_input.py`): history ring of 40 samples, dead zone and magnitude scaling, push class, the camera-relative
direction from the camera rotation matrix, flick / spin gestures. The sticks therefore reach the player one frame after the
buttons, as in the original (`FrameEndUpdate` samples the sticks at the end of a frame; `PadUpdate` processes them in the next).
Note the original's naming: `g_RStick*` is the movement stick (physical LEFT stick, bytes 6 / 7) and `g_LStick*` the gadget stick
(physical RIGHT stick, bytes 4 / 5).

## Keyboard

| Keys | Pad equivalent | Effect |
|---|---|---|
| W A S D | left stick | move Spike (camera-relative); Shift = half tilt (walk). With no player: fly the free camera (Q / E down / up, Shift = 3x speed) |
| Arrow keys | right stick | swing the Monkey Net while it is equipped (full tilt); otherwise the camera |
| Space | R1 | jump (the real capture shows jump = held 0x0008) |
| Left Ctrl | R2 | |
| I / J / K / L | Triangle / Square / Cross / Circle | I = Stun Club (Triangle), K = Monkey Net (Cross): the original new-game slots; J (Square) and L (Circle) are empty slots; message box: Circle (L) turns the page |
| 1 / 2 | L1 / L2 | |
| Enter | Start | closes a message box; otherwise toggles the debug actor pause |
| Tab | Select | |
| Backspace | (Start, message box only) | |
| H, [ , ] | | preview hint, previous / next hint sector |
| C, T, V, Y | | colliders, textures, camera inspection, stage effect |
| Mouse drag / wheel | | orbit / zoom the free camera |
| Esc | | quit |

## PS4 DualShock 4 (and other SDL standard gamepads)

SDL maps controllers by position, so Xbox / Switch pads work the same way (Xbox A = Cross position, and so on).

| DualShock 4 | PSX bit | Notes |
|---|---|---|
| Cross / Circle / Square / Triangle | 0x40 / 0x20 / 0x80 / 0x10 | Cross = equip the Monkey Net, Triangle = Stun Club (the original new-game slots; Square / Circle are empty; the club's own states are not ported yet) |
| L1 / R1 | 0x04 / 0x08 | R1 = jump |
| L2 / R2 (analog trigger > 50 %) | 0x01 / 0x02 | |
| Share, Touchpad click | 0x100 Select | |
| Options | 0x800 Start | closes a message box; otherwise toggles actor pause |
| L3 / R3 | 0x200 / 0x400 | |
| D-pad Up / Right / Down / Left | 0x1000 / 0x2000 / 0x4000 / 0x8000 | |
| Left stick | left analog | moves Spike (radial dead zone 0.18, rescaled, magnitude kept) |
| Right stick | right analog | swing the Monkey Net (push to swing, hold to keep sweeping, release to finish); camera when no swing gadget is equipped |

Dead zone and the PS-convention bytes are in `native_input.c`.

## Gamepad notes

- The first gamepad found is opened at startup (its name and type are printed to stderr as `GAMEPAD opened: ...`); a controller plugged in
  later is opened when none is open.
- SDL's DirectInput joystick backend is disabled by default (`SDL_HINT_JOYSTICK_DIRECTINPUT=0`) because it can hang startup
  on some PCs (`notes/port/native_input_startup.md`). A DualShock 4 is handled by SDL's HIDAPI driver, Xbox pads by XInput.
  `--directinput` turns the backend back on. `--keyboard-only` never initialises the gamepad subsystem.
- Hidden / scripted runs (`--walk-test`, the checkers) send no input: `held = 0`.
- Not verified on hardware by the author: the physical DualShock 4 path (the unit test covers the mapping only).

## Gadgets

The slots, the unlock mask and the first equipped gadget are the **original new-game defaults**, ported from
`GadgetSlotsInitForExit` (0x8007B16C, run by `GameStateReset` with `g_StageExitCode` 0; `SaveNewGameInit` does not touch the gadget
globals) and diffed against the original instructions: `g_GadgetSlots` = { 0, -1, -1, 1 }, i.e. Triangle = gadget 0 (Stun Club),
Square and Circle empty, Cross = gadget 1 (Monkey Net); `g_GadgetUnlockMask` = 3 (club and net); the game then runs
`PlayerSelectGadget(0, -1)`, so **the Stun Club is equipped at the start** (the older demo started with the net and put the club
on Square: both were choices, not original data). `PlayerCheckGadgetSelect`: Triangle / Square / Circle / Cross (pad bits
0x10 / 0x80 / 0x20 / 0x40) select slots 0..3 (the last pressed test wins; an empty slot does nothing), only when `CanSwitchGadget`
allows it (player mode 0 or >= 4 except 9 and 0xB, and not swimming: states 0x46 0x47 0x49 0x4A 0x4C). Pressing the button of the
equipped gadget again changes nothing. The swing uses the original net state set (0x80 swing, 0x81 sweep while the stick is held,
0x83 end) and the original flick detection (the stick must reach magnitude 0x7D: a full-tilt push, not the arrow keys' half tilt;
the viewer feeds the arrow keys at full tilt while the net is equipped); while the net is in
0x80 / 0x81 a sphere in front of Spike catches monkeys (approximation of the original hit volume, see
notes/port/monkeys.md). While the net is equipped the follow camera gets no right-stick input.
