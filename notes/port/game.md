# Integrated game host (2026-10-14)

## Wiring pass 10: reconcile mode and stage CD boundaries (2026-10-15)

The merged mode checker was wrong at the archive boundary. Its second
verified-port filter removed `CdMgrQueueCommand` (`80012360`) from the MIPS
hook set, undoing the area checker's explicit exception for this native
archive service. The host correctly retained pass 9's native queue routing
for XaPlay and its other callers. Original XaPlay calls the queue at
`80011FF0`, `8001202C` and `80012060`, then CdIntToPos at `80012070`.
Executing the queue only on the mode oracle omitted its boundary event and
modified queue RAM only there: the pre-fix run fails at event 5430 with host
command 11 versus oracle CdIntToPos. This was an inconsistent test boundary,
not evidence of an incorrect queue algorithm or a gameplay port defect.

The mode oracle now retains that same native boundary. The standalone
player1 checker continues to prove the original queue implementation and
XaPlay's fallback. `check_game_modes --mutate` explicitly removes the queue
hook again and requires the former queue/CdIntToPos mismatch; its normal
fixtures compare all three mode sequences against original instructions.
Queue callbacks also compare the pointed-to four-byte parameter word, as
copied by the original at `80012468`, including XaPlay's caller padding.
Otherwise moving to the native boundary would hide the existing padding
mutation in excluded stack storage; that mutation must still be rejected.
No new game functions, masks, exclusions or ledger changes are involved.


## Pass 7/8 merge: bounded mode guards (2026-10-15)

The former mode-35 missing-entry guard became invalid when save-menu part 1
was merged. It entered the now-ported synchronous Mode_SaveMenu loop after
the last native-leaf guard, with raw tracing bypassing the normal log limit.
The seeded native-leaf words and SeqPlayer_SaveRestoreVram callback were not
the cause. The guard now checks the merged ledger before probing an absent
entry. The sequence-6 fixture stops at the member-186 entry through the existing
host continuation service; the oracle stops before the same first instruction.
It does not claim save-menu execution coverage. Its owning
module checker and central-wrapper suite retain that coverage.

The test shim caps each bounded native invocation at 20,000 callbacks and
reports an AeRam error before unwinding. Raw tracing also caps the log, and
Python callback failures request an immediate unwind. Public mode-word test
calls now use that bounded scope. Existing MIPS instruction limits remain.

## Wiring pass 7: Stage Start and merged small modes

The existing generated member/address table already listed members
183/184/185/186b/188/18A. Their previously absent module bindings now populate
call, native_call, overlay_call, renderer/render and nested dependency tables.
The hook generator ignores native-only hook types without ledger entry ports,
including 186d, rather than failing regeneration. Member-186 public MCX APIs
use the native `card_call` word-array callback and retain their member guard;
all 22 names and seven argument words are exercised. Pure rendering leaves
in 185/186 retain named callbacks and member guards. Unmerged save-menu game
functions remain named missing calls.

The original-main fixtures now cover three title selections:

| Fixture | Loaded modes | Equal frame boundaries | Stop |
| --- | --- | ---: | --- |
| Boot selection 2 | 181 -> 180 | 574 | main's CdMgrUpdate after title |
| Explicit new-game selection 0 | 181 -> 180 -> 188 | 665 | AreaKindVisitedSet, `800964DC`, at the end of StageStartUi_RunA |
| Explicit title selection 6 | 181 -> 180 -> 186 | 574 | Mode_SaveMenu, `80136AB8`, mode 35; part 1 is not merged |

These are archive/input fixtures, not an assertion that boot's sequence 2
itself selects new game. All non-stack RAM, every scratchpad byte, native
calls, and every frame-boundary hash match. The new-game fixture uses real
stage-start archive assets and reaches 913 VSyncs / 5,761 native calls.
It reaches no Mode_Stage frame: continuing that flow requires the unported
AreaKindVisitedSet helper, and this task explicitly forbids porting new game
functions. Existing boot/audio boundary doubles remain reported by name.

The integrated audit repairs the member-188 LookAt translation, rectangle
copies, initial wipe store, mesh-header mask, wipe divisor, rendering field
reads, and explicit XaPlay caller padding;
instruction evidence and old-source reproducers are in game_module_bugs.md.
The sole new exclusion-registry row declares the unwritten XaPlay caller
input and needs owner review. No RAM mask or ledger minimum/status changed.

The save-menu checker now uses a tracked declaration-only dispatcher and
the production dependency closure instead of two absent temporary C files.
The central-wrapper checker routes that dispatcher and member 188's test API
through the real adapters. Member 186d is explicitly included in check_game's
support sources; it and all newly merged modes were already in the viewer
manifest. No code or data outside this worktree was changed or committed.

The proxy build also retains MinGW's automatic exports when including the
188 test API and checks its exports before starting suites. The four small
overlay checker scripts use their shared harness entry point. These are
checker/link closure repairs, not alternate gameplay implementations.

Both viewer build caches now include generated `.inc` files in their cache
keys, so changing native dispatch routes invalidates the affected executable
and objects. The viewer and net-catch checks were repeated after this fix.

## Wiring pass 9: MissB integration (2026-10-15)

The generator recognizes native word-array parameters written as C arrays as
well as pointers, and emits an AeMissbCallFn adapter with its original
array-before-count argument order. It forwards through the central member/
address table and named native APIs. Renderer names use render_words with
all words intact; missing names retain their identity. The 66 MissB ports
are regenerated into the table, and all eight sources join the shared
viewer/CMake manifest. Verified SoundBankSetVolume and SfxVoiceSetPanVol
replace stale SDK routes; SfxVoiceSetPitch leaves the body missing table.

Area-05 frame 47 exposed mixed CD queuing: XaPlay called the queue port while
other host paths used the native asynchronous archive callback. XaPlay now
accepts that same boundary and keeps the queue C port as its standalone
fallback. The original eight calls and their parameters remain unchanged.
The module checker retains 48 fallback cases and adds eight native-queue
cases with payload comparisons. The former bypass is independently rejected.
Original instruction PCs and the boot/save/body unit-host repairs are in
[game_module_bugs.md](game_module_bugs.md). No arithmetic port bug was found,
and no exclusion row, comparison mask, ledger status or minimum was changed.

The integration checker adds 144 MissB port/renderer/missing/XA comparisons.
Four new executable mutations remove or corrupt that routing. Boot/save
unit hosts supply the newly reachable sound interfaces, run the sound ports
as original instructions, and retain their coverage requirements. Member
189's named unit exports now enter central adapters too. The save-score-1000
fixture uses real score bytes rather than a retired dependency double.

| Check | Final result |
| --- | --- |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons |
| check_game_area | 495 comparisons; four areas; catch frame 11, saved activity 2 |
| check_game_modes --mutate | Three fixtures; 1,813 frame boundaries; 14 focused rectangle/renderer/LookAt comparisons; 149 entry guards, 18 native leaves, 22 MCX routes; 10 old 188 sources and three earlier mutations rejected |
| check_modes | 802 cases |
| check_game_calls | All 68 suites pass after the full run and five focused overlay reruns; aggregate 1,232 wrappers / 188,434 invocations |
| check_game_wiring_mutations | 57/57 caught |
| check_game_module_regressions | 139 positive comparisons; all existing old sources rejected; 0 failures |
| check_viewer_game / check_net_catch | Full RAM/scratchpad repeats match; caught=1, capture=1, removed=1, saved=2 |
| check_verified | 68 saved checker reports; 0 errors; 171 warnings |
| Generators | Nine tracked artifacts plus the hook inventory reproduce byte for byte |

The twelve delegated helpers/partial interfaces and the existing unexercised
AreaGetMonkeyRecord adapter remain outside the exercised-wrapper count.
The initial proxy export failure and the old pre-fix 188 proxy run are
preserved in the task logs; the aggregate contains only passing reports.
Artifacts and a complete checker status summary are under extract/codex-task.

Changed files (25):

| Group | Files |
| --- | --- |
| Host API / dispatch | src/game/ae_game.h, ae_game_stage.c, ae_game_hooks.c, ae_game_native.inc, ae_game_platform.h |
| Stage Start fixes | src/modes/ae_ovl188.c, ae_ovl188.h |
| Generator | tools/generate_game_hooks.py |
| MIPS checkers | tools/mips/check_game.py, check_game_calls.py, check_game_modes.py, check_ovl186b.py, check_ovl188.py |
| Test support | tools/mips/game_integration_callbacks.inc, game_integration_hooks.inc, game_integration_shim.c, ovl186b_test_api.c (new) |
| Build cache closure | tools/codex/build_native_viewer.py, check_viewer_game.py |
| Registry | tools/mips/check_exclusions.csv |
| Notes | notes/port/game.md, game_hooks.md, game_module_bugs.md, modes.md, viewer_game.md |

| check_game_area | 687 comparisons; loads and all 90 frames in areas 01/02/05/44; original net catch at frame 11, saved activity 2 |
| check_game_modes / check_modes | Pass; 4,861 native calls; 240 ported-entry guards, 11 native leaves; 802 resident-mode cases |
| check_game_calls | All 73 suites pass after focused host repairs; 1,427 wrappers / 191,391 invocations |
| check_game_wiring_mutations | 65/65 caught; zero build errors or survivors |
| check_game_module_regressions | 284 comparisons; all former sources, including the XA bypass, rejected; zero failures |
| check_missa / check_missb | 1,280 / 434 original-instruction cases |
| check_player1 | Wrapper run passes; XaPlay has 56 cases, including eight native-queue payload cases |
| check_viewer_game | Two 120-frame repeats and eight neutral frames per area 01/02; all 2,098,176 RAM/scratchpad bytes match |
| check_net_catch | caught=1, capture=1, removed=1, saved=2; no RAM error |
| check_verified | 73 saved checker reports; 0 errors, 278 inventory/coverage warnings |
| Generators | All nine tracked artifacts reproduce byte for byte |

The actual viewer missing-function count is now zero in both areas. Its
checker regenerated the table in viewer_game.md. Native packet presentation
and voice mapping remain outside this integration proof. The existing
AreaGetMonkeyRecord wrapper gap and 14 explicitly delegated interfaces remain
outside the central-entry count. Every MissB wrapper is exercised. MIPS calls
retain hard instruction budgets; checkers ran sequentially without the prior
multi-GB growth.

