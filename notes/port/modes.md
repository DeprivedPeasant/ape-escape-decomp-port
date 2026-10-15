# Resident modes (2026-10-15)

Wiring pass 7 fills the merged small-mode and Stage Start hook tables while
retaining the generated member/address entry guards. The original-main
checker now proves 665 frame boundaries through title selection 0 and member
188, stopping at unported AreaKindVisitedSet (`800964DC`). Title selection 6
also reaches mode 35 / member 186 and reports the unmerged Mode_SaveMenu
entry (`80136AB8`). No new resident or overlay game function was ported.
See game.md for native fixture assumptions and game_module_bugs.md for the
member-188 fixes and the caller-padding input awaiting owner review.

`src/game/ae_modes.c` contains handwritten, structured RAM-image ports. The
instruction evidence is SCUS_944.23 at the addresses below, read with
`tools/mips/mips_ram.py`; no decompiler exports or instruction-to-C generators
were used. `tools/mips/check_modes.py` loads the real EXE and area-01 member,
then varies preexisting RAM, scratchpad, branch state and boundary results.

| Original | Address | Cases | Executed instructions |
| --- | --- | ---: | ---: |
| main | 8005BDC0 | 80 | 73/73 |
| Mode_04 | 8005BEE4 | 32 | 2/2 |
| Mode_Stage | 8005BF70 | 290 | 647/647 |
| Mode_06 | 8008D1F0 | 80 | 197/199 |
| StageInitMemory | 80010B50 | 64 | 59/59 |
| StageRenderInit | 8005BEF0 | 32 | 32/32 |
| BumpAllocPeek | 8008C000 | 160 | 7/7 |
| NullInit | 800143B0 | 32 | 2/2 |
| ResetFrameFlip | 8005DDE0 | 32 | 5/5 |

The suite has 802 top-level comparisons and executes all reachable
instructions: 1,024/1,026 overall. Mode_06's two dead instructions at
8008D3D8..8008D3DC follow a test where selection is nonnegative, at least 2,
below 4, and unequal to 3: it must equal 2, so that fallthrough cannot run.
No reachable coverage gap remains.

Comparisons include every defined return, every main-RAM byte outside the
call stack, every scratchpad byte, and the ordered named calls with their
meaningful arguments. ClearImage requests also compare the eight-byte local
rectangle. NullInit and ResetFrameFlip have void interfaces because their
instructions do not define a return value. BumpAllocPeek keeps low-word
overflow when aligning size, including INT_MAX and UINT_MAX cases.

Ten independently compiled mutations are rejected: NullInit side effect,
allocation rounding, monkey clear count, render-buffer reset, Mode_04 return,
main's mode-13 area choice, Mode_06's launch mode, stage RNG, stage exit result,
and flip flags. Compilation failures do not count as mutation detections.
Default runs write the ledger report; mutation runs leave that report intact.

Two new exclusion rows need the owner's review: the normal original/native
call-stack difference and the proved dead Mode_06 fallthrough. No gameplay
mask, shared uninitialized input, reduced minimum, or weakened ledger status
was introduced.

## Native scheduling and boundaries

`ae_Mode_Stage` retains the original loop. Its static `stage_frame_body`
contains the original per-frame order and fade-return condition;
`stage_exit` contains the original post-loop switch. `ae_mode_stage_frame`
executes that body once and runs the exit on its fade return. `ae_game_frame`
calls this shared scheduler, replacing its former partial reimplementation.

The full function begins with StageRenderInit, StageRequest(1, current area,
0), and ResetFrameFlip. Native area loading supplies the real member/overlay
through file-I/O callbacks and follows the same three-step setup; its debug
entry parameter is supplied to StageRequest. The integration oracle now runs
these original functions for loads and continues through the complete stage
exit for fade frames, rather than stopping before the exit switch.

The native `--game` bootstrap supplies player tables/model/parameters because
PlayerInit remains outside this task. After proving the original stage entry,
it calls the verified PlayerSetMode(0) twice: the first leaves startup mode
12; the second applies normal mode 0's entry reset. The integration fixture
executes and compares both original calls. This is an explicit debug starting
condition; it does not claim a completed title/startup presentation sequence.
Runtime player/monkey/contact/capture logic still executes its real ports.

