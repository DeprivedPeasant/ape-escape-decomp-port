# Native player movement (cloud branch `claude/gallant-bohr-ms36p2`)

Portable C99 reconstruction of the original player movement update: `src/player/`
(`native_player.c/.h`, `test_player.c`, `make_fixture.py`, `check_tables.py`, `run_tests.sh`).
Claimed in `notes/COORDINATION.md`; no Codex renderer / collision / audio file is edited.
All behaviour is traced to the reviewed evidence package `extract/cloud_evidence/player/`
(decompiler pseudocode + instruction listings + decoded `state_tables.json`). Nothing in this
module is differentially validated against the original game: the cloud has no EXE, no member
0x008 and no emulator. Tests are synthetic (see "Validation").

## Interface (integration for the native demo)

```c
AePlayerData d;  ae_player_data_init(&d);                    /* PlayerInit constants, approximate sine table, built-in VectorNormal table */
ae_player_data_load_member(&d, member008, size);              /* real archive member 0x008, as extracted */
ae_player_data_load_exe(&d, exe, size);                       /* exact sine table, g_SurfaceTable and VectorNormal table from SCUS_944.23 */
AePlayer p;      ae_player_init(&p, pos, facing);             /* PlayerSpawnAtEntry defaults */
AePlayerPad pad; ae_player_pad_init(&pad, exe, size);         /* PadUpdate on a RAM image (src/core/ae_pad.c), idle history */

per 30 Hz frame:
    ae_player_pad_set_camera(&pad, eye, target);              /* g_CameraRotMatrix of the previous frame's camera */
    AePlayerPadRaw raw = { held, rx, ry, lx, ly, 0 };         /* the raw pad packet: buttons + analog bytes 4..7 */
    AePlayerInput in;  ae_player_pad_update(&pad, &raw, p.facing, &in);   /* PadUpdate(1): every stick value PlayerUpdate reads */
    in.player_mode = 0;  in.frame_counter = n;
    AePlayerGround g = { ground_y, slope_x, slope_z, surface_flags, ground_type, water_y };   /* from GetGroundHeightEx / GetSurfaceType at p.pos */
    uint32_t unsup = ae_player_update(&p, &d, &in, &g);       /* AE_UNSUP_* bits: behaviour reached but not modelled */
    <resolve p.pos + p.vel against the world: ae_collision_resolve + ground clamp>
    ae_player_apply_motion(&p, new_pos, new_vel, contact);
    ae_player_pad_frame_end(&pad);                            /* FrameEndUpdate: this frame's stick sample (processed by the next PadUpdate) */
```

* `AePlayerGround` maps one-to-one onto what the existing collision engine already returns:
  `AeGroundHit.height/slope_x/slope_z/surface` from `ae_collision_ground_height`, `ground_type` from
  `GetSurfaceType`. `surface_flags` is the scratchpad word `DAT_1f8003f8` (cell attribute: bit 0 hazard,
  bits 1-3 surface kind, low nibble 1 = no floor).
* `AePlayerInput` is the `PlayerScratch` block (`structs.txt`). **Naming trap:** in `PadUpdate` and
  `PlayerUpdate` the symbols `g_RStick*` / `DAT_800e54d0..` hold the *movement* stick (physical left
  stick, raw pad bytes 6 / 7) and `g_LStick*` the gadget stick (physical right stick, bytes 4 / 5); the `PlayerScratch` names
  (`leftStickClass`, `leftStickMag`, `leftStickCam`) are correct. Nothing in `AePlayerInput` is computed by the player module
  any more: `src/core/ae_pad.c` is an instruction-for-instruction port of `PadUpdate` (0x8001325C) with `PadNormalizeSticks`,
  `PadSampleStickHistory`, `QuadrantFromSigns`, `VectorNormalSS` and `ApplyMatrixSV`, diffed against the original code
  (`notes/port/controls.md`, `tools/mips/check_player_input.py`). The raw packet goes in; the 40 sample history, both sticks'
  magnitude (dead zone 0x15, scale 0x7F / 0x6A) and direction, `g_RStickPushState` (0 / 2 / 4 / 8), the spins, the whole
  `g_LStickFlickState` machine (push 4 / hard flick 8 / hold 0x80 / spin 0x10 / 0x20, `DAT_800e54d4`), `g_StickGestureEvents`, the
  camera-relative vectors (`g_RStickCamDir` / `DAT_800e54bc`, forward / side components `DAT_800e54d0/d2` and `cc/ce`) and the
  travel sums come out as the original computes them. `ae_player_stick_class` / `ae_player_stick_cam` remain only as helpers of
  the synthetic tests.
