# Stage fade, resident TIMs and sound stop

The task's eight entry points span two modules. The existing camera port owns
the public `ae_ScreenFlashDraw` symbol at 80046990; `src/world/ae_stagefade.{h,c}`
owns the other seven entries. Stagefade's screen-flash path uses the private
`stagefade_render_screen_flash` helper to reach the same `RenderScreenFlash`
callback, so the host has one public definition. Transition, timer, HUD,
sound-table, and resident-record logic is hand-ported against `AeRam`. Audio
hardware, CD/XA, and asset decoding also remain named callbacks. No game-host
wiring was changed in this worktree.

## Instruction call tree

The edges below follow the MIPS `jal` sites in the original EXE. Branches are
the paths in `StageFadeUpdate`; the renderer and native-system leaves are
callbacks rather than simulated hardware.

```text
StageFadeUpdate
  state 1 (fade out for exit/reload)
    ScreenFlashFadeDraw
      shared ScreenFlashDraw -> RenderScreenFlash (native renderer)
    IsTimeAttack
    StageRequest (ported; actions 24/25 or 3)
    [exit kind 13] HudPanelB_Start (ported), HudPanelA_Start (this batch),
                    HudShowHealth (ported)
  state 2 (flash in)
    ScreenFlashFadeDraw -> stagefade_render_screen_flash -> RenderScreenFlash
    FUN_80028850, FUN_800286CC (unresolved context hooks)
  state 3 (fade then reload)
    ScreenFlashFadeDraw -> stagefade_render_screen_flash -> RenderScreenFlash
    SoundStop(mode 2) -> BgmSeqSetVolume (ported), SeqTrackSetPlayState;
                         mode 2 returns before track close/loop reset
    FUN_80028850, FUN_800286CC
    CdMgrUpdate, VSync
    AreaLoadQueueRead (ported) -> archive/CD callbacks
    LoadListStep, CdMgrUpdate, VSync
    StageRequest(action 13; ported) -> area setup and load callbacks
  state 4 (spiral fade out)
    FadeSpiralDraw -> RenderFadeSpiral (native renderer)
    StageRequest(action 3; ported)

ScreenFlashFadeDraw -> stagefade_render_screen_flash
FadeSpiralDraw -> RenderFadeSpiral
SoundStop -> BgmSeqSetVolume, SeqTrackSetPlayState, SeqTrackClose,
             SoundLoopsReset, SoundStopAllVoices, VSync, SoundBankFree
TimListResidentUpdate -> DebugTextPrintf (aging/debug path)
HudPanelA_Start -> RAM writes only
```

`StageRequest`, `AreaLoadQueueRead`, `BgmSeqSetVolume`, `HudPanelB_Start`, and
`HudShowHealth` are existing ports and are called directly. The transition's
`StageRequest` calls keep their existing `AeStageHooks` boundary for the
remaining area, save-progress, CD and subsystem functions.

## Port details and instruction findings

* `StageFadeUpdate` preserves the four active fade states and the original
  16-bit timer writes. State 1 checks the exit kind before drawing, then reads
  it again after the timer reaches zero. A renderer callback can mutate that
  byte, so the post-fade decision uses the second read just as the MIPS code
  does.
* `ScreenFlashFadeDraw` computes signed `timer * 255 / 48` with C's
  truncation-toward-zero division, then calls its private renderer helper. The
  divisor is 48 in the instructions (not 24). The camera port remains the
  single owner of public `ae_ScreenFlashDraw`.
* `FadeSpiralDraw` clamps the signed timer to 24 when it is at least 25, stores
  the clamp in RAM, then passes the resulting timer to the native renderer.
  The original setup, SDK trig calls, and epilogue run in the checker. The
  renderer replaces only packet and ordering-table link writes; the original
  epilogue still stores the cursor advanced by 32 24-byte packets.
* `SoundStop` performs the bank and voice state changes in instruction order.
  Mode 3 waits/closes the BGM track and clears the two BGM slots, then returns;
  it does not reset loop records, stop all voices, wait for VSync, or free the
  other banks. Modes 0 and 1 differ in which sound-bank slots they clear/free.
* `SoundLoopsReset` resets the five fields the original touches in each of 24
  loop records; it leaves other record bytes intact.
* `TimListResidentUpdate` has three paths: `slot == -1` clears eight resident
  pointers/count and returns `0x800B0000`; mode zero ages resident entries and
  formats debug output below age 80; other modes append a pointer, ID and age.
  The count is an unsigned byte, so `8008D1C4` cannot execute (incremented count
  is at most 256). The checker registers this proof.
* `HudPanelA_Start` writes the mode-dependent timer/state and ORs the visibility
  bit into the existing flags word.

## Native and unresolved hooks

These original functions stay hooks as required by the native port boundary:

| Hook | Responsibility |
|---|---|
| `RenderScreenFlash` | Native renderer builds/submits the screen-flash GPU packet and ordering-table entry. Replaces `ScreenFlashDraw`'s hardware work; receives the computed shade. |
| `RenderFadeSpiral` | Native renderer builds/submits the spiral-fade GPU packet and ordering-table work; receives the clamped timer and advances the OT cursor by 32 24-byte records. |
| `ArcLoadMember` | Resolves/loads an archive member for the asset loader. |
| `CdMgrFlush`, `CdMgrStopStream`, `CdMgrUpdate` | Flush pending CD work, stop streaming, and advance the CD manager. |
| `LoadListPostprocess`, `LoadListStep` | Process a completed load-list item and advance the load-list operation. |
| `LoadTimList` | Loads/processes a TIM list for the native VRAM path. |
| `TimRleDecode1`, `TimRleDecode2` | Decode the two TIM run-length encoded formats. |
| `XaMgrUpdate` | Advances XA audio streaming. |
| `SeqTrackClose`, `SeqTrackSetPlayState`, `SeqTrackKeyOffMaskSet` | Close a sequence track, change its play state, and update its key-off mask. |
| `SoundBankFree`, `SoundBankLoadHeader`, `SoundBankSetVolume`, `SoundBankUpload` | Free a sound bank, load its header, set its bank volume, and upload bank data to SPU memory. |
| `SoundStopAllVoices` | Stops active SPU voices through the sound host. |
| `SpuSetReverbPreset`, `SpuSetReverbVolume` | Configure SPU reverb registers. |
| `SpuUploadBusy`, `SpuUploadBusyWrap`, `SpuUploadStart`, `SpuWaitLoop`, `SpuWaitIdle` | Query/start SPU DMA uploads and wait for upload/idle completion. |
| `VSync` | Waits for the requested video-sync boundary. |
| `DebugTextPrintf` | Emits the resident-TIM diagnostic with its original arguments. |
| `FUN_80028850`, `FUN_800286CC` | Unresolved context-copy/setup callees; original symbols are not identified, so the port preserves them as named callbacks. |

These callbacks are exposed through `AeStageFadeHooks`, `AeStageHooks`, or the
existing `AeSoundHw` interface. The host implementation can replace them with
the archive, renderer, audio, and platform systems without adding hardware
simulation to the port.

## Differential validation

`tools/mips/check_stagefade.py` compares the C ports against the original EXE
using five real area members (areas 1, 2, 5, 24 and 53). It compares return
values, hook calls/arguments, all main RAM outside the registered stack window,
and all scratchpad. For `StageFadeUpdate`, the harness seeds the final fade
state and clears unrelated monkey-spawn and BGM-slot state so the area path
reaches the requested transition without entering another batch's callbacks;
the five original area images remain the test inputs. It separately exercises
the sound-stop modes and callback mutation of the exit-kind byte.

Default run (`--cases 32 --seed 94423`, `--coverage`):

| Function | Cases | Instructions covered | Gap |
|---|---:|---:|---|
| `StageFadeUpdate` | 91 | 218/218 | None. |
| `ScreenFlashFadeDraw` | 32 | 21/21 | None. |
| `FadeSpiralDraw` | 32 | 100/160 | 80079F90 and 8007A000..8007A004 are unreachable from the timer clamp and loop-coordinate conditions; 8007A02C..8007A10C is renderer-owned packet/ordering-table work. |
| `SoundStop` | 32 | 163/163 | None. |
| `SoundLoopsReset` | 32 | 15/15 | None. |
| `TimListResidentUpdate` | 32 | 98/99 | `8008D1C4` is proven unreachable for an unsigned-byte count. |
| `HudPanelA_Start` | 32 | 22/22 | None. |

The checker completes 283 differential cases (315,272 MIPS steps) in about
3.2 seconds on this worktree. Its extra observation of the already-ported
`BgmSeqSetVolume` covers 88/105 instructions; uncovered voice-specific arms
are outside this batch and this function's separate checker owns them. The
extra coverage is not a ninth ledger function.

The mutation suite has 24 deliberate variants across the seven stagefade
entries, including callback and shade checks through `ScreenFlashFadeDraw`.
The documented `ScreenFlashFadeDraw` mutation `(timer * 255 + 1) / 48` was dropped
as equivalent: `timer * 255` is divisible by three and the possible remainder
modulo 48 cannot be 47, so adding one cannot advance the quotient. An
observable scale mutation replaces it. The checker registers the stack
window, renderer-owned instruction ranges, the one unreachable TIM instruction,
and that equivalent mutation in `tools/mips/check_exclusions.csv`.

## Port status

The seven stagefade game-logic functions are ported and `diffed`, with
per-function default case counts and minimums in `tools/verified.csv`. The
checker reaches the original `ScreenFlashDraw` call as a renderer hook while
checking `ScreenFlashFadeDraw`; the existing camera checker owns the standalone
public function. The new checker is discovered by
`tools/run_checks.py`. Host wiring for `AeStageFadeHooks` is left to the project
owner because editing `src/game/` is outside this task's scope.