The initial full sweep and focused repaired-host runs are retained under
extract/codex-task and extract/mips/call_proxy; the final command manifest is
extract/codex-task/final_manifest9.json. The aggregate uses only passing suite
reports. Changes are uncommitted for the project owner's review. Generated
files merge by rerunning the generators.

## Wiring pass 8: missa integration (2026-10-15)

Regenerated module wiring now includes AeMissaHooks, its nested dependencies
and indirect callbacks. The hook generator skips helper hook structs with no
game-host field, so regeneration no longer stops at AeOvl186dHooks. Audited
missa packet callbacks use renderer routes; the five-word trail-segment
callback has its own RenderTrailRingSegment identity rather than recursively
calling the public TrailRingDraw state port. BeginFrame uses the resident
mode's existing native frame service.

The area fixture and viewer supply real player member 008 before StageRequest
reaches PlayerInit. The empty-header reproducer independently exposed cached
reads in that port: preserve the original count/model-table reloads and the
loop's final index. The strengthened missa checker includes that input and
the module regression restores the former cached count to require rejection.
Instruction evidence is in game_module_bugs.md. No ledger requirement or
comparison exclusion was changed.

check_game_area passes all 495 comparisons for areas 01/02/05/44, including
all 360 scripted frames and the frame-11 net catch with saved activity 2.
check_missa passes 1,280 cases. PlayerAuxModelInit's native boundary compares
its one meaningful argument. PointInBoxXZ is removed from check_game's shared
missing table so the body oracles execute the newly ported function too.

The pad adapter now recovers PadUpdate's button word for the historical
zero-argument stick callback before calling StickToButtons. Focused integration
adds 12 real indirect-dispatch comparisons, 16 five-word trail-renderer fallback
checks, 16 enclosing PadUpdate comparisons and four stage archive out-pointer
checks. The area default now includes these 48 additional checks. Debug demo profiles supply valid neutral playback
records; fade transitions reuse the registered stage-fade renderer boundary
and preserve its native cursor advance.

The wrapper sweep also closes the small-overlay shared runners, save-menu
dispatcher identity / missing unit support, and stage-start's formerly bypassed
dispatcher. The two non-returning save workers retain their original module
iteration tests and join the explicitly delegated interfaces; they are not
counted as complete synchronous entry proofs. AreaGetMonkeyRecord remains the
existing unexercised adapter. See game_module_bugs.md for these test-host fixes.

Final validation (full wrapper sweep plus focused reruns of repaired hosts):

| Check | Result |
| --- | --- |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons |
| check_game_area | 543 comparisons: all four loads, 360 scripted frames, bootstrap/catch comparisons and 48 focused routes; catch frame 11, saved activity 2 |
| check_game_modes / check_modes | Pass; 4,861 boot/title native calls; 179 overlay-entry guards and 11 native leaves |
| check_game_calls | All 70 suites pass; 1,300 adapters exercised / 190,174 invocations; 14 explicit delegated interfaces and the existing AreaGetMonkeyRecord gap |
| check_game_wiring_mutations | 61/61 caught; no build errors or survivors |
| check_game_module_regressions | 140 comparisons, zero failures; old PlayerInit and all existing bad sources rejected |
| check_missa | 1,280 original-instruction cases, including the empty-header regression |
| check_viewer_game | Two 120-frame repeats and eight neutral frames in each of areas 01/02; all 2,098,176 RAM/scratchpad bytes equal |
| check_net_catch | caught=1, capture=1, removed=1, saved=2; no RAM error |
| check_verified | All 70 checker reports found; 0 errors, 202 inventory/coverage warnings |
| Generators | All nine tracked artifacts reproduce byte for byte |

Actual viewer runs reach four missing names in each area: ItemsClearFlags,
LevelMonkeyCountGet, PlayerAuxModelInit and SoundSilence. The table in
viewer_game.md is regenerated by its checker. Native packet presentation and
voice mapping remain outside this integration proof. No exclusion row, ledger
status or minimum was changed. Final logs, failed attempts, rejection sources
and the all-zero final command manifest remain under extract/codex-task and
extract/mips. Changes are uncommitted for the owner's review.

Changed files in this pass:

| Group | Files |
| --- | --- |
| Generated host wiring | src/game/ae_game_hooks.c, ae_game_platform.h, ae_game_native.inc |
| Generation and instrumentation | tools/generate_game_hooks.py; tools/mips/game_integration_callbacks.inc, game_integration_hooks.inc, game_integration_shim.c |
| Port and viewer fixes | src/player/ae_missa_player.c; src/app/viewer_game.c |
| Integration checks | tools/mips/check_game.py, check_game_area.py, check_game_calls.py, check_game_wiring_mutations.py, check_game_module_regressions.py |
| Module test closure | tools/mips/check_missa.py, check_ovl186b.py, ovl186b_test_api.c (new), ovl_game_stub.c |
| Notes | notes/port/game.md, game_hooks.md (generated), game_module_bugs.md, missa.md, viewer_game.md (actual-run missing table) |


## Wiring pass 6: mode overlays and original-main entry (2026-10-15)

Members 181/182/180/187/18B now use generated member-and-address adapters and
fully populated named/nested hook tables. `ae_game_run` enters ported main;
ArcLoadMember's native callback copies the requested bytes, successful mode
loads record the member, and ModeEntry selects only that member's port.
Unavailable mode entries call missing and terminate dispatch with an AeRam
error. Native archive/display/renderer APIs keep named callbacks; renderer
word arrays and the original five/seven-word call ABI retain their values.
Native overlay leaves also match member plus address, allowing future members
to reuse an address. Future C ports require normal declarations, ledger member
identity, the viewer source entry and regeneration; native leaves are table
entries in the hook generator.

The integrated original-instruction checker runs main -> boot (181) -> title
(180) with identical real EXE RAM and scripted archive/input callbacks. It
compares 574 frame-boundary RAM/scratchpad hashes, final non-stack RAM, all
scratchpad, and 4,861 native calls across 822 VSyncs / 1,247,124 instructions.
Only the existing stack window is omitted; no new masks or exclusions were
added. No ported entry is intercepted on the oracle side. The oracle changes
native hook sets and clears decoded instructions at real member loads.

Boot selects title sequence 2, which returns to mode 3; the integrated run is
bounded there. The new-game title branch selects mode 8, member 188 at
8013683C, currently the stage-start missing boundary. A separate dispatch
check requires that named stop and automatically adapts when 188 merges.
Native sequence archive/display services remain callbacks; the native viewer
still enters the stage directly. The bounded run reaches these unported
helpers: PadSetupDualShock (80012CA0), SeqTrackSetPlayState (800A2B6C),
SeqTrackClose (800A1F3C), SoundStopAllVoices (800A342C), SoundBankFree
(800A1CCC). It records their deterministic boundary results without claiming
those implementations.

Original-instruction audit fixed member 187 LookAt's discarded
ApplyMatrixLV translation and two member-181 clear-image rectangle errors.
Native clear callbacks now compare their complete eight-byte RECT inputs;
Mode_02 reads its original template, including edited-template test cases. The strengthened checker executes that SDK's real
instructions, and the old source is rejected on RAM. Test closure repairs
replace two missing temporary 187 support files, wire resident mode hooks in
the boot unit host, route both overlay test dispatchers through central
adapters, and update two obsolete vertical-contact inputs to pass the real
HalfPipeRangeTest. Details and instruction evidence: game_module_bugs.md.
No ledger minimum/status or exclusion was weakened.

| Check | Result |
| --- | --- |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons |
| check_game_area | 495 comparisons, areas 01/02/05/44; net catch frame 11, saved activity 2 |
| check_game_modes | 574 frame boundaries; 80 unloaded port guards; 11 native leaf routes/guards; five-word ABI; member mutation and both old rectangle sources rejected |
| check_modes | 802 cases |
| check_game_calls | All 62 suites pass across the full run and focused 181/187 repairs; aggregate 1,163 wrappers / 187,890 invocations |
| check_game_wiring_mutations | 57/57 caught; no build errors or survivors |
| check_game_module_regressions | 139 positive comparisons; nine old 18C, two old stage, one old flash and one old stage-clear sources rejected; zero failures |
| check_viewer_game / check_net_catch | Full-image repeat/neutral comparisons pass; caught=1, capture=1, removed=1, saved=2 |
| check_verified | 62 saved checker reports; 0 errors; 110 existing inventory/coverage warnings |
| Generators | All nine generated artifacts reproduce byte for byte |

The twelve delegated native/partial helpers and the existing unexercised
AreaGetMonkeyRecord adapter remain outside the central-entry count. The full
wrapper run had only the two repaired unit-host failures; focused reruns
exercise all 40 boot/intro/stage-clear adapters (242 invocations), and the
aggregate includes only passing reports. Logs, original failed attempts and
final proofs remain under extract/codex-task and extract/mips. Changes are
uncommitted for the owner's review.

Changed files (31):

| Group | Files |
| --- | --- |
| Host API and dispatch | src/game/ae_game.h, ae_game_stage.c, ae_calls.h, ae_calls_api.h (new) |
| Generated adapters/tables | src/game/ae_calls.c, ae_game_hooks.c, ae_game_modules.h, ae_game_platform.h, ae_game_native.inc (new) |
| Overlay fixes | src/modes/ae_overlay_hooks.h, ae_ovl181.c, ae_ovl187.c |
| Generation/build closure | tools/generate_game_hooks.py; tools/codex/viewer_game_sources.txt |
| Checkers | tools/mips/check_game.py, check_game_calls.py, check_game_modes.py (new), check_game_module_regressions.py, check_ovl181.py, check_ovl187.py |
| Test routing/instrumentation | tools/mips/game_integration_callbacks.inc, game_integration_shim.c, ovl_game_stub.c |
| Tracked unit support (new) | tools/mips/boot_overlay_test_api.c, ovl187_test_api.c, ovl187_test_stubs.c |
| Documentation | notes/port/game.md, game_hooks.md, game_module_bugs.md, modes.md, viewer_game.md |


## Resident mode ports (2026-10-15)

The host now runs the shared body and complete fade exit of the ported
Mode_Stage. Area loads execute StageRenderInit, StageRequest and ResetFrameFlip
in original order. main, Mode_04, Mode_06 and the five scoped helpers have
diffed ledger rows. See [modes.md](modes.md) for the per-function cases,
instruction evidence, boundaries and exclusion review items.