AeModeHooks uses the existing typed name/argument dispatch convention.
Generated central adapters expose all nine original entries. Native mode
calls preserve names, addresses and argument counts; gameplay calls route
through the central table and its loaded-overlay guard. Absent native mode
boundaries set the RAM error flag. Remaining unported gameplay calls retain
their names through the missing callback.

Native boundaries include InitSystem, SetVideoMode, InitDisplay, ResetGraph,
ClearImage, DrawSync, VSync, display/ordering-table submission, BeginFrame,
terrain/prop drawing, frame-flip queue/wait, archive/CD loading, and
RunMinigameExe. Mode_06's DebugMenuUpdate (8008D50C), title-sequence entry
(80136074) and Galaxy entry (80137654) remain named calls for their owning
modules. The host's viewer display callbacks notify its native presentation;
PS1 packet rendering and native audio voice mapping remain incomplete.

## Integration findings

The former host bypassed StageRequest during loading and stopped at the fade
return before 8005C4DC..8005C968. The expanded area oracle independently
rejects both old behaviors in check_game_module_regressions. No external
gameplay module needed a new bug fix in this batch.

The original setup changes the scripted initial player state/position.
Areas 01, 02, 05 and 44 each match all 90 frames and visit states
00/05/08/09/24/25/80/81/83; the prior water/2D observations came from the
former setup fixture. All loads, four bootstrap comparisons per area and
the integrated net encounter pass: 495 comparisons, zero failures. Catch
occurs at swing frame 11, capture mode 8 runs and returns, and live activity
3 is saved as activity 2. No contact or caught-state injection is used.

The wiring checker mutates the actual shared mode body now and catches all
57 variants. The viewer source manifest also includes the already-diffed
18B module required by regenerated central wrappers; no overlay was ported
in this task. The manifest includes its existing native_sdk_apply.c leaf.
The wrapper runner also removes repeated C inputs from legacy checker build
commands: check_ovl18b previously supplied that leaf twice when combining
the game dependency closure and explicit maths inputs. This is a build fix;
no overlay logic or comparison was replaced.

The main boot call at 8002EDF0 is SetVideoMode(0), as named in symbols.csv and
the function inventory. Its native boundary and checker use that name.

Full validation logs live under `extract/codex-task/`. The initial sequential
manifest is `modes-validation.json`; `modes-final-validation.json` records
the final mode mutations, game/area/branch checks, native net/viewer runs,
wrapper aggregation and ledger check. The full wrapper run passed every
suite except the duplicate-input build above; a focused rerun of modes and
18B passed after repair, and aggregation includes only passing reports.

| Check | Final result |
| --- | --- |
| check_modes | 802 cases; 1,024/1,026 instructions, both gaps proved dead; 10/10 mutations |
| check_game | 1,293 cases; 474 frames; 1,109 direct comparisons; 15,356 host calls |
| check_game_area | 495 comparisons, including four loads, 360 scripted frames and net capture |
| Frame branches | 13 branch profiles pass, including the complete original fade exit |
| check_game_calls | All 58 suites pass after the focused repair; 1,090 wrappers; 187,053 invocations |
| check_game_wiring_mutations | 57/57 caught; no build errors or survivors |
| check_game_module_regressions | 131 positive comparisons; two old stage, nine old overlay and one old flash sources rejected |
| check_playerupdate | 2,700 enclosing cases plus 436 TurnTowardsY cases |
| check_net_catch | caught=1, capture=1, removed=1, saved=2, mode=0; no RAM error |
| check_viewer_game | Two 120-frame repeats and eight neutral frames in each area; all 2,098,176 bytes match |
| check_verified | 58 saved reports; 0 errors, 52 inventory/coverage warnings |
| Generator reproduction | Seven artifacts reproduce byte for byte |

Changed files (23):

- Ports and native host: src/game/ae_modes.c, ae_modes.h, ae_game_stage.c,
  ae_game.h; src/app/viewer_game.c.
