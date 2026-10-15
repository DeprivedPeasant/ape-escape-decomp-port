# PlayerUpdate batch: complete

## Wiring pass 4 update (2026-10-15)

This supersedes the historical area-05 ledge observation and SDK Y-rotation
boundary below. RotMatrixY now executes its exact C maths port in the game
host; the original SDK entry executes in the area oracle. The new pause and
progress modules run their ports. All four areas pass 90 frames plus loads
and actor/state bootstrap checks (372 comparisons), with PlayerUpdate still
executing original instructions. Exact rotation changes area 05 to water
state 46 and area 44 to state 2D; both start net state 80. Areas 01/02 complete
the net sequence 80/81/83. The separate successful-ledge regression remains.
The complete checker again passes 2,700 enclosing cases and 436 TurnTowardsY
cases. Viewer/standalone comparisons remain byte exact across all 2,098,176
bytes, and their missing counts are now zero/six for areas 01/02.

PlayerUpdate @80060780 is one hand-written structured C function,
`ae_PlayerUpdate`, in `src/player/ae_playerupdate_frame.c`. Its static helpers
divide the original into readable phases; they do not replace the enclosing
function with independently callable fragments. The service contract lives in
`ae_playerupdate_frame.h`. Game logic calls existing ports directly; SDK maths
uses the verified implementations. Loaded action and mode addresses go through
the central host dispatcher.

## Enclosing-function validation

`tools/mips/check_playerupdate.py --coverage` runs 2,700 complete PlayerUpdate
cases and 436 TurnTowardsY cases on real area members 05B, 064, 066 and 05E
(areas 01, 02, 05 and 44). The whole-function suite compares callee sequences,
every main-RAM byte outside the declared stack window, and all 1,024 scratchpad
bytes. It uses named callee doubles to isolate the enclosing function; the
area integration checker separately executes real ported callees.

PlayerUpdate executes 2,969/2,986 instructions. The remaining 17 have registered
proofs: nonnegative idle variant increment; the impossible masked fluid arm;
positive slope-length divisors; excluded hazardous surface sound defaults;
the impossible surface kind >=8; and the impossible negative absolute value
of a signed-halfword stick lane multiplied by four. None is an untested
reachable branch. TurnTowardsY executes 71/71 instructions.

`--mutate` catches 31 whole-function mutations and all 12 leaf mutations.
The independent portion checker still catches its 16 mutations. Missing
services fail closed. No new gameplay RAM masks were added.

Three declared shared inputs expose original undefined caller state: the
unwritten normal/position padding in PlayerUpdate's local frame, and the
leftover a1 of PlayPlayerVoice on the lethal-ground path, and the camera-script
table consumed by PlayerSetMode when entering actor mode. The local-frame
service preserves supplied bytes instead of zeroing the normal. For the real
host, voice ID 10 always chooses SFX 0x8A through 800689CC..800689D0, so its
fallback argument cannot change behavior; the default supplies 0x8A. The
whole-function doubles still check the exact original argument via an explicit
service. Exclusion rows added in this continuation remain for owner review;
no existing exclusion was weakened or min_cases lowered.

PlayerActorCameraTable supplies the last input through the existing
AePlayerActHooks.incoming_a2 contract. The complete update copies that logical
table into its setter context. The host requires an explicit provider when
entering actor mode and fails closed if it is absent; it does not invent a
zero table. Normal movement, jump and net paths do not enter that mode.

## Host and viewer

Mode_Stage calls PlayerUpdate through the central port table. It no longer
invokes a PlayerUpdate frame callback. `generate_game_calls.py` and
`generate_game_hooks.py` generate the typed service wiring and retain the
native presentation interface. XaPlay executes its verified RAM scheduling
port; its CD SDK operations remain named hardware boundaries.

`check_game_area.py` runs original PlayerUpdate without an entry interception
for 90 frames in each required area. Raw packets move, hold/release jump and
flick the net stick; the original PadSampleStickHistory also runs each frame.
States 00, 05, 08, 09, 80, 81 and 83 execute in areas 01, 02 and 44.
Area 05 instead reaches state 0E (ledge grab), preventing the later net swing. All 360 frames,
four area loads and four bootstrap state selections match (368 comparisons).
The explicit boot fixture loads the real player tables, initializes a healthy
net-equipped player and ready controller, and marks sound banks unloaded;
it never copies diverged gameplay bytes from the oracle between frames.
PlayerInit is still outside this batch. Existing integration masks are retained.

Viewer --game now uses the complete port. `viewer_player_bridge.c/.h` are
deleted. Boot loads member 008 and its RAM table pointers, while GL reads a
presentation snapshot from the game RAM after each frame; native player
movement is never copied back into RAM. The standalone host and GL viewer
share one input script. `check_viewer_game.py` passes 120 scripted frames,
two repeats and neutral-input frames in areas 01 and 02, comparing all
2,098,176 RAM/scratchpad bytes. The SDL 3.4.18 SDK was reused read-only from
the main checkout via process-local SDL3_DIR; build outputs remain here.

