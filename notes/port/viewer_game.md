# Integrated native viewer game mode

## Resident stage mode update (2026-10-15)

`ae_game_frame` now schedules the same body and exit used by the ported
Mode_Stage loop; area loading follows the original render-init/request/flip
setup. The debug bootstrap then selects normal player mode through two
verified PlayerSetMode calls. Display, VRAM clear and frame-flip operations
have explicit native presentation callbacks. See [modes.md](modes.md) for
the original instruction proof and the remaining startup boundaries.

Both areas still match all 2,098,176 bytes in two 120-frame repeats and the
eight-frame neutral run. Original stage setup reaches five previously
unseen unported helpers; the generated missing table below now reports
5 names for area 01 and 11 for area 02. These counts supersede the historical
wiring-pass counts below. The viewer manifest also links the existing 18B
module and its exact ApplyMatrixLV dependency.

## Wiring pass 5 update (2026-10-15)

The shared viewer/CMake manifest includes ae_ovl18c.c. Area-02 overlay entries
now dispatch through their member-18C ports; their original_call hooks invoke
the central table by address, and renderer hooks use the synchronous typed
AeGameHost.render_words callback. Native name and word-array pointers remain
pointers, including requests with 6, 8 and 11 words. The viewer retains these
as presentation notifications and preserves the existing packet cursor for
EffectDrawRecord; original packet rendering remains incomplete.

Both areas pass two 120-frame repeats and eight neutral frames, comparing
every one of 2,098,176 bytes. The actual-run table below has no missing names
for area 01. Area 02 has six EXE helpers: HalfPipeRangeTest, ObjFacePathPoint,
ObjFollowPath, ObjMove, ObjSetPosToPathPoint and ZakoHooksAdd. All former 18C
missing entries are gone. This section supersedes the historical pass counts.

## Wiring pass 4 update (2026-10-15)

Pause-menu and progress sources are in the shared viewer/CMake manifest.
PauseMenuUpdate, DrawTextSprites, SoundLoopsPause and OpenPauseMenu run their
ports directly from Mode_Stage. Pure SDK X/Y/Z rotation runs exact C maths;
GPU, VRAM and UI presentation retain named callbacks. The new sound pan and
bank-volume callbacks are explicit logged no-ops pending native voice mapping.
Native pause-menu packet rendering is still incomplete.

The final actual-run missing table below supersedes older counts: area 01 has
zero missing names and area 02 has six. Both areas pass two 120-frame hidden
viewer/standalone repeats and an eight-frame neutral comparison, with every
one of 2,098,176 RAM/scratchpad bytes matching and no masks.

Game mode is the default launch configuration in `tools/codex/build_native_viewer.py`. Use `--legacy-demo` for the earlier player/camera/indicator prototypes. The raw executable selects integrated logic with `--game`; bare scene inspection remains available for the existing renderer checker.

```powershell
.venv/Scripts/python.exe tools/codex/build_native_viewer.py --area 0x01 --keyboard-only --run
.venv/Scripts/python.exe tools/codex/check_viewer_game.py --frames 120
```

## Wiring