* Position integration, the ground clamp and contact recording are `CollisionUpdate` /
  `BodyLandOnGround` / `BodyRecordColliderContact` / `CollisionResolveProbes`; **those are not in the
  evidence package**, so the module stops at `p->vel` and the host (or the tests' stand-in) integrates.
* Events (`p.events[]`) report SFX / voice / rumble / damage / FX requests for the host; nothing is played here.

## Behaviour implemented (evidence)

| Behaviour | Evidence |
|---|---|
| Data-driven state machine, `PlayerSetState` incl. 5-frame blend rule | `80064208_PlayerSetState.c`, `800649d8_PlayerRunTransitions.c` |
| 27 transition conditions, 7 transition actions | `800650xx..80065bxx_PlayerCond_*`, `800661d4`, `80065c20`, `80066770` ... (table index -> function: `state_tables.json`) |
| Turn modes 0 / 1 / 2 / 3 / 7 / 10 (snap, 0x71 / 0x155 limited turn, none, toward desiredDir, 8-way) incl. `TurnTowardsY` | `80060780_PlayerUpdate.c` lines ~330-470, `80066c90_TurnTowardsY.c` |
| Move modes 0-11, 14-16: stand, creep/walk (mode 3, stick-scaled anim), run (4), jump air control (7), dash, crouch walk, drag in fall (1) ... | PlayerUpdate move-mode switch |
| Gravity, jump impulse, hold boost, double jump (`JumpFromLStick`), `FallingHigh`, `PlayerIsOnGround`, `PlayerAct_Land` (soft / hard landing, fall damage, squash) | `PlayerAct_*`, `PlayerCond_*`, `PlayerInit` constants |
| Ground physics: slope compensation, per-surface acceleration (`PlayerAccelXZ`), steep-slope wall bump, damage floors | PlayerUpdate lines ~700-960 |
| Common tail: clamp, `fallFrames`, `vel` / `velFrac` split, anim events (SFX / voice / call index), blend / squash timers | PlayerUpdate tail |
| Spring / conveyor / air-jump pushes from `attachFlags` | PlayerUpdate lines ~210-300 (host must set the flags) |

Every state reachable from stand through the no-gadget transition table (31 states: 0, 2, 3, 5, 8, 9, 0xA,
0x10-0x13, 0x15-0x17, 0x24-0x2A, 0x2F-0x33, 0x35-0x37, 0x44, 0x45) uses only modelled conditions, actions
and modes. States entered by *code* (wall bump 0x42 / 0x2F, death 0x22, launch 0x2B, hurt 0x4B / 0x4D) are
partly modelled; the 0x46..0x4D range is a jump-table dispatch (see below).

## Not modelled (reported through `AE_UNSUP_*`, never guessed)

* Gadgets (2026-10-07): the nine gadget state sets load from member 0x008 and run after set 0 (PlayerUpdate order);
  `PlayerCheckGadgetSelect` (exact: `CanSwitchGadget` 0x80066F50 with `IsPlayerSwimming`, then Triangle / Square / Circle /
  Cross = pad bits 0x10 / 0x80 / 0x20 / 0x40 pick `gadget_slots[0..3]`), the state part of `PlayerSelectGadget` (current / saved
  gadget, equip SFX request, gadget state -> 0x24 in the air / 0 on the ground), `GadgetHasAltStance`,
  `PlayerCond_GestureEvent0C` (43), `PlayerAct_FaceStick` (15, also as a frame action) and the complete `PadUpdate` stick
  processing (including the spin states 0x10 / 0x20) are modelled, which is enough for the **Monkey Net** swing (0x80 start,
  0x81 sweep while the stick is held, 0x83 end; `test_gadget.c` on the real data). The new-game slot / unlock defaults are
  ported from `GadgetSlotsInitForExit` (below). Not modelled: the other gadgets' conditions / actions / move modes (they fail
  closed), aim mode, gadget sounds and effects of `PlayerSelectGadget`. Naming note: the game's `g_LStick*` globals are the
  gadget stick (physical right stick) and `g_RStick*` the movement stick.