This does not complete unrelated systems: BodyMove (player/RC car) is still
the existing named host boundary in this worktree, pending the other job.
AnimFramePtr, some renderer/model initialization, HUD and area-overlay hooks
remain visible in viewer diagnostics. The viewer bridge is removed despite
those open boundaries; original body integration is not claimed here.

The additional `check_game_area.py --frame-branches-only` diagnostic reaches
pause/squash FX rendering and reports a ProjectEffectPoint versus matrix-setup
callback ordering difference. It reports no gameplay RAM difference at that
failure. This renderer diagnostic is not counted as a successful comparison
and is not masked away; the required four-area movement/jump/net run passes.
Resolving that native FX/SDK boundary is outside the two original functions
owned by this batch.

The required check_game, check_game_calls and check_game_wiring_mutations
suites pass. The call-table suite exercises 862 wrappers / 179,755 calls,
including eight real-service differentials through PlayerUpdate's central
adapter. All 39 executable wiring mutations are caught. Its fixed symbol
inventory is cached per worker instead of rereading the ledger for every RAM
helper lookup. check_verified passes with three pre-existing warnings
(ModelDrawWithOt's stale exclusion and PadUpdate/CameraLookAt coverage gaps).

## Findings and named boundaries

The central PlayerCheckGadgetSelect adapter now composes the already verified
decision helper with the complete PlayerSelectGadget port, preserving its
return and side effects. No new original function outside this batch is
claimed in the ledger. The old central ScaleMoveVel wrapper incorrectly
treated its C helper addresses as original ABI arguments. The original
80065C20 entry has no arguments and installs globals itself; its special
adapter now supplies those globals. TurnAroundAndJump, which delegates to
ScaleMoveVel, needs the same adapter. A proven existing leaf bug was also
corrected: gadget-4 controller fields clear on every gadget-4 jump, including
modes other than 1 (80065C4C branches directly to 80065C6C). The action checker
now exercises these modes, and central helper cases use garbage incoming
arguments to verify the adapter ignores them.

Open direct boundaries are PlayerAnimateAndDraw (native renderer),
FUN_80142808 (loaded overlay logic), XaPlay and CdMgrQueueCommand (CD/XA
archive/hardware), CdIntToPos and CdPosToInt (SDK), memset (SDK), and RotMatrixY
(SDK in TurnTowardsY). The two loaded mode callbacks at 800B4DC8/800B4DC0
remain address-carrying PlayerModeCallback services: the central table calls
ported identities and reports unported identities. All 40 real action-table
entries have existing ports; no action-table handler was invented. See
`playerupdate_calls.md` for the direct call and table inventory.

The remaining sections record the earlier continuation's range evidence;
their former next-step list is historical, not outstanding work.

## Completed leaf, verified again

TurnTowardsY @80066C90 lives in `src/player/ae_playerupdate.c` with its hook
contract in `ae_playerupdate.h`. `tools/mips/check_playerupdate.py --coverage`
passes 436 cases across real areas 01, 02, 05, and 44, with 71/71 original
instructions executed. `--mutate` catches all 12 deliberate mutations. Its
central game-host adapter and RotMatrixY wiring were already in this worktree
when this continuation began.

## Verified portions of the enclosing function

`src/player/ae_playerupdate_internal.h` contains three hand-written static
helpers intended for the eventual complete PlayerUpdate. They are currently
used only by `tools/mips/playerupdate_test_api.c` and the range checker
`tools/mips/check_playerupdate_fragments.py`; production behavior is unchanged.

| Helper | Original range, inclusive | Cases | Instructions |
|---|---|---:|---:|
| playerupdate_copy_input | 80060A30..80060C64 | 864 | 142/142 |
| playerupdate_select_tables | 80060C68..80060D04 | 120 | 40/40 |
| playerupdate_prepare_vertical_move | 800614E8..80061690 | 2640 | 105/107; 2 proved unreachable |

The checker loads the real EXE and four real area members, randomizes input
and scratchpad contents, and compares **all** main RAM and scratchpad bytes.
There is no stack mask or shared stale-stack input. Range execution begins at
the stated start with s4 = the scratchpad address, established by PlayerUpdate's
prologue. It stops at the continuation for input, or at the first callee entry
for the other helpers, after the original call delay slot has executed. These
are range tests, not substitutes for whole-function differential validation.

Default coverage and counts are saved in
`extract/checks/results/check_playerupdate_fragments.json`, explicitly marked
`fragment_checks_only`. `--mutate` catches all 16 fragment mutations, covering
masks, health and lock predicates, signed gadget indexing, state threshold,
table selection, water dispatch, buoyancy, gravity, clamp limits, negative
division rounding, and fraction consumption. Together with the leaf's 12,
28 mutations have been caught; this does not validate the missing code.

One append-only exclusion row records the proof for 80061570..80061574:
groundType is masked to 0..15; reaching 80061568 requires value < 2 and != 1,
so it is necessarily 0 and that branch must be taken. Its otherwise arm is
dead for every RAM image, independent of the chosen scenarios. The row is
available for the project owner's review; no min_cases was reduced.

## Instruction findings

The old `notes/decomp/playerupdate.c` pseudocode is incomplete at the water
state switch: it represents the jump-table branch @80061518 as an indirect
call followed by return. The real branches continue through vertical movement
preparation @8006165C and PlayerCheckGadgetSelect into the rest of PlayerUpdate.

States 49/4A clear integer velocity, vertical fraction, and horizontal target
velocity before joining the buoyancy path; 46/47/4C enter that path without
clearing those fields. State 4D clears integer velocity and skips gravity.
States 48/4B and all states outside 46..4D use normal gravity. Fluid kind 1
halves vertical integer velocity with division toward zero, applies buoyancy
when posY-156 is below waterY, then clamps integer velocity to [-16,16].
The fraction is subsequently consumed into fixed-point vertical move velocity
and cleared, with unsigned wrapping shifts and additions.

Input health checks are `health != 0`, including negative health. Restricted
input may still preserve right-stick aim on death when aim-mode byte == 1 and
stage lock != 11. Active aim/camera bytes must equal 1, not merely be nonzero.
Camera/aim-mode's held mask overrides the earlier aim-request mask. The
current gadget uses signed byte indexing for PlayerStateSets.

## Earlier continuation plan (completed)

1. Hand-port the complete enclosing function, calling existing ports directly,
   with exact mode and action-table dispatch. Use the helpers above as parts
   of that single original function. There is no partially callable public
   PlayerUpdate stub.
2. Cover idle streaming setup/teardown, contact and attachment handling, mode
   dispatch, velocity restoration, surface and air handling, facing/turn
   modes, movement modes, slopes and hazards, transitions/actions, tilt and
   gadget effects, velocity clamps, renderer boundary, animation events,
   blend/timer cleanup. The primary listing is
   `extract/codex-task/playerupdate.listing.txt`; call map and dispatch tables
   are in `notes/port/playerupdate_calls.md`.
3. Preserve the original unwritten local normal on paths that skip its
   initialization: tilt handling after non-default modes can read that local.
   This needs an explicit stale-stack service and a declared checker input,
   not an invented zero normal. Recheck call arguments against instructions,
   particularly PlayPlayerVoice's fallback argument and CollisionBodyRegister.
4. Add whole-function randomized/edge/real-area tests and mutations to
   check_playerupdate.py, complete coverage/proofs, and append PlayerUpdate's
   ledger row only with its actual status and case count.
5. Wire the complete function and its services into the game host. Change
   check_game_area to execute original PlayerUpdate without interception and
   exercise movement/jump/net pad scripts for all four required areas. Current
   area-frame success still uses the frame callback boundary.
6. Switch viewer --game to the complete port, then delete viewer_player_bridge
   and update check_viewer_game. The concrete blocker is the missing enclosing
   PlayerUpdate and integration proof, not a renderer dependency discovered
   by these range checks.

Named boundaries still required by the enclosing function include
PlayerAnimateAndDraw (renderer), FUN_80142808 (overlay logic), XaPlay and
CdMgrQueueCommand (archive/CD hardware), CdIntToPos/CdPosToInt (SDK), and SDK
memset. Mode callback tables must route their actual loaded addresses to
existing ports or named original-function hooks. TurnTowardsY currently has
the named RotMatrixY SDK hook. Existing gameplay callees and their sources
are listed in the call map; none was newly ported outside this batch.

No files under src/bodies were changed, no user settings were changed, and no
commit was made. Git ownership checks are handled for read-only commands by
passing the already-existing `extract/codex-task/git-readonly.config` as
GIT_CONFIG_GLOBAL in a subprocess environment.

## Regression results in this continuation

- check_game: passed 1,309 cases, 498 frames, 1,127 direct comparisons.
- check_game_calls: all checker batches passed; 863 central wrappers exercised
  over 179,751 cases. The monkey level 5 batch took substantially longer than
  the other batches, but completed successfully.
- check_game_wiring_mutations: 36/36 caught.
- check_game_area: four area loads and 8/8 existing frames each for 01, 02,
  05, and 44 matched (36 comparisons). This baseline still intercepts
  PlayerUpdate and therefore does not satisfy this task's integrated-player
  requirement.
- check_verified: final run passed with zero errors and three warnings in
  other batches: ModelDrawWithOt's declared-unreachable range executed;
  PadUpdate and CameraLookAt have undeclared default-scale coverage gaps.
  Their ledger/exclusion rows were not changed. Missing-results warnings
  present before the central-call run were resolved by that run.
- TurnTowardsY default and mutation checks, and the new fragment default and
  mutation checks, passed as described above.
- git diff --check: passed. tools/check_verified.py was not edited.

## Merge integration, pass 3

The final host also executes the PlayerAnimateAndDraw port through
AePlayerUpdateFrameHooks, rather than bypassing animation in the host. The
boot fixture initializes the real member-008 actor before any update. The
previous null model left a zero bone count and an invalid do-while bone loop,
which overwrote g_PlayerStateSets and eventually faulted at FFFFF400.
The four area runs pass 372 comparisons: four loads, four actor initializations,
four state selections and 360 frames. PlayerTryLedgeGrab's copied vector
padding has a separate explicit shared-input service; no player byte is masked.
See game_module_bugs.md for instruction evidence and old-source rejection.