The mode checker passes 802 cases, all 1,024 reachable instructions, and ten
mutations. All four areas match their loads and 90 frames; the expanded
bootstrap and net encounter bring the area check to 495 comparisons. The
original catch occurs at frame 11 and persists activity 2. Frame-branch checks
also include the original post-loop exit now. Wiring mutations pass 57/57;
the regression suite rejects both former stage setup/exit behaviors alongside
its existing module reproducers (131 positive comparisons, zero failures).

The debug viewer explicitly selects normal player mode after original stage
entry using two verified PlayerSetMode calls. Viewer/standalone repeats still
match all 2,098,176 RAM/scratchpad bytes. The original setup exposes five
additional unported names: ItemsClearFlags, LevelMonkeyCountGet, PlayerInit,
SoundSilence and StageExitStart. Actual missing counts are now 5/11 for areas
01/02; viewer_game.md contains the generated names and addresses. These are
visible boundaries, outside this task's helper list.

Regenerated central wrappers also include the already-diffed 18B overlay.
The viewer manifest includes it and its existing exact ApplyMatrixLV source.
No external gameplay module required a new source fix. Changes remain
uncommitted for the project owner's review.

## Game-host wiring pass 5 (2026-10-15)

The central table now selects overlay entries by loaded member and address,
scanning all entries at an address rather than accepting only the first one.
An unidentified or unloaded member remains a named missing boundary. A test
table reuses two existing port adapters at one address for members 181/18C;
five alternating-member checks prove selection and unknown-member rejection.
No new gameplay function was ported.

The 26 ae_ovl18c ports are generated into the table. AeOvl18cHooks retains its
name-dispatch interface: original_call forwards address and meaningful words
through the central adapters, preserving up to seven o32 inputs and restoring
stack words; unknown requests retain their full name and array. Renderer
requests use AeGameHost.render_words, a synchronous native callback preserving
the name, address, count and every word, including 11-word surface requests.
Neither string nor array pointers are cast to integers. The existing nested
zako/world/spawn tables are wired normally. The viewer source manifest includes
ae_ovl18c.c and its presentation callback preserves EffectDrawRecord's cursor.

Adding a future mode overlay requires its normal public declarations and
diffed ledger rows with `overlay member XXX.bin` in notes, plus its source in
tools/codex/viewer_game_sources.txt. Run both generators. The declarations and
ledger automatically supply central wrappers, includes and module fields;
ordinary named hooks and the original_call/renderer name-dispatch convention
share this path. No address-specific dispatch switch is needed. Members 181,
182, 187, 188, 189 and 18B were not present as mode source modules in this
worktree, so no new mode gameplay was added here.

The inventory contains 1,078 wrappers for 1,073 addresses; 750 ordinary wires
in 38 tables plus two name-dispatch callbacks in the 39th table. Ordinary
routes: 326 ported, 187 missing, 109 SDK/platform, 97 renderer, 11 indirect,
seven archive, six typed HUD, five padding and two existing player services.
Both generators reproduce their tracked artifacts byte for byte.

Original-instruction integration exposed eight expression/store/branch
defects in 18C and its ambient caller-padding input. Minimal fixes and
independent old-source rejection are documented in game_module_bugs.md.
The only new exclusion-registry row is an explicit shared input for the
ambient halfword, pending owner review. No comparison mask or ledger
requirement was weakened. All 18C gameplay entries and ported EXE dependencies
execute their original instructions in the area oracle; only their reviewed
renderer blocks and still-unported EXE helpers are boundaries.

Final logs and reports are under extract/codex-task/ and extract/mips/.
Changes remain uncommitted for the project owner's review.

| Check | Pass-5 result |
| --- | --- |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons; 15,356 host calls |
| check_game_area, 01/02/05/44 | 372 comparisons: four loads, eight boot comparisons, 360 frames; zero failures |
| check_game_calls | All 56 suites pass; 1,065 wrappers exercised; 186,116 adapter invocations |
| check_game_wiring_mutations | 55/55 caught; no build errors or survivors |
| check_game_module_regressions | 49/49 positive comparisons; nine old overlay sources rejected |
| check_ovl18c | 216 cases across 26 functions; zero failures or coverage gaps |
| Overlay routing | Five real member guards; six array/port comparisons; five duplicate-address selections |
| Named host / typed HUD routes | 213 hooks / 1,704 comparisons; 36 typed HUD requests |
| Dispatch boundaries | 16 renderer forwards; 32 composed gadget selections; one internal-entry guard |
| check_viewer_game | Two 120-frame repeats and eight neutral frames per area; all 2,098,176 bytes match |
| check_verified | 56 saved reports; zero errors, 37 existing inventory/coverage warnings |
| Generator reproduction | All eight generated artifacts reproduce byte for byte |

The full wrapper run was followed by a focused overlay rerun after the final
checker cleanup. The aggregate includes only passing reports. AreaGetMonkeyRecord
remains unexercised by its existing suite; the twelve delegated helpers/partial
interfaces remain outside the central-entry proof count. The viewer's actual
missing list is empty in area 01 and contains six EXE helper names in area 02;
see viewer_game.md. Native packet rendering and voice mapping remain incomplete.

Changed files in this pass (21 tracked files):

| Files | Purpose |
| --- | --- |
| src/game/ae_calls.c, ae_game.h, ae_game_hooks.c, ae_game_modules.h | Member/address selection, overlay adapters/tables and typed renderer array boundary. |
| src/world/ae_ovl18c.c | Minimal fixes for the eight instruction mismatches and explicit ambient caller input. |
| src/app/viewer_game.c; tools/codex/viewer_game_sources.txt | Overlay source closure and native renderer notification/cursor handling. |
| tools/generate_game_calls.py, generate_game_hooks.py | Reproducible multi-member dispatch and native name-dispatch wiring. |
| tools/mips/check_game_area.py, game_integration_shim.c, check_game_wiring_mutations.py | Real overlay oracle continuations, typed array/member/duplicate tests and six new routing mutations. |
| tools/mips/check_ovl18c.py, check_game_module_regressions.py | Stronger focused cases, verified build closure and nine independent old-source rejection proofs. |
| tools/mips/check_zako1.py, check_zakoai.py | Record the two actual Atan2Safe inputs in shared checker events. |
| tools/mips/check_exclusions.csv | One documented ambient caller-input row, pending owner review. |
| notes/port/game.md, game_hooks.md, game_module_bugs.md, viewer_game.md | Final results, generated routing/missing inventories and original instruction evidence. |

## Game-host wiring pass 4 (2026-10-15)

Pause-menu and progress now participate in the generated central call table.
Mode_Stage calls PauseMenuUpdate, DrawTextSprites, SoundLoopsPause and
OpenPauseMenu directly; their former frame-host slots are removed. The viewer
source manifest includes ae_pausemenu.c and ae_progress.c. Both modules use
their real dependency tables, including progress's sound, gimmick and area
queries and pause-menu's pad/player/body/spawn services.

RotMatrixX/Y/Z run exact C maths in ae_sdk_matrix.c, including low-word
multiply/add/subtract, arithmetic shift and halfword truncation. Their SDK
entries execute on the integration oracle without interception. Existing
RotMatrix/CompMatrix/ScaleMatrix and related maths retain their ports. GPU,
VRAM, display, pan/volume and draw-page operations remain named platform or
renderer callbacks. Memset's capitalized pause-menu name aliases the existing
memory boundary. ModelEmitPrimsScratchColors is a renderer operation.
PauseMenuRun's declared caller_s5 input is explicitly supplied by
AeGameHost.pause_menu_caller_s5 (zero for native sessions); copied unit tables
preserve the checker's chosen value. No simulated register bank was added.

The inventory is 1,052 wrappers for 1,047 addresses and 750 hook wires in 38
tables: 323 ported, 190 missing, 109 SDK/platform, 97 renderer, 11 indirect,
seven archive, six typed HUD, five padding and two existing player services.
All eight generated artifacts reproduce byte for byte.

Integration exposed two missing shared water-height stores and a wrong depth
field; their minimal
fixes, original instruction evidence and old-source rejection are documented
in game_module_bugs.md. The integration oracle now also models the existing
inline wobble-GTE and effect-object renderer boundaries, preserving gameplay
instructions and comparing all scratchpad bytes. Animation and pause-menu
hooks use their unit checkers' meaningful argument counts. The pause-menu
and player2 standalone builders link the verified dependency closure instead
of their obsolete incomplete source lists. The progress wrapper checker also
exercises AreaKindVisitedGet when its optional query-hook table is null.

Exact SDK Y rotation changes the scripted area-05 and area-44 trajectories.
Both still move, jump and begin state 80. Area 05 enters water state 46;
area 44 reaches state 2D. Areas 01/02 still complete states 80/81/83.
Assertions record these original observations; no gameplay byte is masked or
copied to correct divergence. The old frame-branch pause-result doubles were
retired: modes 2/3 now explicitly test the original inactive-menu gate.

| Check | Final result |
| --- | --- |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons; 15,356 host calls |
| check_game_area, 01/02/05/44 | 372 comparisons: four loads, eight boot comparisons, 360 frames; zero failures |
| check_game_calls | All 55 suites pass; 1,039 wrappers exercised; 185,900 adapter invocations |
| check_game_wiring_mutations | 49/49 caught; no build errors or survivors |
| check_game_module_regressions | 13/13 comparisons; both old water sources and the old depth field rejected |
| check_playerupdate | 2,700 enclosing cases + 436 TurnTowardsY cases |
| check_pausemenu | 736 cases across 19 functions |
| check_progress | 213 cases across 25 functions |
| check_pose | 1,764 cases; 160 per new SDK axis; all 102 instructions per axis reached |
| Pause SDK hook routing | 96 direct original-instruction comparisons |
| check_player2 | 807 cases across 42 functions |
| check_effects | 840 cases; 44/44 mutations caught, including both old water behaviors |
| Named host / typed HUD routes | 213 hooks / 1,704 comparisons; 36 typed HUD requests |
| Helpers / dispatch / extended ABI | 768 helpers; 16 renderer forwards; 32 gadget selections; 48 extended comparisons |
| Frame branches | 13 frames / 52 comparisons |
| check_viewer_game | Two 120-frame repeats and eight neutral frames per area; all 2,098,176 bytes match |
| check_verified | 55 saved checker reports; zero errors, 13 existing coverage/inventory warnings |