* Water: air meter, swimming (move mode 0xC, turn mode 4), `PlayerCheckWaterEntry`. States 0x46..0x4D dispatch through
  the table at 0x800A5A78 to four gravity handlers (H1 stop+buoyancy, H2 buoyancy, H3 pinned, H4 normal; found in
  the listing, `80060780_PlayerUpdate.asm.txt` 865-958, then they rejoin the normal flow; the decompile's `return` is wrong).
  The handler addresses are read from the EXE (`ae_player_data_load_exe`); without them these states fail closed.
  Which state uses which handler needs those 32 bytes (EXE file offset 0x96278).
* Climb / rope / held-by-object / idle animation modes (5, 6, 7, 10), ledge grab, contact types 4 / 5 / 0xB.
* Model tilt (`upVector`, tilt modes 1-4), effects spawning (dust, splashes, particles).
* Right-stick gesture conditions; camera-lock / aim input.

## Missing original inputs (for the local agent)

1. **`g_SurfaceTable` (0x800AEF60, 8 x 0x10 bytes) and the SDK sine table (0x800A9DB0, 1025 s16)**. The C module
   reads both from the EXE (`ae_player_data_load_exe`) at runtime; `check_tables.py <EXE>` verifies the
   built-in sine approximation and prints the surface fields. Without them ground acceleration is not
   reproducible (the module reports `AE_UNSUP_COND` rather than inventing values).
2. **`CollisionUpdate` (0x8007247C), `BodyProbeGround`, `BodyLandOnGround`, `BodyRecordColliderContact`,
   `CollisionResolveProbes`**: where `pos += vel`, the floor clamp, the landing vertical velocity and
   `Player.contact` come from. Needed to make fall / land exact instead of the host stand-in.
3. ~~`VectorNormalSS` core at 0x80030210~~ **resolved (2026-10-07):** `src/core/ae_vecnorm.h` is the core translated from the
   listing (GTE SQR, LZCR, the 192-entry table at 0x800A9C14 = floor(sqrt(2^30 / (i + 64))), GPF, `srav`). **`$v0` is the sum of
   squares** (never overwritten after the two trapping `add`s), so `norm_returns_squared` is gone; the vector is the low 16 bits
   of `IR0 * IRn >> t6` (`VectorNormal` stores the full 32 bits). A signed overflow of the sum traps in the original; the port
   counts it (`ae_player_normalize_traps`) and writes nothing. The older approximation (`floor_sqrt(2^30 / index)` and an
   `(x * factor) >> shift` without the 16-bit IR truncation) agreed on 2-D input but not in general.
4. **The 32 table bytes at 0x800A5A78** (state -> handler H1..H4; handlers themselves are decoded) and `PlayerCheckGadgetSelect` internals
   if gadget play is wanted.
5. **Original-instruction differential inputs**: a PCSX-Redux `player_state_logger.lua` capture (tools/pcsx/) of
   `{pad, pos, vel, state, animFrame}` per frame on a known area would validate walk / run / jump / land end to end.
6. `RotMatrixY` (used by `TurnTowardsY`) is not in the package; the standard libgte y-rotation matrix is assumed.

## Validation

`src/player/run_tests.sh` (gcc, ASan/UBSan; ~7.1k checks, 0 failures):

* math: `IntSqrt` and the vector normalisation are cross-checked against the instruction-validated Codex routines
  (`ae_collision_sqrt(n,0)`, `ae_collision_normalize`) over 300k inputs each; sine/cosine identities;
* loader: a member-format image is synthesised from `state_tables.json` and parsed (78 states, 192 transitions,
  reversed state-0x4D frame range preserved); EXE table loader on a synthetic image;
* behaviour on a synthetic flat floor with a *synthetic* surface table: stand loop, creep / walk / run state
  selection and exact `moveVel` formula, bounded turning, jump (impulse, hold boost, apex), landing, ledge fall,
  fall-damage entry into 0x4B, event emission, unsupported-state reporting.

These tests prove the code matches the pseudocode transcription and internal consistency. They are not
original-game differential tests.

### Input path: differential check against the original instructions (2026-10-07)

`tools/mips/check_player_input.py` (found by `tools/run_checks.py`, ~45 s) runs the original MIPS code of SCUS_944.23 and the C
ports on identical RAM images (GTE through the pinned oracle) and requires byte-identical RAM, scratchpad, return values and
hook-call sequences: `VectorNormalSS` / `VectorNormal` (also the player's `ae_player_normalize` with the built-in and the EXE table;
the add-overflow traps are matched), `ApplyMatrixSV`, `QuadrantFromSigns`, `PadNormalizeSticks`, `PadSampleStickHistory`,
`PadUpdate` (random RAM states, NULL-hook fail-closed runs, a gesture state x sample grid, 116 cases sitting exactly on the
push / flick, spin and 0xB50 / 0xB51 thresholds, and 900 frames of flick / spin / creep / hold-spin / random stick sequences compared
after every frame, ring wrap included), the `g_CameraRotMatrix` part of `CameraLookAt`, `CanSwitchGadget`, `IsPlayerSwimming`,
`PlayerCheckGadgetSelect` and `GadgetSlotsInitForExit` / `InitForArea` / `Save` / `Restore`. About 14.7k cases, 98.0 % of the
original instructions executed (the rest: 8 instructions of `PadUpdate` that are provably dead - negative-sum and negative-index
branches - and the roll / view-matrix half of `CameraLookAt`, which is not ported). Hooked, not ported: `PadGetState`,
`PadInfoMode`, `PadSetupDualShock`, `PadDemoPlaybackStep`, `StageRequest`, `StickToButtons`, `PadRumbleReset`,
`PlayerSelectGadget` (only its state part is in `ae_player_select_gadget_from`), `HudFindGadgetIconPos`.

## Handoff for local Codex work (requested 2026-10-06)

Branch `claude/gallant-bohr-ms36p2`. Codex: please work only in `tools/codex/` and your own notes; send any
request to change `src/player/` back through `notes/codex-inbox.md`. In priority order:

1. `python src/player/check_tables.py extract/disc/SCUS_944.23 --dump <ignored path>` and report the
   output (sine-table mismatches against the built-in approximation, the eight surface entries).
2. Dump the 32 bytes at 0x800A5A78 (EXE file offset 0x96278): which of handlers H1..H4 each state 0x46..0x4D uses
   (`ae_player_data_load_exe` already reads them; this just confirms and documents the mapping).
3. Export pseudocode + `.asm.txt` into `extract/cloud_evidence/player/` (same format and README rules) for:
   `CollisionUpdate` 0x8007247C, `BodyProbeGround`, `BodyLandOnGround`, `BodyRecordColliderContact`,
   `CollisionResolveProbes`, the `VectorNormalSS` core 0x80030210..0x800302CC with its lookup table address,
   `sin_1` 0x8002FB8C and `RotMatrixY`. Note which callers set `Player.contact` and where `pos += vel` and the floor clamp happen.
4. Capture a PCSX-Redux trace with `tools/pcsx/player_state_logger.lua` extended to log per frame
   `{pad held, left-stick bytes, pos, vel, velFrac, moveVel, state, animFrame, facing, groundY, contact}` for: standing,
   walk, run, turn, jump (held / tapped), walk off a ledge, hard fall. Keep the file ignored (not in Git) but record its checksum.
5. Add `tools/codex/check_native_player.py` that replays the trace's inputs through `ae_player_update` (build with
   `src/player/run_tests.sh` flags) and reports the first diverging frame per field.

