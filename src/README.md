# src/: portable C of the native port

Each folder is one subsystem. The modules read the game's own tables from the EXE / archive members at run time
(no game data in the repo). Includes between folders are relative (`#include "../camera/native_camera.h"`), so no
`-I` flags are needed. Python checkers compile the modules they test through `tools/codex/src_paths.py`
(`src('native_camera.c')`); `CMakeLists.txt` at the repo root and `tools/codex/build_native_viewer.py` build the
SDL3 + OpenGL demo from the same file list.

| Folder | What it is | Checkers (run with `uv run python <script>`) | Notes |
|---|---|---|---|
| `app/` | Demo viewer (`native_viewer.c`), audio output, host glue that wires the native player into the viewer, keyboard + gamepad input (`native_input.c`, test `test_input.c`; controls: `notes/port/controls.md`) | `src/app/check_input.py`, `tools/codex/check_overnight_demo.py` (builds the viewer, 540 frames), `check_native_viewer.py`, `check_player_viewer.py`, `check_keyboard_startup.py` | notes/65, `notes/port/overnight_demo.md` |
| `audio/` | SEQ / SPU music and effects player (`sp_*`), voice pool, ADSR, reverb, Gauss table | `tools/native_audio/check_player.py`, `tools/codex/check_voice_pool.py`, `check_adsr_counters.py`, `check_reverb_*.py`, `check_shared_reverb.py`, `check_music_fidelity.py`, `check_spu_reference.py` | notes/30, `notes/port/audio_*`, `notes/port/validation/` |
| `bodies/` | Collision-body system (body list and slots, body-vs-body tests, `CollisionUpdate` minus the player / RC car movement) translated 1:1 from the original code onto an `AeRam` image (`ae_bodies.c`, hooks for everything outside the module) | `tools/mips/check_bodies.py` (differential run against the original MIPS code, about 30 s) | `notes/port/bodies.md` |
| `camera/` | Follow camera, slots, regions, scripts, blends, molten / stage handlers, SDK math | `tools/codex/check_camera_*.py`, `check_native_camera.py` | notes/19, `notes/port/native_camera*.md` |
| `collision/` | Collision engine and terrain ground height | `tools/codex/check_native_collision.py`, `check_native_ground.py`, `check_collision_*.py` | notes/67-69 |
| `core/` | `AeRam`: byte-exact 2 MiB PSX main RAM + 1 KiB scratchpad image with bounds-checked little-endian accessors and a sticky error flag; gameplay modules ported 1:1 run on it with the original addresses and struct layouts | `tools/mips/check_bodies.py` (exercises it) | `notes/port/bodies.md` |
| `effects/` | Particle effect bytecode VM | `tools/codex/check_native_effects.py`, `check_effect_scripts.py` | notes/54, 64 |
| `indicators/` | Stage hint markers: state, triggers, world presentation | `tools/codex/check_indicator_*.py`, `check_native_indicators.py` | `notes/port/native_indicator*.md` |
| `math/` | Original SDK matrix / sqrt / apply routines | `tools/codex/check_sdk_apply.py`, `check_sdk_matrix.py`, `check_sdk_sqrt.py` | `notes/port/native_sdk_*.md` |
| `monkeys/` | The monkey AI and update code (MonkeySpawnAll .. MonkeyUpdateAll), hand-ported on the RAM image (`ae_monkey*.c`) | `tools/mips/check_monkey_base.py`, `check_monkey_level1..10.py` | `notes/port/monkeys-port.md`, notes/05, 15, 17 |
| `player/` | Player state machine, collision body and collider volume queries, synthetic tests, replay harness | `src/player/run_tests.sh` (no game data), `check_tables.py`, `replay/replay_check.py`, `tools/codex/check_cloud_volumes.py` | notes/70 |
| `render/` | Actor drawing, sky props, prop depth / fog / GTE / SZ helpers | `tools/codex/check_native_actor.py`, `check_sky_props.py`, `check_prop_*.py` | `notes/port/native_prop_*.md`, `native_sky*.md` |
| `text/` | Font and text layout (`tl_text`) | `tools/native_text/check_text.py` | notes/32 |
| `ui/` | Hint message box and presentation (OpenGL) | `tools/codex/check_native_presentation.py` | `notes/port/native_presentation.md` |

Generated validation reports live in `notes/port/validation/`; hand-written design notes in `notes/port/`.
Test harnesses that need the extracted game data (`player_review_bridge.c`, `volume_review_bridge.c`) stay in
`tools/codex/`. `tools/run_checks.py` runs every checker (over an hour).
