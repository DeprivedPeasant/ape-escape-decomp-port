# Boot and intro mode overlays (members 181 and 182)

Both overlay members load at `0x80136000`. The checker restores the SCUS 944.23 RAM image, loads one real area member, then overlays the selected member at that address. Members 181 and 182 reuse the same RAM range, so a local hook is active only while its matching member is loaded.

## Member 181 call tree and complete code inventory

The initial inventory had the boot mode and minigame launcher but missed a second code region after `Mode_02`. Instruction prologues, return sites, local calls, the warning string, and the CD event setup identify these additional routines:

| Address | Function | Job / boundary |
| --- | --- | --- |
| `80136000` | `Mode_Boot` | Boot mode; invokes the boot display setup, screen clear, logo A, and optional logo B before selecting mode 26. |
| `801362A4` | `Boot_ClearScreen` | Clears both display buffers and waits for the initial load list. |
| `80136424` | `Boot_SetupDisplay` | Sets geometry offset, initializes the two draw/display environments, and writes boot display flags. |
| `80136528` | `Boot_DrawLogoA` | Runs the first logo's frame loop. |
| `801366EC` | `Boot_DrawLogoB` | Loads the second logo TIM data and runs its frame loop. |
| `80136894` | `Mode_02` | Clears the screen, runs the minigame EXE, then sets mode 3. |
| `80136940` | `Ovl181_CheckCdHardwareIntegrity` | Sends CD commands, checks status bits, stores command parameters, and selects the warning path. |
| `80136AEC` | `Ovl181_CdCommand` | Member-local CD register command engine; replaced by the native CD manager hook. |
| `80136D18` | `Ovl181_DrawIntegrityWarning` | Ported display setup and warning-color loop; the GP0 clear packet, text decoding, glyph packets, and backdrop are native renderer hooks. |
| `80136E60` | `Ovl181_DrawEncodedWarningText` | Native renderer hook for packed-text decoding and glyph packet submission; no C port or ledger row. |
| `80136FA0` | `Ovl181_BuildWarningGlyphPacket` | Native renderer hook for expanding bitmap bits into GPU packet words; no C port or ledger row. |
| `80137014` | `Ovl181_BuildWarningBackdrop` | Native renderer hook for building and submitting the backdrop packet grid; no C port or ledger row. |
| `80137280` | `Ovl181_InstallCdExceptionVector` | Native interrupt hook for changing CP0 state and installing the bundled CD event vector. |
| `801372F0` | `Ovl181_RegisterInitialCdEvent` | Native CD-manager hook for registering the first event callback. |
| `80137338` | `Ovl181_RegisterCdReadEvent` | Native CD-manager hook for installing the read callback and issuing the next command. |
| `8013735C` | `Ovl181_RegisterCdRetryEvent` | Native CD-manager hook for installing the retry callback and issuing the next command. |
| `80137380` | `Ovl181_RegisterCdCompleteEvent` | Native CD-manager hook for installing the completion callback and issuing the next command. |
| `801373A4` | `Ovl181_HandleCdStatusEvent` | Native CD-manager hook for routing the next callback from CD/CP0 state. |
| `80137424` | `Ovl181_HandleCdPositionEvent` | Native CD-manager hook for comparing the event timestamp and rearming a callback. |
| `80137490` | `Ovl181_HandleCdModeEvent` | Native CD-manager hook for polling CD status and changing the active callback. |
| `80137540` | `Ovl181_HandleCdSectorEvent` | Native interrupt hook for acknowledging the low-level CD event through CP0. |

`Ovl181_CheckCdHardwareIntegrity` and `Ovl181_DrawIntegrityWarning` are game-logic ports. The integrity check preserves the original command order and early-failure paths, stores the three input bytes only after status checks, shows the warning only when the gate byte is set and status bit `0x10` is clear, always sends final command 9, and returns -1 if an earlier or final command failed. The warning port sets up the display environments, follows the `GetVideoMode() == 1` field update, sets the text rectangle for both lines, alternates the warning color around each text hook, submits the backdrop hook, enables display output, and exits through the BIOS. Its inline GP0 clear-packet block is a registered renderer patch. `Ovl181_CdCommand` is a native CD-manager hook that supplies the controller operation result.

The string at `0x801371A0` decodes as Japanese text meaning the program was forcibly terminated and the console may have been modified. That, plus the status test and direct CD register table at `0x80137220`, motivates the integrity/warning names. The names for the embedded callback entry points describe their register use and are inferred; their original overlay contains no symbol names.