Integration (Codex-owned renderer): replace the `move_actor` stand-in in `src/app/native_viewer.c` with
`ae_player_update` + `ae_collision_resolve`, feeding `AePlayerGround` from `ae_collision_ground_height`. Read the
"Interface" section above; keep the fixed 30 Hz step.

## Body motion adapter (`native_player_body.{c,h}`, `test_body.c`)

`ae_player_body_step(AePlayer*, const AePlayerBodyWorld*)` reconstructs CollisionUpdate's player-body pass from
`local_handoff/8007247c_CollisionUpdate` and its callees (BodyProbeGround, CollisionResolveProbes arithmetic,
BodyLandOnGround, PlayerTryLedgeGrab, BodyRecordColliderContact gating, grid clamp), calling back into the host for
ground heights, wall resolution (`ae_collision_resolve` adapter shown in the header), ray tests and collider lookup.
144 synthetic checks pass (all worlds are made up; no original-game validation). Fail-closed bits report the pieces
the listings leave to unexported shape helpers (collider scan, ledge-grab ray test, probe-point builders, mode-1 sound/rumble).

Findings from the listings (the decompile / brief were wrong on these):
* BodyLandOnGround restores the pre-step snapshot when the body rises more than 0xFF in one step (negative-up Y), not on a drop.
* PlayerTryLedgeGrab needs `vel.y > 0` (falling).
* The top-probe ceiling term is applied to `vel.y` and again to `y` (kept as in the listing).
* **Request for Codex (`src/collision/native_collision.c`, `ae_collision_resolve`):** the grid-probe contact id comes from
  `lh` (sign-extended 16-bit, CollisionResolveProbes line ~121) while collider ids use `lhu`; the engine treats the grid
  id as unsigned, so the contact word differs when a cell's +0xE value is negative.