The wrapper suite was executed in full, followed by a focused effects rerun
after the final depth-field fix; the aggregate contains only passing reports.
The existing AreaGetMonkeyRecord adapter remains unexercised by its unit
suite and is not included in the 1,039 wrapper count. The twelve delegated
helper/partial interfaces are also not counted as central-entry proofs.

The actual-run viewer missing list is empty for area 01 and has six names for
area 02 (viewer_game.md). Native packet/HUD rendering and voice mapping remain
incomplete. New pan/bank-volume boundaries are explicit presentation no-ops in
the viewer's sound diagnostic. No exclusion, ledger minimum or status was
weakened. The only new ledger rows are the three pure SDK axis maths ports.
Changes are uncommitted for the owner's review; logs and proofs are under
extract/codex-task/.

Changed files in this pass:

| Files | Purpose |
| --- | --- |
| src/game/ae_game.h, ae_game_stage.c, ae_game_frame_host.h | Direct pause/progress steps and explicit caller input. |
| src/game/ae_calls.c, ae_game_modules.h, ae_game_hooks.c, ae_game_platform.h | Regenerated typed adapters, module tables and native boundaries. |
| src/core/ae_sdk_matrix.c, ae_sdk_matrix.h; tools/verified.csv | Exact SDK axis maths and their three ledger rows. |
| src/player/ae_player2_leaf.c; src/effects/ae_effects_water.c | Water-height publication and distinct entry-depth field. |
| src/app/viewer_game.c; tools/codex/viewer_game_sources.txt | New module build sources and explicit sound pan/bank boundaries. |
| tools/generate_game_hooks.py | SDK/renderer identities, meaningful hardware arities, caller-s5 wiring. |
| tools/mips/check_game_area.py, check_game_wiring_mutations.py, check_game_module_regressions.py | Original-instruction area checks, renderer continuations, new routing mutations and module reproducers. |
| tools/mips/check_game_calls.py, check_pausemenu.py, check_player2.py, check_pose.py, check_effects.py | Optional-query adapter coverage, real build dependencies, SDK axes and strengthened water fixtures/mutations. |
| tools/mips/game_integration_callbacks.inc, game_integration_hooks.inc | Regenerated callback forwarding and test invocations. |
| notes/port/game.md, game_hooks.md, game_module_bugs.md, playerupdate.md, viewer_game.md | Current results, routing inventory, evidence and actual-run missing table. |

## Game-host wiring pass 3 (2026-10-15)

This section supersedes the historical pass-1/2 counts below. The failure at
FFFFF400 came from a null player model in the boot fixture: the original bone
builder overwrote g_PlayerStateSets, including the event-table pointer later
read by PlayerRunAnimEvents. Initializing the actor from member 008 with the
existing ModelInitAuto port fixes the bootstrap in both checker and viewer.
See game_module_bugs.md for instruction evidence and old-source rejection.

Mode_Stage and AePlayerUpdateFrameHooks now run PlayerAnimateAndDraw and
RcCarDraw through the central ports. Their model/primitive hooks route through
named renderer callbacks; ModelFillVertexColors preserves its fifth word.
The existing typed HUD route remains unchanged. Ported MenuWindowsRun,
MessageBoxUpdateDraw and StageDrawScreenOverlay also have no frame-host slots.
Remaining frame-host fields represent unported native/presentation steps.
XaPlay's RAM scheduling executes its existing port; CD SDK calls remain host
boundaries. DrawPrim and MatrixNormal have named SDK routes and reviewed
one-/two-word arguments. No new gameplay function was ported.

The final generated inventory contains 1,005 wrappers for 1,000 addresses and
676 hook wires in 36 tables: 280 ported, 184 missing, 96 SDK, 86 renderer,
11 indirect, six typed HUD, six archive, five padding, one local-frame service
and one voice-fallback service. Both generators reproduce the seven affected
tracked artifacts byte for byte; ae_calls.c/ae_game_modules.h are unchanged.

Integration also exposed RibbonDraw's incorrect first-point duplicate test
and an implicit ledge-vector padding input. The minimal RibbonDraw fix and
explicit LocalDirectionPadding service have strengthened regressions that
reject their old source. The padding is observed from original execution and
supplied to C; resulting player bytes remain compared. Two shared_random rows
register the same input for area/bodymove checks, pending owner review. No new
RAM mask, instruction-coverage exclusion, minimum reduction or status change
was added. Ported PlayerUpdate, animation and ledge functions execute original
instructions without entry interception.

| Final check | Result |
| --- | --- |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons; 15,356 host calls |
| check_game_area, 01/02/05/44 | 372 comparisons: four loads, eight boot comparisons, 360 frames; zero failures |
| Frame branch fixture | 13 frames / 52 comparisons, including RC car renderer calls |
| check_game_calls | all 53 suites pass; 992 wrappers; 184,471 adapter invocations |
| check_game_wiring_mutations | 46/46 caught; no build errors or survivors |
| check_game_module_regressions | 10/10 comparisons |
| check_playerupdate | 2,700 enclosing cases + 436 TurnTowardsY cases |
| check_bodymove | 160 cases; 64 hook calls; old padding source rejected |
| check_fxscript | 107 cases; 12 RibbonDraw cases; old first-point source rejected |
| Typed HUD / named host wires | 36 typed requests / 188 hooks and 1,504 comparisons |
| Special-interface helpers | 768 comparisons |
| Dispatch guard/boundaries | 16 renderer forwards; 32 gadget selections; one internal-entry guard |
| Extended ABI | 48 comparisons; 16 three-word stack restoration checks |
| check_viewer_game | two areas, two 120-frame repeats each, eight neutral frames each; all 2,098,176 bytes match |
| check_verified | 53 saved checker reports; zero errors, 13 warnings |

Area 05 reaches ledge state 0E before the scripted net flick. Areas 01, 02 and
44 exercise movement, jump and net states 00/05/08/09/80/81/83. The checker
asserts these area-specific observations instead of requiring a net swing
while the original player is hanging from a ledge. Collision-skip branch
fixtures use animation-only frames, exercising Mode_Stage's skip without
inventing an initialized capture/squash sequence. The live menu mutation
fixture closes through mode 7 after emitting DrawPrim; its modeled side effect
makes menu/message ordering observable after MenuWindowsRun was ported.
The seven-word test now targets the remaining FUN_8013DF54 overlay boundary;
the retired ObjAddHitBodyIfPlayerNear callback no longer exists in the module.

The real-player viewer has two missing names in area 01 and eight in area 02.
Its generated actual-run table is in viewer_game.md. Native packet rendering,
HUD presentation and sequencer-to-mixer voice mapping remain incomplete;
full-image determinism does not claim presentation fidelity.

| Changed files | Purpose |
| --- | --- |
| src/game/ae_game.c, ae_game.h | Existing XaPlay dispatch; explicit padding host service. |
| src/game/ae_game_stage.c, ae_game_frame_host.h | Direct ported frame steps; retire their host slots. |
| src/game/ae_game_hooks.c, ae_game_platform.h | Regenerated port/renderer/SDK wiring. |
| src/app/viewer_game.c | Real actor bootstrap and renderer notifications; remove animation bypass. |
| src/bodies/ae_bodies.h, ae_bodymove.c | Explicit ledge direction-padding input. |
| src/effects/ae_fxscript.c | Correct first projected point duplicate comparison. |
| tools/generate_game_hooks.py | Reproducible routes, native service and SDK aliases. |
| tools/codex/check_viewer_game.py | Regenerate the missing-function table from actual runs. |
| tools/mips/check_game_area.py | Real actor bootstrap, original-first padding observation, meaningful branch/ABI fixtures. |
| tools/mips/check_game_wiring_mutations.py | Updated direct-step, live-menu and seven-word mutations. |
| tools/mips/check_game_module_regressions.py | Add ledge padding and zero-origin ribbon regressions. |
| tools/mips/check_bodies.py, check_bodymove.py | Hook ABI layout and poisoned native local-storage regression. |
| tools/mips/check_fxscript.py | Zero-origin regression and verified dependency closure for builds/mutations. |
| tools/mips/check_exclusions.csv | Two documented shared-input rows; no masks added. |
| tools/mips/game_integration_callbacks.inc, game_integration_hooks.inc, game_integration_shim.c | Regenerated callbacks/invocations and explicit observed-padding service. |
| notes/port/game.md, game_hooks.md, playerupdate.md, game_module_bugs.md, viewer_game.md | Current counts, evidence, validation and generated missing list. |

Detailed stdout and proof copies are ignored under extract/codex-task/;
actual wrapper reports are under extract/mips/call_proxy/. Changes remain
uncommitted for the project owner's review. No game data or tools were added.



## Game-host wiring pass 2 (2026-10-15)

This section supersedes the earlier wiring counts. Regeneration produces 941
wrappers for 936 original addresses and 639 callback wires in 32 tables:
258 ported, 81 SDK/platform, 66 ordinary renderer, 6 typed HUD renderer,
210 missing, 9 indirect, 5 archive and 4 padding defaults. HUD and bodymove
are included in the shared viewer/CMake source manifest. No new game function
was ported.

Six AeHudHooks draw callbacks forward the const AeHudDraw pointer synchronously
to AeGameHost.hud_draw. They never cast it to a word or report missing gameplay.
ClearMemory uses the named memset platform boundary; ModelDrawScreenSpace uses
the named renderer boundary. The existing frame adapters now find and call the
ported MessageBoxUpdateDraw and StageDrawScreenOverlay entries.

CollisionUpdate's embedded body.move receives the generated body_move table,
including all nine module dependencies. Its stack base is the original entry SP
minus the 56-byte CollisionUpdate frame; direct bodymove wrappers retain the
entry SP. The body_move fallback has been removed by the merged module, and
its retired slot remains unused. XaPlay's RAM and CD queue logic runs its port;
CD SDK calls remain named host boundaries.

Ray-test contract: copied probe callbacks receive h->user unchanged. The game
passes g->misc directly and no longer unwraps an owner table. The misc checker
supplies and checks a non-null user sentinel for direct and copied callbacks.
Camera-update's separate nested-owner contract is unchanged.

Integration found an incorrect MessageBoxDrawWindow ordering-entry request;
the module fix, original instruction evidence and old-source rejection are
recorded in game_module_bugs.md. The already-fixed CollProbe_Box vertical
return is now reached by two explicit unit cases, checked through the central
wrapper, and covered by a new mutation. Restoring its old return-0 behavior
fails both cases. Restoring the old HUD bucket fails 36 cases.

