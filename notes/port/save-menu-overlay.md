# Save menu overlay member 186, range C

## Instruction call tree

This batch covers member addresses `8013CD1C..80141EBB`. The entry points in
this range fall into these connected groups:

- `CardMgr_StateMachine` dispatches the current UI state. It calls the state
  callbacks, card manager transitions, menu input, fade helpers, and draw
  routines. `CardMgr_SetState` owns the shared state updates and card-list
  preparation.
- `CardMgr_OnCardResult_0..9` dispatch on the active card operation at
  `800E3880`. They call `McxCardTypeSync`, `McxAllInfoSync`,
  `McxHideTransSync`, `CardMgr_ResetSlots`, and `CardMgr_SetState`; the
  low-level `MemCardOp` entry at `80010000` is an external card-system hook.
- `CardMgr_MenuUpdate` polls `PadGetState` and updates the save-menu state.
  The animation tick updates four overlay animation records; the drawing
  entry points consume those values.
- The menu/text helpers call earlier member functions `SaveUi_DrawText`,
  `SaveUi_DrawWindow`, `SaveUi_DrawIcon`, `SaveUi_DrawMainWindow`,
  `SaveUi_DrawBackground3D`, `SaveUi_YesNoUpdate`, `SaveUi_ConfirmPressed`,
  and `SaveUi_FadeTick`. These remain named overlay hooks in this batch.
- The MCX object callbacks update the thread/callback list and flags. PsyQ
  thread APIs and the MCX helpers beyond this range remain named hooks.

Memory-card SDK calls, BIOS thread APIs, pad reads, text/window/model drawing,
and inline GPU packet construction are native boundaries. The C port retains
the surrounding state, timers, input decisions, selection and text placement.
Renderer hooks use the member function name and the logical values available
at the boundary; they do not expose emulated GPU packets.

## Validation status

**Complete.** `tools/mips/check_ovl186c.py --coverage` passes the default 593 differential scenarios in about 5.6 seconds. It loads `SCUS_944.23`, each real area member in `{001,002,005}.bin`, and `186.bin` at `0x80136000`; every MIPS call has an 180,000-instruction limit. The checker compiles the C port with GCC warnings as errors. Its RAM comparison covers all memory except the declared stack window and 12-byte call-work region.

`--mutate` catches 96/96 marker-write mutations: two in each of the 48 C functions. `.venv/Scripts/python.exe tools/check_verified.py` passes; the 48 C functions below each have one `diffed` row in `tools/verified.csv` with the default per-function case count.

### Ported functions

| Function | Status | Default cases | MIPS instruction coverage |
|---|---:|---:|---:|
| `CardMgr_SetState` | diffed | 48 | 283/288; 5 proven unreachable |
| `CardMgr_StartOps` | diffed | 1 | 23/23 |
| `SsGetMute` | diffed | 1 | 8/8 |
| `CardMgr_IsBusy` | diffed | 1 | 8/8 |
| `CardMgr_StateMachine` | diffed | 88 | 276/276 |
| `CardMgr_OnCardResult_0` | diffed | 7 | 62/62 |
| `CardMgr_OnCardResult_1` | diffed | 6 | 53/53 |
| `CardMgr_OnCardResult_2` | diffed | 3 | 30/31; 1 proven unreachable |
| `CardMgr_OnCardResult_3` | diffed | 6 | 51/51 |
| `CardMgr_OnCardResult_4` | diffed | 5 | 42/42 |
| `CardMgr_OnCardResult_5` | diffed | 5 | 34/34 |
| `CardMgr_OnCardResult_6` | diffed | 20 | 91/94; 3 proven unreachable |
| `CardMgr_OnCardResult_7` | diffed | 5 | 49/49 |
| `CardMgr_OnCardResult_8` | diffed | 5 | 35/35 |
| `CardMgr_OnCardResult_9` | diffed | 21 | 86/86 |
| `McxCardTypeSync` | diffed | 3 | 31/31 |
| `McxAllInfoSync` | diffed | 4 | 29/29 |
| `CardMgr_ResetSlots` | diffed | 4 | 41/41 |
| `McxHideTransSync` | diffed | 13 | 25/25 |
| `CardMgr_Draw` | diffed | 5 | 31/31 |
| `CardMgr_MenuUpdate` | diffed | 25 | 75/75 |
| `CardMgr_AnimTick` | diffed | 8 | 47/47 |
| `CardMgr_RunMessage` | diffed | 9 | 118/118 |
| `CardMgr_DrawMessage` | diffed | 18 | 88/124; 36 GPU packet instructions routed to renderer hooks |
| `CardMgr_DrawMenuLabels` | diffed | 5 | 60/60 |
| `CardMgr_DrawBlinkingMenu` | diffed | 14 | 60/60 |
| `SaveUi_DrawStateOverlay` | diffed | 146 | 142/142 |
| `SaveUi_GetColorBuffer` | diffed | 1 | 3/3 |
| `CardMgr_DrawMenuItems` | diffed | 5 | 70/70 |
| `SaveUi_GetBuffer2` | diffed | 1 | 3/3 |
| `CardMgr_DrawMenuTexts` | diffed | 8 | 37/37 |
| `CardMgr_DrawMenuItems2` | diffed | 8 | 47/47 |
| `SaveUi_DrawStringPair` | diffed | 1 | 23/23 |
| `SaveUi_DrawYesNo` | diffed | 4 | 34/34 |
| `SaveUi_DrawFileHeader` | diffed | 4 | 32/32 |
| `SaveUi_DrawEmptySlotText` | diffed | 4 | 30/30 |
| `SaveUi_ThreadSetHook` | diffed | 4 | 41/41 |
| `SaveUi_ThreadClose` | diffed | 4 | 15/15 |
| `SaveUi_Nop` | diffed | 4 | 4/4 |
| `SaveUi_ThreadSwitch` | diffed | 4 | 13/13 |
| `SaveUi_ThreadSetHookReset` | diffed | 4 | 12/12 |
| `McxStartCom` | diffed | 4 | 28/28 |
| `MCXINIT_OBJ_B0` | diffed | 4 | 19/19 |
| `MCXINIT_OBJ_FC` | diffed | 1 | 2/2 |
| `MCXINIT_OBJ_104` | diffed | 16 | 16/16 |
| `MCXINIT_OBJ_144` | diffed | 16 | 16/16 |
| `MCXINIT_OBJ_184` | diffed | 16 | 78/78 |
| `MCXINIT_OBJ_2BC` | diffed | 4 | 24/24 |