- `src/app/viewer_game.{c,h}` owns the session and a single game AeRam. It reads the PS-X EXE, finds the area and overlay members from the original area load chain, supplies their bytes through AeGameHost and calls `ae_game_load_area`. The boot-owned heap cursor starts at 0x80100000. Overlay bytes are data for the native ports, never executable instructions.
- SDL keyboard/gamepad samples are serialized to the SDK packet: status 0, id 0x73, active-low buttons in bytes 2/3, right stick X/Y in 4/5, left stick X/Y in 6/7. Port 2 is disconnected. The original PadUpdate runs once inside `ae_game_frame`; the original stick-history sampling runs at the native frame epilogue. Game ticks use a 30 Hz accumulator; hidden `--frames` runs use fixed ticks and do not require keyboard focus.
- The viewer runs the real `ae_PlayerUpdate`, `ae_PlayerAnimateAndDraw` and, when active, `ae_RcCarDraw`. Boot reads member 008's model header and initializes the player actor with `ae_ModelInitAuto`, then loads its state sets and parameters. The deleted viewer_player_bridge is no longer part of this path. The native player structure is a presentation snapshot read from RAM after the frame; it never supplies movement results. Named renderer notifications feed the existing scene presentation; original HUD drawing and packet fidelity remain pending.
- DrawTerrain/DrawAreaProps and frame presentation callbacks notify the existing scene rendering pass. Monkeys use AE_MK position, facing, animation and visibility/activity fields with the existing actor renderer. Removed/caught activity 2 and hidden flags are skipped. Spawned enemy records, live pickups, projectiles, particles and effect objects are displayed as debug crosses only: red enemies, yellow items, magenta projectiles, cyan effects. This visualization never updates actor RAM.
- CameraUpdate supplies the RAM eye/target at 0x800F45A8/0x800F45B0 to the native GL view. Its CameraLookAt boundary uses the already verified yaw-only helper for the next PadUpdate. Original GPU view/projection/packet emitters are still presentation boundaries; no full original projection parity is claimed. V toggles the free camera without changing game logic.
- AeSoundHw.wait_idle synchronizes the existing SDL audio stream. SoundStop now executes the verified RAM port. Its sequencer/bank/voice helpers remain named missing game functions; the obsolete whole-SoundStop native callback was removed. Native voice mapping remains pending. The existing per-area bank starts BGM playback. CPU wait_loop is a no-op. ADSR, hardware key-off masks and per-channel volume are explicitly logged no-ops because RAM sequencer channels do not identify the native mixer handles; BGM fades/track changes and gameplay SFX sequencing remain unwired. Sound diagnostics are logged once. No fake hardware registers were added.
- Missing original logic is reported once per name/address pair, listed again at exit and counted in the window title. Unknown indirect targets retain their actual address. Native renderer notifications are presentation boundaries, not missing gameplay callbacks.

## Old module removal

The viewer no longer uses the approximate monkey module, catch sphere, NET_REACH/NET_RADIUS or developer N catch. With no remaining code consumers, `src/monkeys/native_monkeys.c`, `native_monkeys.h`, `check_monkeys.py` and `test_monkeys.c` were deleted. N still cycles actor clips in the scene-only inspector. Catching awaits the original player animation-event/body path; no replacement approximation was added.

## Validation

- Python viewer build and CMake Release build pass with warnings treated as errors. CMake Release uses -O2 to match the Python builder (the installed GCC 8.1 emits an existing maybe-uninitialized warning in native_player.c at -O3).
- `check_viewer_game.py`: areas 01 and 02, 120 scripted frames each, two independent hidden GL viewer runs per area. Every one of 2,098,176 main-RAM/scratchpad bytes equals a standalone SDL/GL-free host using the same real player ports and input. No exclusions or masks. Both areas also pass an eight-frame neutral-input comparison without window focus. Missing lists match between viewer and standalone runs.
- `check_native_viewer.py`: 101 area loads, seven GPU captures, 20 malformed-pack checks, zero failures. Final quick rerun: four representative areas, seven captures, 20 malformed packs, zero failures.
- `check_overnight_demo.py`: 540 combined native frames, zero failures; rerun after the final integration fixes.
- `tools/check_verified.py`: zero errors. This worktree has no saved differential result reports, so it reports 45 coverage warnings. The checker and verification ledger were not modified.
- Native audio game-mode smoke: per-area bank playback produces audio frames; RAM remains error-free. Full matching sound control is not claimed.

Artifacts and detailed stdout are ignored under `extract/codex-task/`. Game validation writes `extract/codex-task/viewer_game/report.json`, standalone/viewer RAM dumps, traces, logs and PPMs. The inspected screenshot `extract/codex-task/viewer_game/area_01.png` is the eight-frame neutral area 01 capture: Spike in the foreground, grassy Fossil Field terrain, tree trunks, cliffs, a monkey on the raised stone and yellow debug item markers on the stepped platform. The camera comes from game RAM. The 120-frame scripted capture shows Spike falling beside a cliff, an existing incomplete-player/camera limitation rather than a determinism failure.