The area fixture initializes a dedicated renderer arena in BeginFrame. Its
active-message mutation profile has a passing original baseline and explicit
menu/debug-text effects to make frame swaps observable. The registry records
only emitted HUD packet spans/ordering tags and the no-heightfield scratch
halfword at 1F8003FC. Standard sprite fields are decoded and compared; real
terrain slopes, packet cursor and gameplay state remain compared. Existing
particle exclusions now also cover boundary scenarios. Original group-local
padding is observed and supplied through the existing explicit padding hook;
no effect-object padding mask was added.

The custom HUD test dispatcher now calls public proxy functions, which invoke
the central typed wrappers. Its sound-ID observation leaves the real sound
port running. Older checker builds resolve new module dependencies against
the proxy library; misc's own standalone builder links the real verified module
closure. The retired monkey body_move test callback and the truncated ctypes
body-hook layout were repaired.

| Final check | Result |
| --- | --- |
| Central wrappers | All 49 suites pass; 931 wrappers; 180,461 adapter invocations |
| Ledger checker | 49 saved checker reports; 0 errors, 13 coverage/inventory warnings |
| Areas 01, 02, 05, 44 | 4 loads + 32 frames = 36 comparisons, zero failures |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons; 15,356 host calls |
| check_bodies | 6,313 cases; 1,047 hook calls |
| check_bodymove | 160 cases; 64 hook calls |
| check_misc | 297 cases, including non-null copied callback user assertions |
| check_collprobe | 1,306 cases; 44/44 mutations caught |
| check_hud | 580 cases; old ordering request rejected |
| Typed HUD wires | 36 request comparisons across all six draw hooks |
| Named host wires | 152 hooks; 1,216 comparisons |
| Special-interface helpers | 576 comparisons |
| Partial boundaries / internal guard | 32 forwarding comparisons / 1 guard |
| Extended ABI | 48 comparisons; 16 three-word restoration checks |
| Pause/fade/transition branches | 12 frames + 12 loads |
| Wiring mutations | 43/43 caught; no build errors or survivors |
| Module regressions | 8/8 comparisons |
| Viewer/standalone | 2 areas, 120 frames, 2 repeats; all 2,098,176 bytes match |
| Viewer missing names | 9 in each area; actual-run list in viewer_game.md |
| Generator reproduction | All 8 generated artifacts match byte for byte |

Files changed in this pass:

- src/game/ae_calls.c, ae_game_modules.h, ae_game_hooks.c, ae_game_platform.h:
  generated full ledger wrappers, new tables and typed HUD routes.
- src/game/ae_game.h, ae_game.c, ae_game_body.c: typed HUD host boundary,
  embedded movement wiring, legacy-slice dependencies and uniform ray user.
- src/ui/ae_hud.c and src/world/ae_misc.c: correct message ordering entry and
  document the existing copied callback user contract.
- src/app/viewer_game.c and tools/codex/viewer_game_sources.txt: explicit HUD
  presentation no-op and new module translation units.
- tools/generate_game_hooks.py: typed HUD generation, renderer/memory aliases,
  embedded movement wiring and reproducible typed test invocations.
- tools/mips/game_integration_callbacks.inc, game_integration_hooks.inc,
  game_integration_shim.c, game_shim.c: generated boundaries, HUD capture and
  original group-local padding observation.
- tools/mips/check_game.py, check_game_area.py, check_game_calls.py,
  check_game_wiring_mutations.py, check_game_module_regressions.py: current
  integration, wrapper and renderer-boundary proofs.
- tools/mips/check_bodies.py, check_bodymove.py, check_misc.py,
  check_collprobe.py, check_hud.py, monkey_harness.py, check_exclusions.csv:
  checker ABI/dependencies, stronger regressions and documented boundaries.
- notes/port/game.md, game_hooks.md, game_module_bugs.md, viewer_game.md:
  current inventory, evidence, file report and actual missing lists.

The complete wrapper execution passed 48 suites and found the HUD custom
interface/ordering-check defects described above. The final focused HUD run
exercised all 29 HUD wrappers in 580 cases and passed. After that repair,
check_game_calls.py --summarize aggregates all 49 successful actual reports,
with no failed checkers. This does not claim a second full execution. The
longest suite, monkey level 5, completed all 2,346 cases in 1,084.5 seconds;
the delay was Python scenario generation, not a native-port hang.

Changes remain uncommitted for the project owner's review. Evidence and actual
check stdout are under ignored extract/codex-task/ and extract/mips/.

## Newly merged module wiring (2026-10-15)

This update supersedes the older Task2 counts below. There are now 900 typed
wrappers (895 distinct original addresses) and 627 generated callback wires
in 30 module tables: 247 ported, 79 SDK/platform, 65 renderer, 218 missing,
9 indirect, 5 archive and 4 padding defaults.

Both generators reproduce all eight tracked generated artifacts byte for byte.
The new stage-fade and Zako AI modules are included in the viewer manifest;
CMake consumes that same manifest. No new gameplay function was ported.

Mode_Stage calls AreaFeature30Update, ColorFadeReset, StageFadeUpdate,
SoundStop and VramScrollUpdate through the central table explicitly. Their
obsolete frame-host fields were removed; native presentation/player boundaries
retain their callback types. The legacy gadget HUD callback also reaches the
HudFindGadgetIconPos port. SoundStop's RAM logic runs; native sequencer-channel
to mixer-voice mapping remains incomplete and is reported honestly by the viewer.

Seven-word hooks use ae_game_call7 for ported entries: the three extra words
are placed at call SP +0x10/+0x14/+0x18 and all prior words restored afterward.
Unported extended hooks use the optional missing_words boundary, preserving
all seven words; hosts without it retain existing name/address diagnostics.
SDK aliases SetDrawMove/exit use DR_MOVE/Exit, CdIntToPos and VSync have their
reviewed SDK addresses, ScreenBandDraw is a named renderer callback, and the
three named boss overlay updates retain their original identities. Overlay
interception in the test oracle is scoped to the correct loaded member.

CollisionProbe5/6/7 existed in the original AeMiscHooks header before this
update. All CollisionProbe0..7 and the four legacy indicator/actor fields are
unused declarations in the current misc sources. They remain explicitly
unresolved; no invented original address or game behavior was introduced.

Integration exposed one module defect: Indicator_Register returns its allocated
index, consumed at gimmick +0x14. The port now preserves that return and its
checker compares it. The unchanged old module/header and the deterministic
old-adapter zero-return behavior are both rejected. See game_module_bugs.md.
No exclusion, ledger minimum or NOT_ORIGINAL entry changed.

The wrapper run also exposed a checker-build dependency gap: the stage-fade
checker omitted the new collider ports used by ae_misc.c. It now links the
real game module closure, removes its link-only gameplay stubs, and fingerprints
headers in its object cache. Its custom stagefade_call_* exports now route
through the central wrapper proxy. The final focused misc/stagefade/zakoai
run passes all three suites: 64 wrappers, 621 adapter invocations.

Validation on the final wiring:

| Check | Result |
| --- | --- |
| Areas 01, 02, 05, 44: load + 8 frames | 36 matching comparisons, zero failures |
| Central wrapper suites | all 47 pass; 891 wrappers; 179,723 adapter invocations |
| Legacy game integration | 1,309 cases; 498 frames; 1,127 direct comparisons; 14,274 host calls |
| Special-interface helpers | 576 comparisons |
| Partial-entry boundaries / internal-entry guard | 32 comparisons / 1 guard |
| Named SDK/archive/renderer hook wiring | 149 hooks, 1,192 comparisons |
| Extended ABI | 48 comparisons, 16 checks of all three restored stack words |
| Pause/fade/transition frame branches | 12 frames and 12 loads match; fades execute the actual port |
| Wiring mutations | 41/41 caught, no build errors or survivors |
| Module regressions | 5/5 comparisons |
| Misc module checker | 297 cases; old source and zero-result adapter rejected |
| Viewer / standalone | areas 01/02, two 120-frame viewer runs each; all 2,098,176 bytes match |
| Viewer missing names | 12 in area 01, 11 in area 02; actual-run table in viewer_game.md |
| Ledger checker | zero errors, nine coverage/inventory warnings; results present for all 47 checkers |

Ported entries execute original instructions on the MIPS side; there are no
new port interceptions or masks. Full wrapper-suite counts are recorded in
extract/mips/game_calls_results.json. The initial full run found the stage-fade
checker dependency failure described above; after the focused repaired reruns,
`check_game_calls.py --summarize` confirms all 47 successful suite reports.
Aggregation reuses actual reports; it does not claim a second full execution.
Detailed logs/proofs are under ignored extract/codex-task/ and extract/mips/.
Changes remain uncommitted in this worktree for review.

### Wiring update file report

| Files | Change |
| --- | --- |
| src/game/ae_calls.c, ae_game_modules.h | Regenerated 900 wrappers and all module fields/includes. |
| src/game/ae_game_hooks.c, ae_game_platform.h | Regenerated new hooks and renderer/SDK aliases. |
| src/game/ae_calls.h, ae_game_body.c | Seven-word call API and exact restoration of the three extra words. |
| src/game/ae_game.c, ae_game.h | Extended missing callback; legacy HUD registration reaches the existing port. |
| src/game/ae_game_stage.c, ae_game_frame_host.h | Ported frame steps use the central table; remove their obsolete frame callback fields. |
| src/world/ae_misc_batch.c, ae_misc_batch.h | Preserve Indicator_Register's allocated-index return. |
| src/app/viewer_game.c | Remove the obsolete whole-SoundStop callback and correct sound diagnostics. |
| tools/codex/viewer_game_sources.txt | Add stagefade and zakoai translation units; shared by viewer and CMake. |
| tools/generate_game_hooks.py | Seven-word routes, extended missing route, named aliases/overlay addresses, output-directory creation. |
| tools/mips/check_game_area.py | Meaningful hook counts, scoped overlay callbacks, real fade branches, seven-word and restoration proofs. |
| tools/mips/check_game_module_regressions.py | Add allocated-index regressions to the existing climb cases. |
| tools/mips/check_game_wiring_mutations.py | Observable frame swaps and five new extended-argument/restoration mutants. |
| tools/mips/check_misc.py | Compare Indicator_Register's original signed return. |
| tools/mips/check_stagefade.py, check_game_calls.py | Link real module dependencies without gameplay stubs; route custom stagefade_call exports through the central proxy. |
| tools/mips/game_integration_callbacks.inc, game_integration_hooks.inc | Reproducibly generated new callbacks and seven-word invocations. |
| tools/mips/game_integration_shim.c | Extended trace forwarding and test-only call-stack observation. |
| notes/port/game.md, game_hooks.md, game_module_bugs.md, viewer_game.md | Current wiring, inventory, evidence, validation and actual viewer missing list. |