Viewer integration: replace `move_actor` with `ae_player_update` then `ae_player_body_step`, using the `walls` adapter in the header.

## Volume queries (`native_player_volume.{c,h}`, `test_volume.c`)

Implements the two host callbacks of the body adapter from `cloud_evidence/player/body_dependencies/` listings:
`ae_volume_collider_at` (BodyRecordColliderContact scan, BRC:56-320, with CollHalfPipeHeight, ColliderTrenchHeight,
DiagonalSignedDistance, ColliderSlopeContactDir, SquareRoot0) and `ae_volume_ray_test` (CollisionRayTest, mode 0 = the
PlayerTryLedgeGrab call, over the probe points / collider mask of CollisionBuildProbePoints + CollisionPointInGrid +
CollisionShiftProbePoints + PointInBoxXYZ + ColliderTypeActive). One `AeVolume` is the `user` of all four body
callbacks; `ae_volume_body_walls` records the body as CU:160 sees it and forwards to the host's walls. Records are the
28 x int16 (0x38-byte) Collider records of the Codex loader. 313 synthetic checks (incl. one ae_player_update +
ae_player_body_step ledge grab and one collider-contact frame); no original-game validation.

Still missing (callbacks; without them the query fails closed: collider_at -1, ray_test 0 = no snap, bit in `status`):
* `g_CollShapeHandlers` table 0x800AF180 and its handlers (class-0x2000 colliders, via ColliderSlopeContactDir).
* Probe handler tables `PTR_CollProbe_Box_800B20E0` (grid shapes) and `PTR_CollProbe_Box_800B2108` (colliders) and
  their functions: CollisionRayTest only sequences them (the same tables CollisionResolveProbes uses).
* `GetSolidHeight` 0x8004D8C0: the only possible writer of scratch 0x1F8003FA, which the ray-test ground fallback returns.
* LoopSlotRef / LoopSlotAdd paths (ray modes >> 16 != 0; not used by the player); the CBP write of collider +0x32.

Listing vs pseudocode / brief:
* ColliderTrenchHeight calls VecLength3 with a third argument (a2 = dz for 0x53, dx for 0x54; CTH:14/122): the trench
  radius is measured in the y/z (x/y) plane, not `VecLength3(dy, 0)`.
* CollisionRayTest tests the BODY's eight probe points (built at CU:160) and skips colliders that PointInBoxXYZ puts
  more than half+0x200 away from the body (mask 0x800B20C0), not from the ray point. Grid ids are `lh` (a negative id
  still falls to the ground test and is returned when the point is not underground); collider ids are `lhu`.
* BRC sends shapes 0x4E-0x51 (half spaces) down the cylinder path; ColliderHalfSpaceInside is not called by BRC or CRT
  (exported only). ColliderSlopeContactDir passes y absolute (x/z relative) and reads sp+0x2C that only the handler writes.
* DiagonalSignedDistance leaves its foot point stale for orientations other than 0/0x400/0x800/0xC00 (fails closed).
* SquareRoot0's `srav` path is hidden by Ghidra's overlap warning (SQ0:15-20); negative inputs other than -1 read below
  the LUT (fails closed). LUT values are taken as floor(sqrt(t4 << 18)) per Codex's check of all 192 entries.

## Synthetic walk test after the exact input path (2026-10-07)

`--walk-test` feeds raw pad bytes through the ported `PadUpdate` instead of the former float magnitude / direction. Because the
stick magnitude is now the original's `((x*x + z*z) >> 7) - 0x15` scaled by 0x7F / 0x6A (tilt 114 gives 95, tilt 127 only 123: the
history stores `0x7F - raw` for y, so only raw 0x00 reaches +127 and run needs magnitude 0x7D) and the stick reaches the player
one frame late (`FrameEndUpdate` samples it, the next `PadUpdate` processes it), the old scripted route no longer visited both
scripted camera regions of area 5 and its jump at frame 60 hit another transition priority there. The walk script was adjusted
(full-deflection bytes 0xFF / 0x00, a shorter left leg, up / down legs added around it); the hidden traces and the generated
`notes/port/validation/overnight_demo_validation.md` differ accordingly (covered pixels changed; kinds / regions are unchanged).
No expected value of `src/player/run_tests.sh` changed.