## Member 182 inventory

The code entry points are `Mode_03` (`80136000`), `Ovl182_DrawScene3D` (`8013654C`), `Ovl182_UpdateCamera` (`8013674C`), `Ovl182_Init` (`80136AC0`), `Ovl182_StateMachine` (`80136C5C`), `Ovl182_DrawIntroState` (`80136D00`), `Ovl182_MenuInput` (`80136F54`), `Ovl182_FadeTick` (`801374E4`), and `Ovl182_DrawFade` (`801375C4`). The nonzero tail after `DrawFade` is packed menu text, jump-table entries into `Ovl182_MenuInput`, and static tables; it contains no further function prologue or return-root pair.

The main call flow is `Mode_03` → initialization / state machine / frame rendering. The frame path reaches scene drawing and camera update; menu input and fade helpers control the state transitions. LIBGTE sine, cosine, matrix construction/composition, and matrix application use the validated SDK C ports. GPU packet blocks remain renderer hooks.

## Differential status and instruction coverage

The default checker runs against four real area members. At CD command 2, the harness also records the three staged bytes at `0x80137230` so the native hook comparison checks the implicit command payload at the moment it is consumed. Only the C ports in the coverage matrix have ledger rows with `overlay member 181.bin` or `overlay member 182.bin`; pure CD, interrupt, and packet-building functions are named native hooks with no C body or ledger row. Their original ranges are registered in `tools/mips/check_exclusions.csv`.

| Function | Cases | MIPS instructions executed / total | Coverage note |
| --- | ---: | ---: | --- |
| `Mode_Boot` | 2 | 167 / 169 | 2 declared unreachable: negative arm after the void logo loop. |
| `Boot_ClearScreen` | 4 | 96 / 96 | Full. |
| `Boot_SetupDisplay` | 4 | 65 / 65 | Full. |
| `Boot_DrawLogoA` | 2 | 113 / 113 | Full. |
| `Boot_DrawLogoB` | 2 | 106 / 106 | Full. |
| `Mode_02` | 4 | 31 / 31 | Full. |
| `Ovl181_CheckCdHardwareIntegrity` | 8 | 107 / 107 | Full. |
| `Ovl181_DrawIntegrityWarning` | 4 | 74 / 82 | 8 declared unreachable: inline GP0 clear-packet renderer block. The video-mode field passed to `PutDispEnv` and both warning colors are compared. |
| `Mode_03` | 7 | 281 / 339 | 58 declared unreachable: inline mode-frame packet builder and impossible return arms. |
| `Ovl182_DrawScene3D` | 8 | 75 / 128 | 53 declared unreachable: inline water-quad packet builder. |
| `Ovl182_UpdateCamera` | 8 | 221 / 221 | Full. |
| `Ovl182_Init` | 8 | 103 / 103 | Full. |
| `Ovl182_StateMachine` | 12 | 39 / 41 | 2 declared unreachable: impossible negative draw-state result. |
| `Ovl182_DrawIntroState` | 6 | 59 / 149 | 90 declared unreachable: native intro-title packet block. |
| `Ovl182_MenuInput` | 13 | 356 / 356 | Full. |
| `Ovl182_FadeTick` | 8 | 56 / 56 | Full. |
| `Ovl182_DrawFade` | 8 | 41 / 75 | 34 declared unreachable across the first ordering-table link and final packet submission/link block. MIPS executes timer, alpha, fade-state, and state-dependent packet-mode branches; `OverlayFadeQuad` receives the cursor, state, timer, alpha byte, and selected mode word. |

Overall coverage is 1,990 / 2,237 instructions across the 17 diffed C ports. The 247 declared gaps are the native packet blocks and impossible branches shown in the matrix. Pure CD, interrupt, exception, and private packet-building routines are represented by named native hooks and their registered exclusion ranges; they are not ledger rows. RAM masks remain limited to the reserved packet/ordering-table area and documented native camera outputs; game state, computed hook values, and packet cursors remain compared.

## Native hooks left open

The host still needs to supply these platform, archive, audio, input, display, and renderer operations. Their argument words are preserved at the original call boundary.