## Historical Task2 follow-up: file changes and verification

Both integration findings are resolved; changes remain uncommitted.

| File | Change |
| --- | --- |
| src/collision/ae_collprobe.h | Dedicated six-word climb callback type; other hook types unchanged. |
| src/collision/ae_collprobe.c | Forward signed collider +8 and the type-59/type-89 flag. |
| src/world/ae_misc.h | Use the same six-word type for the copied probe callback. |
| src/game/ae_calls.h | Declare ae_game_call6. |
| src/game/ae_game_body.c | Marshal and restore both stack words; forward all six through the copied misc probe adapter. |
| src/game/ae_game_hooks.c | Both generated climb wires route six words to the central wrapper. |
| tools/generate_game_hooks.py | Generate six-word central calls from callback declarations. |
| tools/mips/game_integration_hooks.inc | Regenerated six-word direct test invocations. |
| tools/mips/check_collprobe.py | Six-word recording/comparison, explicit 59/89 cases with +8=173/219, and three argument-loss mutations. |
| tools/mips/check_misc.py | Same ABI, original stack-word interception, explicit radial dispatch cases and three copied-hook argument-loss mutations. |
| tools/mips/check_game_area.py | Compute each live slot from the current table and mask only target +18:2. |
| tools/mips/check_game.py | Apply the same declared padding mask to integrated frame comparisons. |
| tools/mips/check_exclusions.csv | Owner-approved check_game_area ram_mask row with original reader evidence. |
| notes/port/game_module_bugs.md | Mark both findings resolved; record original caller/reader evidence and old-source failure proofs. |
| notes/port/game.md | Update frame results and record this file-level report. |

No change to ae_misc.c is required: its existing callback assignment preserves
the corrected type, and its original indirect caller reaches the radial entry.
No camera gameplay source or tools/check_verified.py change was made.

Final unit results: collprobe 1,304 cases, 43/43 mutations caught; misc 213
cases, 70/70 existing mutations plus three climb mutations caught. Both checkers
assert that the explicit 59/89 scenarios reached the original six-word call.
The unchanged HEAD collision source/header and unchanged HEAD misc/collision
sources/headers fail the strengthened checkers; see the ignored reproducible
proof script extract/codex-task/prove_old_task2.py and log
extract/mips/task2_old_source_proof.log. Controlled mutations also catch each
lost word without relying on undefined old host arguments.

Game: 1,309 cases, 498 frames, 1,127 direct comparisons and 14,274 host calls
match. Game-area: all four loads and all 32 frames match (36 comparisons).
Central-wrapper suites: all 45 pass, exercising 862 wrappers with 179,315
adapter invocations; the final strengthened collprobe/misc suites also pass
through the central wrappers (1,517 cases). Module regressions: both 59/89
cases pass. Wiring mutations: 36/36 caught.
Ledger checker: zero errors and three existing warnings. Detailed stdout is
under ignored extract/mips/task2_*.log. The camera reader audit and exact
addresses are in game_module_bugs.md and the exclusion reason.

The central host has 871 typed wrappers for all 871 `diffed` ledger rows
(866 distinct original addresses), and 564 generated callback wires in 28
module tables. Area loads 01, 02, 05 and boss area 44 match the original.
Areas 01, 02, 05 and 44 each complete eight matching stage frames. The radial
climb hook now preserves all six arguments. Camera target padding has the
owner-approved two-byte exclusion for every live slot. See [module findings](game_module_bugs.md).

## Design and files

`src/game/ae_calls.{c,h}` contains one readable, typed adapter per ledger row
and an address-sorted table. `tools/generate_game_calls.py` reads C declarations
and the ledger, never instructions. Adapters preserve signed arguments, narrow
fields through the actual C parameter types, and marshal out pointers as RAM
addresses. Arguments beyond a0-a3 are read at call_stack_pointer+10/+14 hex.
`ae_game_call5` and `ae_game_call6` supply extra callback words and restore
the prior stack words after the call; Gadget EffectSpawnFromDef's nonzero direction is tested.
Duplicate addresses prefer complete RAM ports over partial/native interfaces.

`ae_calls_special.c` handles interfaces that differ from the original ABI;
`ae_calls_legacy.c` isolates the historical gadget hook tag. CameraLookAt and
PlayerCheckGadgetSelect are complete-entry host boundaries because their ledger
rows cover only a native helper/decision. VectorNormal's internal tail entry
80030210 consumes t0/t1/t2, so a public a0-a3 call fails explicitly and writes
no game RAM. It must be entered through VectorNormal/VectorNormalSS. No original
register simulation is introduced to make that internal label a callable API.

An overlay address is dispatched only when the ledger identifies its member
and `g->loaded_overlay` agrees. Five unqualified overlay rows stay at the host;
member 18C's identified 8013F0C0 wrapper is eligible only with 18C loaded.
Direct unit tests can address the individual typed adapters separately; they
do not claim that an unidentified overlay is eligible for runtime dispatch.

`ae_game_hooks.c` and `ae_game_platform.h` are generated by
`tools/generate_game_hooks.py`; the full per-hook inventory is
[game_hooks.md](game_hooks.md). Generation uses declarations and established
SDK identities, including notes/inventory/functions.csv, not translated game
instructions. Address-labelled SDK aliases now use the same named callback as
the public SDK API. Hardware SDK functions are host boundaries; already ported
pure arithmetic SDK functions reach their C ports through the central table.
Unported arithmetic APIs have named platform callbacks too. There are 33
named platform APIs, renderer callbacks, five archive callbacks, twenty named
frame boundaries, and the existing sound-hardware interface.

`ae_game_body.c` supplies the older typed body and animation adapters, native
array/out-pointer copies, and the collision-ray adapter. Camera-update's copied camera callbacks unwrap their owning AeCamupdHooks.
CollisionRayTest's copied probe callbacks preserve the original user unchanged;
the game adapter does not unwrap them. Body printf uses the named Printf platform API;
probe construction, sound, pickups and camera nudge route through the call table.
CollisionUpdate's player/RC movement blocks now run the verified bodymove ports.
AeSolidHooks has only undefined-caller-value fields, initialized to zero;
BodyStub_ReturnZero's documented NULL default retains the original zero result.

Generated module callback counts (callback entries, not unique functions):

| Route | Entries |
| --- | ---: |
| Ported call table | 229 |
| Named SDK/platform | 73 |
| Named native renderer | 62 |
| Missing game logic | 182 |
| Indirect original function dispatch | 9 |
| Archive/disc host | 5 |
| Explicit padding defaults | 4 |
| Total | 564 |

These counts exclude the older ten typed body callbacks, default body stub,
three animation callbacks, the two legacy gadget callbacks and sound-hardware
callbacks. Host registration is separate from application implementation:
this task integrates the modules, not the viewer or a complete native renderer.

## Area loading and stage frame

`ae_game_load_area` asks `area_member` for bytes, copies them at 80154000,
optionally loads the host-provided code-overlay image and identity, then calls
AreaLoad (80031B60). The image supplies original data; production never executes
its instructions. Archive/disc operations inside game logic go to archive
callbacks. Missing member bytes fail explicitly rather than using stale data.

`ae_game_frame` is one Mode_Stage body, starting at 8005BFCC. The native app owns
the loop and mode epilogue. The original EXE is executed only in tools/mips.
The main ordered sequence is:

| Order | Entry | Implementation |
| ---: | --- | --- |
| 1 | PadUpdate 8001325C, frame RNG updates | ported |
| 2 | StageRequestUpdate 8005D890 when request=6 | ported; skips ordinary update body |
| 3 | PauseMenuUpdate 800821D4 | host; returns 2/3 issue requests 28/7 |
| 4 | BeginFrame 8005DF48 | host |
| 5 | MenuWindowsRun 8007D530 | host |
| 6 | MessageBoxUpdateDraw 8008DF4C | host |
| 7 | AreaFeature30Update 8009B7FC | host |
| 8 | frame counter, ColorFadeReset 8008C390 | counter ported, fade host |
| 9 | StageFadeUpdate 80079B10 | host; negative result exits frame |
| 10 | DrawTextSprites 80082288 | host, conditional debug text |
| 11 | CameraUpdate 80047BD0 | ported |
| 12 | SetColorMatrixMode 80015D58 | ported; SDK/render boundaries |
| 13 | PlayerAnimateAndDraw 80063628 / PlayerUpdate 80060780 | named host, selected by stage flag bit 0 |
| 14 | special actor callback, SpecialActorPlayerNear 8009B6C8 | indirect host/ported, conditional flags and player mode |
| 15 | RcCarUpdate 800994A8, RcCarDraw 80099AE0 | update ported; draw host; conditional active car/flag |
| 16 | ProjectileSystemsUpdate 80084AA0 | ported |
| 17 | ZakoUpdateAll 80050C34 | ported |
| 18 | MonkeyUpdateAll 80042E18 | ported |
| 19 | AreaOverlayUpdate 8005ACE8 | ported dispatch; boss update bodies host |
| 20 | DrawTerrain 80043A8C | host |
| 21 | ItemsUpdateDraw 80084598 | ported; ItemDrawModel is its own render boundary |
| 22 | IndicatorsUpdateDraw 800719F8 | ported; renderer boundary |
| 23 | DrawAreaProps 80016F34 | host |
| 24 | EffectScriptUpdate 80069BEC | ported; renderer boundary |
| 25 | EffectsUpdateAll 8006A9F8 | ported; renderer boundaries |
| 26 | HudPopupUpdate 800983E8 | ported; renderer boundary |
| 27 | ColliderScriptsUpdate 80088AB8 | ported; indirect handlers |
| 28 | GimmickUpdateAll 8005B68C | ported; indirect handlers |
| 29 | PlayerEyeBlinkUpdate 80067AF0 | ported; eye transfer SDK boundary |
| 30 | CollisionUpdate 8007247C | ported; skipped for player modes 8/13 or stage flag bit 0 |
| 31 | SoundEmitterUpdate 8008686C | ported; native sound hardware |
| 32 | StageRequestUpdate 8005D890 | ported |
| 33 | StageDrawScreenOverlay 8005E0F0, VramScrollUpdate 800691F8 | named host |
| 34 | VSync(1), elapsed tick accounting | SDK host, accounting ported |
| 35 | pause-button gates, SoundLoopsPause 8007292C, OpenPauseMenu 8007F56C, PlaySfx 800877F8 | gates/PlaySfx ported; pause UI host |
| 36 | QueueFrameFlip 8005DEE0 | host, normal presentation |

