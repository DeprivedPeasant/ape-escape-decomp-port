# Native viewer input startup

## `--keyboard-only`

Explicit, opt-in controller-free launch for `native_viewer.exe` and
`build_native_viewer.py`.

- Initialises SDL with `SDL_INIT_VIDEO` only (no `SDL_INIT_GAMEPAD`).
- Skips `SDL_GetGamepads` enumeration and the open loop entirely.
- Gamepad added/removed/button event handlers never open a device while the
  option is set (`gamepad_open` returns immediately).
- Window, renderer, keyboard, mouse and game simulation are unchanged.

Motivation: on some hosts, adding `SDL_INIT_GAMEPAD` intermittently stalls
startup (observed with the DirectInput joystick backend), while
`SDL_INIT_VIDEO` alone starts normally. This option lets a run avoid the
gamepad subsystem without touching the environment.

### Defaults and scope

- Without the flag, startup and input behavior are exactly as before
  (gamepad subsystem initialised, devices opened). Gamepad support is never
  silently disabled.
- Hidden-run defaults (`--check`, `--capture`, `--frames`) are unchanged.
- No OS, user, or SDL hint/environment settings are modified.
- This is a viewer launch option only; it does not claim original gameplay
  input behavior.

### Build CLI

`python tools/codex/build_native_viewer.py ... --keyboard-only` appends the flag
to the shared presentation arguments, so it is forwarded to the `--check` run,
`--capture` and `--run` invocations.


Codex verification: strict C11 default/player builds,540 exact prior camera/proximity
frame matches with the option (no DirectInput hint) and equal default screenshots passed.
Legacy follow540 and indicator101-area regressions passed using only the documented
per-child [DirectInput hint](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_DIRECTINPUT).
The underlying device/backend cause is not established. Physical gamepad input was not
validated. See validation/keyboard_startup_validation.md.