### Native-only functions and packet ranges

These originals only build GPU packets or perform platform orchestration, so they have no C ledger row:

| Original | Native work kept behind a named boundary |
|---|---|
| `CardMgr_DrawSelectionBars` (`8013E438..8013E600`) | Builds selection-bar packets. The renderer receives animation pointers and the selected item; execution then continues into the ported `CardMgr_AnimTick` state updates. |
| `CardMgr_DrawFileList` (`8013E604..8013EB10`) | Builds file-row packets from slot, file, and card state. |
| `CardMgr_DrawIconAnim` (`8013EBD0..8013ED08`) | Builds animated icon packets from the icon-frame table and UI state. |
| `CardMgr_DrawProgressBar` (`8013ED0C..8013EED4`) | Waits for VSync and builds progress-bar packets. The hook preserves the VSync call and its order. |
| `CardMgr_DrawMessageLines` (`8013F9F8..8013FB04`) | Builds the message text background packets. |
| `CardMgr_DrawMessage` inline block (`8013F848..8013F8D4`) | Builds the message frame packets; the C function sends choice, file state, UI state, and line count through `CardMgr_DrawMessageFrame` and `CardMgr_DrawMessageLines` renderer hooks. The remaining 88 instructions of `CardMgr_DrawMessage` are covered as game logic. |
| `McxStopCom` (`80141C00..80141C3F`) | Its body queries the current thread, calls `McxComInit`, creates the MCX thread with priority/argument `2`, and exits the critical section. Those operations stay in platform hooks. |
| `MCXINIT_OBJ_31C` (`80141EAC..80141EBB`) | Shared `MCXINIT_OBJ_2BC` return epilogue (`lw $ra`, restore `$s0`, return, restore stack); not a separate operation. |

`MCXINIT_OBJ_32C` at `80141EBC` is just outside this batch and remains a named continuation hook called by `MCXINIT_OBJ_2BC`.

### Hook boundaries still owned by the host