Negative fade calls WaitFrameFlip (8005DE8C), and SoundStop (80086FD0, mode 1)
unless the stage field +122 is 3, then returns zero to the native mode owner.
Request-6 presentation performs DrawSync(0), VSync(2), PutDispEnv, PutDrawEnv,
and DrawOTag in original order. Pending flips use the current draw buffer and
clear stage +136 after PutDrawEnv; otherwise display and draw buffers use stage
+134 and its XOR-1 counterpart. `transition_present` is an optional native
notification after those boundaries. The per-mode epilogue beyond 8005C4DC
belongs to the app's mode owner and is not part of this frame API.

## Validation and current result

All tests use the original instructions as the oracle, the actual repaired
module sources, and identical host models. Strict area checks compare main RAM
outside the existing declared stack/padding windows, all scratchpad, meaningful
returns and ordered host-call arguments. The frame checker intercepts actual
unported entries, including the loaded boss overlay's 80140D60 update. It does
not execute a boss routine on only one side or compare agreed faults as proofs.

| Check | Result |
| --- | --- |
| Central wrapper suites, all 45 reviewed checkers | all pass; 862 wrappers, 179,315 adapter invocations |
| Six differing native/array interfaces against original entries | 576 randomized comparisons |
| CameraLookAt and gadget-decision forwarding | 32 boundary comparisons; no complete-entry port claim |
| Internal register-only normalize entry | rejection and no-RAM-write guard passes |
| SDK/archive/render hook tables | all 140 hooks, 1,120 boundary comparisons |
| Gadget fifth-argument integration | 16 original-entry comparisons |
| Area loads 01, 02, 05, 44 | all four match |
| Eight consecutive frames per loaded area | 01: 8/8; 02: 8/8; 05: 8/8; 44: 8/8 |
| Pause/fade/transition/update gates | 12 independent frames plus their 12 area loads match |
| Executable wiring mutations | 36/36 caught; no compile failures counted |
| Original legacy integration slice | 1,309 attempts; 498 frames and 1,127 direct comparisons match; 14,274 host calls |
| Ledger/exclusion consistency | zero errors, three existing coverage warnings |

Adapter invocation counts include the unit suites' deliberate error/fail-closed
cases and nested dependencies; they are not a count of successful whole-game
frames. The six special interfaces bring callable original-entry wrapper
coverage to 868. Two partial interfaces are tested as host boundaries, and the
internal entry as unsupported: all 871 rows have an explicit tested disposition.
The full wrapper run was followed by rechecks of misc, gadget, effects and
player2 after their integration adapters changed. Existing reports are aggregated
with `--summarize`; this mode does not pretend all reports are current binaries.
`--remaining` skips only successful reports whose source fingerprint agrees;
the fingerprint is saved at proxy build time, not inferred at worker completion.

Legacy comparison rejects 65 agreed invalid-state faults and one runaway;
these contribute no completed equivalence proof. Its existing registered stack,
monkey, particle and scratchpad blind spots still apply. The strict area checker
reuses only its existing stack window, monkey +C8, and particle +1B declarations.
Task2 adds the owner-approved camera slot target +18:2 padding mask, computed
from the current slot table for every live slot, and repairs the six-word radial
climb callback. No ledger minimum changes. The padding reader audit and
old-source failure proofs are recorded in game_module_bugs.md. Zero-return unported logic and no-op rendering models prove
integration under those models, not the behavior of missing native subsystems.

The 36 mutations cover 24 pure pairwise frame-order swaps, four ordinary
wrapper argument/width errors, two special-wrapper pointer errors, four wrong
hook targets, a missing stage monkey table and a lost fifth hook argument.
Equivalent or unreached exploratory candidates were replaced before defining
the final suite; no survivor is counted as a detection.

Reproduce with `.venv/Scripts/python.exe`:

```text
tools/mips/check_game_calls.py
tools/mips/check_game_area.py --helpers-only
tools/mips/check_game_area.py --dispatch-only
tools/mips/check_game_area.py --host-hooks-only
tools/mips/check_game_area.py --extended-hooks-only
tools/mips/check_game_area.py --frame-branches-only
tools/mips/check_game_area.py --keep-going
tools/mips/check_game_module_regressions.py
tools/mips/check_game_wiring_mutations.py
tools/mips/check_game.py
tools/check_verified.py
```

The area and module-regression commands currently exit 1 on the reported
module limitations; the other commands pass. Full wrapper suites are extended
validation (the largest suite takes many minutes); ordinary area/frame checks
and focused wrapper suites remain short. Local reports/logs are ignored under
extract/mips and extract/codex-task. No game data, dependencies or commits were
added; changes are left in the working tree for the project owner.

## Missing original functions

The following 199 distinct name/address pairs account for 218 generated missing
hook entries. Twelve unresolved declarations in AeMiscHooks are unused by its
current C implementation. Identified overlay entries remain host boundaries.
An absent host reports each pair once and returns zero; indirect unknown targets
retain their actual address. This table is regenerated from the hook inventory.