| Area | Hook names | Original operation |
| --- | --- | --- |
| Archive and CD | `StageInitMemory`, `LoadListStart`, `ArcLoadEntry`, `ArcLoadMember`, `LoadListStep`, `RunMinigameExe`, `CdMgrWaitIdle`, `CdMgrUpdate`, `LoadTimList` | Initialize the load memory, start and step the asynchronous archive list, read archive entries/members, wait for or advance CD-manager work, load TIM assets, and execute the minigame EXE. |
| Controller | `PadSetupDualShock`, `PadGetState`, `PadInfoMode`, `StickToButtons` | Set analog/actuator mode, read pad state and capabilities, and derive pseudo-buttons from stick axes. |
| Game state | `GameStateReset` | Reset the resident game-state globals before entering the intro mode. |
| Model and renderer | `ModelInitNormals`, `MeshInit`, `ModelTransformRigid`, `MeshTransformRot90`, `ModelTransformSkinned`, `ModelDraw`, `MeshDrawAnimColors`, `ModelEmitPrims`, `ModelEmitPrimsCycleColors`, `DrawShadow`, `AlertIconDraw` | Initialize or transform model data; project, light, and emit model, mesh, shadow, and alert-icon packets. |
| Display / GTE | `ResetGraph`, `StopCallback`, `GetVideoMode`, `SetGraphDebug`, `SetDispMask`, `DrawSync`, `ClearImage`, `ClearImage2`, `DrawOTag`, `PutDrawEnv`, `PutDispEnv`, `SetDefDrawEnv`, `SetDefDispEnv`, `VSync`, `SetGeomOffset`, `SetGeomScreen`, `SetRotMatrix`, `SetTransMatrix`, `SetBackColor`, `SetColorMatrix`, `SetFarColor`, `InitDisplay`, `BeginFrame` | Reset and configure PsyQ display/GTE state, synchronize, clear VRAM, submit ordering tables/environments, and drive frame timing. |
| Debug input display | `DebugTextSetColorWrap`, `DebugDrawStickHistory`, `DebugDrawStickGauge` | Set debug text color and draw stick diagnostics. `DebugTextPrintfWhite` is already ported and remains a direct call. |
| Audio | `SoundStopAndRestoreVolume`, `SoundStopAllVoices`, `SoundFade`, `SoundSetStereo` | Stop voices, restore/fade volume, and change sequence stereo mode. |
| Member 181 CD / interrupt | `Ovl181_CdCommand`, `Ovl181_InstallCdExceptionVector`, `Ovl181_RegisterInitialCdEvent`, `Ovl181_RegisterCdReadEvent`, `Ovl181_RegisterCdRetryEvent`, `Ovl181_RegisterCdCompleteEvent`, `Ovl181_HandleCdStatusEvent`, `Ovl181_HandleCdPositionEvent`, `Ovl181_HandleCdModeEvent`, `Ovl181_HandleCdSectorEvent` | Execute CD controller commands, install the bundled exception/event handlers, route status/position/mode events, and acknowledge sectors. These are named native hooks with no C ports or ledger rows. |
| Member 181 warning renderer | `Ovl181_IntegrityWarningClearPacket`, `Ovl181_DrawEncodedWarningText`, `Ovl181_BuildWarningGlyphPacket`, `Ovl181_BuildWarningBackdrop`, plus `Exit` | Build the warning's GP0 clear packet, decode its packed text, build glyph/backdrop packets, then terminate through the BIOS. The warning display setup and color loop remain in the `Ovl181_DrawIntegrityWarning` C port. |
| Member 182 renderer | `OverlayModeFrame`, `OverlayWaterQuad`, `OverlayIntroTitle`, `OverlayFadeQuad` | Draw the background frame, water quad, intro title, and fade quad from the values computed by the game logic. |

Already ported EXE helpers are called directly, including `PadUpdate`, `PadRumble`, `PadRumbleReset`, `MatrixFromDirXZ`, `ModelPoseBones`, `ModelInit`, `StaticModelInit`, `SetColorMatrixMode`, `ModelDrawWithOt`, `ApplyLevelFog`, `EffectsReset`, `SoundStop`, `PlaySfx`, `HeapInit`, `ColorFadeReset`, and `DebugTextPrintfWhite`. `CameraLookAt` uses the existing yaw-matrix adapter; the rest of its native GTE outputs remain outside the RAM comparison as documented by the checker exclusion.

## Mutations and remaining caveats

The corrected checker ran 35 mutations, at least two per diffed C port, and caught all 35. Pure native hooks have no C body or mutation cases. The command-2 payload snapshot makes mutations of the first saved CD parameter observable before the phase byte is overwritten.

The CD event callback argument arities are inferred from register use because the functions are embedded library code without normal game-level call sites. They remain native hooks; the checker validates their named boundary arguments and does not claim to emulate the CD driver or interrupt hardware. The new names and the integrity-check interpretation are evidence-based but not original symbols.