## Missing functions reached

Generated by `tools/codex/check_viewer_game.py` from the actual final runs. Areas 01/02 use real PlayerUpdate and PlayerAnimateAndDraw, with two 120-frame repeats each. Counts: 01: 5, 02: 6.

| Name | Address | Areas |
| --- | --- | --- |
| ItemsClearFlags | `800847C4` | 01, 02 |
| LevelMonkeyCountGet | `800965EC` | 01, 02 |
| PlayerInit | `8003272C` | 01, 02 |
| PointInBoxXZ | `80073B64` | 02 |
| SoundSilence | `80086148` | 01, 02 |
| StageExitStart | `8004D73C` | 01, 02 |

## Changed files

- Added `src/app/viewer_game.{c,h}` and `src/app/viewer_player_bridge.{c,h}`.
- Updated `src/app/native_viewer.c`, `native_player_host.{c,h}` and `native_audio_output.{c,h}`.
- Updated `CMakeLists.txt` and `tools/codex/build_native_viewer.py`; added the shared game source manifest `tools/codex/viewer_game_sources.txt`.
- Added `tools/codex/check_viewer_game.py`, the standalone `viewer_game_harness.c` and this report.
- Deleted the four old monkey module/checker files listed above.

Changes are left in the working tree for the project owner. No commits, game data or downloaded tools were added.

## Game-host wiring update (2026-10-15)

The shared source manifest now includes ae_stagefade.c and ae_zakoai.c (ae_misc_batch.c was already included). Both areas again pass two hidden 120-frame viewer runs against the standalone host: all 2,098,176 RAM/scratchpad bytes match, with no masks. Eight-frame neutral-input comparisons also pass. The updated missing table above is regenerated from extract/codex-task/viewer_game/report.json. Final tools/check_verified.py result: zero errors, nine coverage/inventory warnings, with saved default-scale results for all 47 module checkers.

## Pass 2 integration (2026-10-15)

The shared source manifest includes ae_bodymove.c and ae_hud.c. The game now forwards six HUD draw kinds synchronously through AeGameHost.hud_draw with an unchanged const AeHudDraw pointer. Viewer logic runs all HUD state/cursor updates; its HUD draw implementation currently does nothing. The renderer does not yet display the original HUD. Body movement uses the real module dependencies instead of a whole-movement missing callback. See game.md for final checks and game_module_bugs.md for the ordering-entry correction.

Final pass-2 validation: all 49 central-wrapper suite reports pass (931 wrappers,
180,461 adapter invocations); check_verified.py finds all 49 saved module reports
and reports zero errors, 13 coverage/inventory warnings. Final viewer report
again has zero failures, nine missing functions per area and exact full-image
agreement for both 120-frame repeats. The HUD callback remains a presentation
no-op; this task verifies game-host integration, not visible HUD rendering.

## Pass 3 integration (2026-10-15)

This section and the generated missing table supersede historical pass-1/2
counts above. Both areas pass two 120-frame scripted viewer/standalone
comparisons and an eight-frame neutral comparison: all 2,098,176 bytes match,
without masks. The actual missing counts are 2 in area 01 and 8 in area 02.
The five-word ModelFillVertexColors request has its own named callback;
other original model/primitive operations use named renderer notifications.
Those notifications do not implement original packet rendering. HUD remains
a typed presentation no-op. The same scene renderer consumes the final RAM
snapshot, with the existing projection/audio limitations described above.

The real actor bootstrap fixes the null-model corruption found by the MIPS
integration test. PlayerUpdate, PlayerAnimateAndDraw and RcCarDraw have no
whole-function frame-host fields. MenuWindowsRun, MessageBoxUpdateDraw and
StageDrawScreenOverlay also call their ports directly from Mode_Stage.