| Function | Original address | Hook table(s) |
| --- | --- | --- |
| AimRequestStart | 80048398 | AeCameraHooks |
| AimReticleDrawCircle | 80085E08 | AeGadgetHooks |
| AnimDecodeFrame | 80014D70 | AePlayer2Hooks |
| AnimFramePtr | 80015240 | AeMiscHooks, AePlayerActHooks |
| Area44_Boss_SetAnim | 80140948 | AeAreaSetHooks |
| Area46_BossUpdate | 8013F1AC | AeMiscHooks |
| Area46_Boss_ClearFlag | 8013EF00 | AeAreaSetHooks |
| Area46_Boss_SetAnim | 8013EF0C | AeAreaSetHooks |
| Area53_BossUpdate | 8013D57C | AeMiscHooks |
| Area56_Boss_SetCameraObjAnim | 801406F8 | AeAreaSetHooks |
| Area57_BossUpdate | 8013E4B4 | AeMiscHooks |
| Area57_Boss_ClearFlag | 8013F398 | AeAreaSetHooks |
| Area57_Boss_SetAnim | 8013F3A4 | AeAreaSetHooks |
| AreaCheckExits | 80010D80 | AeStageHooks |
| AreaEffect | 8013D358 | AeGadgetHooks |
| AreaMonkeyQuotaMet | 80097454 | AeGimmickBHooks |
| AreaUsesAltMember | 800115F4 | AeStageHooks |
| BgmFadeOut | 8008760C | AeStageHooks |
| BodyInsideBox | 8008EC70 | AeGimmickAHooks |
| BossAimSfx | 8004898C | AeCameraHooks |
| CallIndexedHandler | 8004D880 | AeCameraHooks |
| CameraAreaScriptDispatch | 8004B590 | AeCameraHooks, AeCamupdHooks, AePlayerActHooks |
| CameraGetSlotTarget | 8004B54C | AeGadgetHooks |
| CameraRadarFocusSelect | 80047E8C | AeCameraHooks |
| CameraRadarFocusUpdate | 80047EE0 | AeCameraHooks |
| CameraRcCarFollow | 8004AA58 | AeCameraHooks |
| ColliderBoxContainsPoint | 8009091C | AeGimmickBHooks |
| ColliderSlopeContactDir | 8004F884 | AeCollProbeHooks, AeMiscHooks |
| CollisionProbe0 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe1 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe2 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe3 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe4 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe5 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe6 | unresolved (unused declaration) | AeMiscHooks |
| CollisionProbe7 | unresolved (unused declaration) | AeMiscHooks |
| DirIndexFromFlags | 8007774C | AeCollProbeHooks, AeMiscHooks |
| DrawScaledModelAt | 8007CAE0 | AeGadgetHooks |
| Draw_Nop | 8005E000 | AeZako1Hooks |
| EffectRingDraw | 8006E5B4 | AeEffectsHooks |
| FUN_80066F34 | 80066F34 | AeGimmickBHooks |
| FUN_8006AA60 | 8006AA60 | AeGimmickBHooks |
| FUN_80088d78 | 80088D78 | AeMiscHooks |
| FUN_80088f2c | 80088F2C | AeMiscHooks |
| FUN_800895f0 | 800895F0 | AeMiscHooks |
| FUN_80089b10 | 80089B10 | AeMiscHooks |
| FUN_80089d64 | 80089D64 | AeMiscHooks |
| FUN_80089d6c | 80089D6C | AeMiscHooks |
| FUN_8008aa64 | 8008AA64 | AeMiscHooks |
| FUN_80091598 | 80091598 | AeGimmickBHooks |
| FUN_80091AF0 | 80091AF0 | AeGimmickBHooks |
| FUN_80091C04 | 80091C04 | AeGimmickBHooks |
| FUN_80092324 | 80092324 | AeGimmickBHooks |
| FUN_80093848 | 80093848 | AeGimmickBHooks |
| FUN_80094330 | 80094330 | AeGimmickBHooks |
| FUN_80094A48 | 80094A48 | AeGimmickBHooks |
| FUN_80094E5C | 80094E5C | AeGimmickBHooks |
| FUN_80096638 | 80096638 | AeStageHooks |
| FUN_80097504 | 80097504 | AeGimmickBHooks |
| FUN_8009B3B0 | 8009B3B0 | AeGimmickBHooks |
| FUN_80137248 | 80137248 | AeMiscHooks |
| FUN_8013DF54 | 8013DF54 | AeZakoAiHooks |
| FUN_8013EA24 | 8013EA24 | AeStageHooks |
| FUN_8013F504 | 8013F504 | AeZakoAiHooks |
| FUN_8013F660 | 8013F660 | AeZako1Hooks |
| FUN_8013d000 | 8013D000 | AeAreaSetHooks, AeCameraHooks |
| FUN_8013d10c | 8013D10C | AeAreaSetHooks |
| FUN_8013d1f0 | 8013D1F0 | AeAreaSetHooks |
| FUN_8013d33c | 8013D33C | AeAreaSetHooks |
| FUN_8013d358 | 8013D358 | AeFxscriptHooks, AeWorldHooks |
| FUN_8013d440 | 8013D440 | AeAreaSetHooks |
| FUN_8013d47c | 8013D47C | AeAreaSetHooks |
| FUN_8013d52c | 8013D52C | AeAreaSetHooks |
| FUN_8013d804 | 8013D804 | AeAreaSetHooks |
| FUN_8013d8bc | 8013D8BC | AeAreaSetHooks |
| FUN_8013d9a8 | 8013D9A8 | AeAreaSetHooks |
| FUN_8013db98 | 8013DB98 | AeAreaSetHooks |
| FUN_8013dc00 | 8013DC00 | AeAreaSetHooks |
| FUN_8013de14 | 8013DE14 | AeAreaSetHooks |
| FUN_8013df28 | 8013DF28 | AeAreaSetHooks |
| FUN_8013e020 | 8013E020 | AeAreaSetHooks |
| FUN_8013e070 | 8013E070 | AeAreaSetHooks |
| FUN_8013e14c | 8013E14C | AeAreaSetHooks |
| FUN_8013e4a8 | 8013E4A8 | AeAreaSetHooks |
| FUN_8013e4d8 | 8013E4D8 | AeAreaSetHooks |
| FUN_8013e7f8 | 8013E7F8 | AeAreaSetHooks |
| FUN_8013e8c0 | 8013E8C0 | AeAreaSetHooks |
| FUN_8013ec5c | 8013EC5C | AeAreaSetHooks |
| FUN_8013ecf4 | 8013ECF4 | AeAreaSetHooks |
| FUN_8013ee9c | 8013EE9C | AeAreaSetHooks |
| FUN_8013f080 | 8013F080 | AeAreaSetHooks |
| FUN_8013f130 | 8013F130 | AeAreaSetHooks |
| FUN_8013f464 | 8013F464 | AeMiscHooks |
| FUN_8013f4f0 | 8013F4F0 | AeAreaSetHooks |
| FUN_8013f544 | 8013F544 | AeAreaSetHooks |
| FUN_8013f660 | 8013F660 | AeZako2Hooks |
| FUN_8013f68c | 8013F68C | AeAreaSetHooks |
| FUN_8013f86c | 8013F86C | AeAreaSetHooks |
| FUN_8013fa2c | 8013FA2C | AeMiscHooks |
| FUN_8013fc00 | 8013FC00 | AeZonesHooks |
| FUN_8013fc18 | 8013FC18 | AeMiscHooks |
| FUN_8013fd74 | 8013FD74 | AeMiscHooks |
| FUN_8013fe4c | 8013FE4C | AeMiscHooks |
| FUN_8014022c | 8014022C | AeFxscriptHooks |
| FUN_801402d8 | 801402D8 | AeMiscHooks |
| FUN_801407bc | 801407BC | AeAreaSetHooks |
| FUN_80140880 | 80140880 | AeZakoAiHooks |
| FUN_80140a60 | 80140A60 | AeMiscHooks |
| FUN_80140d60 | 80140D60 | AeMiscHooks |
| FUN_8014116C | 8014116C | AeZakoAiHooks |
| FUN_801414a0 | 801414A0 | AeGimmickHooks |
| FUN_8014263c | 8014263C | AeFxscriptHooks |
| FUN_80143854 | 80143854 | AeMiscHooks |
| FadeFrame | 8007A14C | AeStageHooks |
| GimmickTouchCheck | 80090754 | AeGimmickAHooks |
| HalfPipeRangeTest | 80050490 | AeCollProbeHooks, AeMiscHooks |
| HudDrawMonkeyInfo | 80060448 | AeCameraHooks |
| IndicatorActivate | unresolved (unused declaration) | AeMiscHooks |
| IndicatorDraw | unresolved (unused declaration) | AeMiscHooks |
| IndicatorRegister | unresolved (unused declaration) | AeMiscHooks |
| Indicator_Activate | 800717B4 | AeGimmickBHooks |
| ItemSaveCollectedMask | 800847EC | AeStageHooks |
| ItemsClearFlags | 800847C4 | AeStageHooks |
| LevelMonkeyCountGet | 800965EC | AeStageHooks |
| MatrixFromFrontUp | 80014A84 | AePlayer2Hooks |
| ModelEmitPrims | 800180B8 | AePlayer1Hooks, AeZako1Hooks |
| MonkeyFlag248Set | 8009A17C | AeCameraHooks |
| ObjAccelerate | 80056DC4 | AeZakoAiHooks |
| ObjAddHitBodyIfPlayerNear | 80058220 | AeZakoAiHooks |
| ObjClampFrame | 800570D4 | AeZakoAiHooks |
| ObjCooldown | 8005588C | AeZakoAiHooks |
| ObjFallAndBounce | 800572CC | AeZakoAiHooks |
| ObjFallToGround | 8005648C | AeZakoAiHooks |
| ObjFleeFromPlayerInArea | 80056128 | AeZakoAiHooks |
| ObjGetLinkedObjState | 800582EC | AeZakoAiHooks |
| ObjSetSpeed96 | 8005325C | AeZakoAiHooks |
| ObjSetVelFromFacing | 80056200 | AeZakoAiHooks |
| ObjSpawnParticleAt | 80057504 | AeZakoAiHooks |
| ObjTryStepOnGround | 80056344 | AeZakoAiHooks |
| ObjTurnToPlayer | 8005696C | AeZakoAiHooks |
| ObjTurnTowardsPlayerXZ | 80053154 | AeZakoAiHooks |
| ObjWalkOnGround | 80055460 | AeZakoAiHooks |
| PadDemoPlaybackStep | 80013128 | AePadHooks |
| PadSetupDualShock | 80012CA0 | AePadHooks |
| PlayTimeAdd | 8005DD44 | AeStageHooks |
| PlayerAttachSet | 80066EEC | AeGimmickBHooks |
| PlayerCanActivateGadget | 8007CA48 | AeFxscriptHooks, AeGadgetHooks, AePlayer2Hooks |
| PlayerEnterCutsceneMode | 80095588 | AeStageHooks |
| PlayerFxJumpBurst | 8006C58C | AePlayerCondHooks |
| PlayerFxSetMatrixPos | 8006CFBC | AeGadgetHooks |
| PlayerInit | 8003272C | AeStageHooks |
| PlayerInput_StickGesture | 800650D8 | AePlayerActHooks |
| PlayerStopGadgetSounds | 8007A180 | AePlayer2Hooks |
| PointInBoxXZ | 80073B64 | AeCollProbeHooks, AeMiscHooks |
| ProgressFlagSet | 800962F4 | AeGimmickAHooks |
| ProgressSetByte | 80096360 | AeGimmickAHooks, AeStageHooks |
| RadarDrawModel | 8009A574 | AeGadgetHooks |
| RadarUsable | 800468EC | AeCameraHooks |
| ScreenFadeSet | 80046978 | AeCameraHooks |
| SeqTrackClose | 800A1F3C | AeStageFadeHooks |
| SeqTrackSetPlayState | 800A2B6C | AeStageFadeHooks |
| SlideDoorMove | 8008FAD0 | AeGimmickAHooks |
| SoundBankFree | 800A1CCC | AeStageFadeHooks |
| SoundEmitterPan | 80087B34 | AeGimmickAHooks, AeGimmickBHooks |
| SoundEmitterVolume | 80087BA4 | AeGadgetHooks |
| SoundLoopStop | 8008806C | AeFxscriptHooks, AeGadgetHooks, AeGimmickAHooks, AeGimmickBHooks, AeMiscHooks |
| SoundSilence | 80086148 | AeStageHooks |
| SoundStopAllVoices | 800A342C | AeStageFadeHooks |
| SpawnEffectGroupAt | 800698E4 | AeGimmickBHooks |
| SpecialActorDraw | unresolved (unused declaration) | AeMiscHooks |
| SpecialActorPlaceAtCollider | 8009B578 | AeGimmickBHooks |
| StageExitStart | 8004D73C | AeStageHooks |
| StageGoalReached | 80097A60 | AePlayerActHooks |
| StageGradeUpdate | 80097BFC | AeStageHooks |
| StageIntroStart | 8009546C | AeStageHooks |
| StageIntroUpdate | 800954C4 | AeStageHooks |
| StageTypeClearInit | 80094F48 | AeStageHooks |
| StageTypeClearUpdate | 80095030 | AeStageHooks |
| StageVictoryStart | 8009543C | AeStageHooks |
| StepToward | 800463A4 | AeFxscriptHooks |
| StickToButtons | 800140B4 | AePadHooks |
| SwingDoorBodyDelta | 8008F308 | AeGimmickAHooks |
| TargetListFindBest | 8007CDFC | AeGadgetHooks |
| TrailRingDraw | 8001F954 | AeGadgetHooks |
| TrailRingPush | 8001F8B0 | AeGadgetHooks |
| TurnTowardsY | 80066C90 | AeFxscriptHooks, AePlayer2Hooks |
| WaitFrameFlip | 8005DE8C | AeStageHooks |
| ZakoChasePlayer | 80052FE8 | AeZakoAiHooks |
| ZakoEnterDeath | 800530D0 | AeZakoAiHooks |
| ZakoEnterIdleWait | 80052ED4 | AeZakoAiHooks |
| ZakoReturnIfFar | 800534E8 | AeZakoAiHooks |
| ZakoSaveDefeatedMask | 800509CC | AeStageHooks |
| ZakoStateHurt | 80052DFC | AeZakoAiHooks |
| ZakoStateWatch | 80053264 | AeZakoAiHooks |
| ZakoWalkToTarget | 80052D70 | AeZakoAiHooks |
| Zako_InitDefaultStats | 8005337C | AeZakoAiHooks |
| Zako_Nop | 80058134 | AeZakoAiHooks |
| Zako_RecoverFromStun | 80053440 | AeZakoAiHooks |
| Zako_WanderBounce | 80053304 | AeZakoAiHooks |

The fifteen remaining frame-host entries are additional named app boundaries
when no host implementation is supplied. BodyMove's player/RC blocks use
8007247C as their containing-function address; PlayerEventCallback dispatches
the actual runtime pointer from the event tables (800B027C / 800B03AC). Partial
CameraLookAt (80046670) and PlayerCheckGadgetSelect (80067974), unknown original
addresses and overlay-identity guard failures are additional dispatch boundaries.
SDK, renderer and archive callbacks are listed in game_hooks.md and are not
missing game logic.