- Generated adapters and tables: src/game/ae_calls.c, ae_game_hooks.c,
  ae_game_modules.h, ae_game_platform.h; tools/mips/game_integration_callbacks.inc.
- Generators and build closure: tools/generate_game_hooks.py;
  tools/codex/viewer_game_sources.txt.
- Validation: tools/mips/check_modes.py, check_game_area.py,
  check_game_calls.py, check_game_module_regressions.py,
  check_game_wiring_mutations.py, check_exclusions.csv; tools/verified.csv.
- Documentation: notes/port/modes.md, game.md, game_hooks.md, viewer_game.md.

Changes are left uncommitted for the project owner's review.


## Game-host mode entry, wiring pass 6

`ae_game_run` enters the existing `ae_main` port with the fully wired mode
hooks. main reads the real mode table at 800AFA30, invokes the native
ArcLoadMember callback, waits for the archive, and calls the port selected
by member and address. The archive callback copies the member to its
requested AeRam destination and reports failure through ae_ram_fail.
Successful loads at 80136000 update AeGame.loaded_overlay. ModeEntry checks
all entries at a shared address; an unavailable member/entry is reported
through missing and stops the dispatcher. A host may supply mode_continue
to end a session at a mode boundary.

AeOverlayHooks, AeOvl180Hooks, AeOvl187Hooks and AeOvl18bHooks now bind their
named calls to the common mode adapter. EXE/overlay ports use the central
table, native APIs use generated platform/archive/renderer callbacks, and
unported game calls retain missing. Renderer arrays keep all their words;
the ordinary native adapter preserves up to seven original words.
Overlay-local native leaves carry the same member guards as C ports.
Adding another merged mode requires its normal public declarations and
ledger member identity, its viewer source entry, and generator runs;
new native API identities belong in the generator's declaration tables.

check_game_modes runs the original main, boot, title and sequence-player
instructions against the actual complete host. Both sides begin with the
same real EXE RAM. Scripted native archive callbacks load real members 181
then 180, supply a small title command stream and empty scene, and preserve
the original mode table and boot-selected sequence 2. Native VSync scripts
pad input. All 574 BeginFrame boundaries hash every non-stack RAM byte and
all scratchpad bytes; final RAM and the full normalized call sequence also
match. The run compares 4,861 callbacks, 822 VSync calls and 1,247,124
original instructions. Only the existing check_game stack window is omitted.

The original oracle resumes with member-specific hook sets after each
archive load and clears cached instruction decodes. Its register state
exists only inside tools/mips, never in the production host. The native
fixture stops after title returns to mode 3, at main's next CdMgrUpdate;
no ported function entry is intercepted or replaced. This validates the
mode flow and wiring, with scripted archive/display services; it does not
claim a finished native CD sequence renderer. The debug viewer retains
its direct stage entry and links all these mode sources.

The member-identity mutation is rejected. The checker additionally rejects
unloaded mode entries/native leaves and checks the native five-word ABI.
See game_module_bugs.md for the independently proved member-187
ApplyMatrixLV fix discovered during the wiring audit.


The bounded main run reaches five named unported helpers: PadSetupDualShock
(80012CA0), SeqTrackSetPlayState (800A2B6C), SeqTrackClose (800A1F3C),
SoundStopAllVoices (800A342C), and SoundBankFree (800A1CCC). The deterministic
fixture supplies their boundary results; their implementations are not claimed
as ported. Title sequence 0 selects mode 8, whose original table entry is
member 188 / 8013683C. The check separately proves that missing-entry stop;
it automatically retires that assertion when member 188's ledger entry merges.
The integrated sequence-2 run itself ends at mode 3, before intro execution.


ClearImage/ClearImage2 call comparisons also include all eight rectangle
bytes, even when the pointer lies in the excluded stack window. This exposed
two repaired member-181 defects: the final boot clear is 640x512, and Mode_02
copies the embedded rectangle at 80136918 (384x512 in the real data). The
updated boot suite varies that template in three existing cases. The main
checker rejects both former rectangle sources, in addition to the lost-member
mutation. See game_module_bugs.md for instruction addresses and details.