| Hook group | Original work represented by the hook |
|---|---|
| Earlier save-menu overlay: `SaveUi_DrawText`, `SaveUi_TextWidth`, `SaveUi_DrawWindow`, `SaveUi_DrawWindowBorder`, `SaveUi_DrawIcon`, `SaveUi_DrawMainWindow`, `SaveUi_DrawBackground3D` | Draws text, measures text, draws the window and border, draws icons, and draws the main window/background. These functions are in the earlier member ranges. |
| Earlier save-menu state/input: `SaveUi_YesNoUpdate`, `SaveUi_ConfirmPressed`, `SaveUi_FadeTick`, `SaveUi_SetState` | Updates the yes/no choice, reads confirmation, advances fade state, and performs the earlier overlay's state transition. |
| Memory-card BIOS: `MemCardOp`, `MemCardSync` | Starts and polls BIOS memory-card operations. Their callbacks and card results remain native. |
| MCX service: `McxCardType`, `McxAllInfo`, `McxSync`, `McxHideTrans`, `McxShowTrans`, `McxComInit`, `McxCheckThreadReady`, `McxFastDsrEntry`, `MCXINIT_OBJ_32C` | Queries card type and card metadata, synchronizes MCX events, shows/hides the MCX transition, initializes and checks MCX communication/thread state, enters the fast DSR handler, and continues the adjacent MCX initialization routine. |
| BIOS/platform calls: `VSync`, `PadGetState`, `EnterCriticalSection`, `ExitCriticalSection`, `GetCurrentThread`, `SetThreadHook`, `CreateThread`, `OpenThread`, `StartThread`, `ChangeThread`, `CloseThread` | Waits for a display frame, samples pad state, and manages the BIOS thread and critical-section operations. |
| Text/frame/audio/display calls: `TextDraw`, `PlaySoundEffect`, `BeginFrame`, `ClearImage`, `DrawSync`, `PutDrawEnv`, `PutDispEnv`, `DrawOTag` | Submits text, plays the frame sound, begins a display frame, clears the image, synchronizes GPU work, installs draw/display environments, and submits the ordering table. |
| Renderer: `CardMgr_DrawSelectionBars`, `CardMgr_DrawFileList`, `CardMgr_DrawIconAnim`, `CardMgr_DrawProgressBar`, `CardMgr_DrawMessageLines`, `CardMgr_DrawMessageFrame` | Replaces packet construction with logical renderer requests. The C port keeps the game-state updates and the original native-call ordering around these requests. |

### Findings from the instruction listings

- `CardMgr_SetState` has distinct operation-zero and nonzero paths in state 32, signed halfword fields in the card result, and a timeout scale divided by **90**. State 21 passes `VSync(-1)` and stores the result as a word. States 24/26/31/37/39/40 have the flag and timer effects shown by their individual instruction paths; state 25 resets the repeat counter.
- State 29 returns through draw and menu update; states 19 and 28 only blink and do not invoke the state overlay. The state overlay dispatches selected state values and indexes text descriptors by the full state number.
- Callback 2 is a no-op when the current state is 38; otherwise that target sets state 18. Callback 6's ordinary operation path sets state 21; its `-2`/`-1` path hides the transition and sets state 46. The impossible fall-through instructions in callbacks 2 and 6 are listed in the exclusion registry.
- `CardMgr_MenuUpdate` compares signed selection values to the signed limit. `CardMgr_AnimTick` uses the 16-bit animation frame count/index and the `0x16`-byte record stride.
- `CardMgr_RunMessage` resets menu fade at `8013AF66`; its local `ClearImage` environment pointer is the stack address `801FFED8`. The local display counter and global frame number at `800EC350` are separate fields. If the first fade exits before drawing a choice, the original returns caller-saved `$s1`; both sides receive the same declared hook value.
- `SaveUi_ThreadSetHook` passes the return from `EnterCriticalSection` as `SetThreadHook`'s third argument. The `GetCurrentThread` call and its delay-slot effects are preserved.
- `MCXINIT_OBJ_184` retries mode 17 while retry count is below 3, then selects the failure path; `MCXINIT_OBJ_2BC` creates and starts thread type 1 before calling the out-of-range continuation.
- Text helpers use the instruction-derived label and coordinate bases, preserve the header page-dependent second-string offset, and write the empty-slot character at the original record offset.

### Coverage exclusions and test model

The instruction coverage table above covers every C function except the individually declared dead instructions. The five native drawing ranges and the inline message packet range are registered as native work. `CardMgr_SetState` has five proven dead loop/clamp instructions; `CardMgr_OnCardResult_2` has one skipped jump-tail instruction; and `CardMgr_OnCardResult_6` has three instructions after unconditional transfers to its shared state call. No undeclared coverage gaps remain.

The registry also records the different MIPS/host stack windows, the named 12-byte work area used for stack outputs, and the caller-owned initial `$s1` value in `CardMgr_RunMessage`. The `OnCardResult_9` profiles can set the card-operation field through the same modeled `MemCardOp(13)` hook on both sides, so the post-call signed-operation branches are exercised as well as the normal case. This is test-hook behavior, not a new production-side write.
