# Player object (2026-10-06)

`g_Player` at 0x800EC1E0, struct `Player` (0x150 bytes) in `tools/structs.txt`.
The camera rotation `MATRIX` follows it at 0x800EC330 (`g_CameraRotMatrix`).
Function names: `tools/symbols.csv` batch 6. Source export: `notes/decomp/player-move.c`.

## The player is a data-driven state machine

```
g_PlayerStateSets[currentGadget + 1]   (PlayerStateSet[10] at 0x800F4DE0; [0] = no gadget)
  .animSet      model animation set
  .stateTable   8 bytes per state: {firstFrame u16, lastFrame u16, flags u32}
  .transitions  {state, cond, newState, action} x N   (PlayerRunTransitions)
  .events       {state, frame, kind, arg, ...}        (PlayerRunAnimEvents)
```

- States `< 0x80` use the no-gadget set; states `>= 0x80` use the current gadget's set.
- `PlayerSetState(s)` sets `state`, starts `animFrame` at the state's firstFrame, sets
  `animSet` and copies the entry's flag nibbles into `stateFlags`. If the previous anim
  hadn't finished, it starts a 5-frame blend (`animBlend`, `animBlendFrom`).
- `PlayerRunTransitions(table)` checks entries for the current state. Conditions and actions
  are small functions reached through tables at 0x800B0298 and 0x800B03AC (examples:
  `PlayerCond_JumpHeld`, `PlayerCond_AnimDone`, `PlayerAct_TurnAround`, `PlayerAct_FaceStick`).
  A negative condition index means "not".
- `PlayerRunAnimEvents` fires SFX, voice, effects and callbacks on specific
  `{state, animFrame}` pairs.

So most of the player's behaviour lives in **data tables**, not code. For the port, that's
just data. For modding it's ideal: new moves are mostly table edits.

## Special-case updaters (called from PlayerUpdate)

| Function | States | What |
|---|---|---|
| PlayerUpdateClimb | 0x38–0x3C | climbing on `climbTarget`; jump (mask 0x0A) → state 8 |
| PlayerUpdateRope | 0x3D–0x41 | moving along `rope` between two points; jump → 0x35 |
| PlayerUpdateHeldByObject | — | locked to a contact object (hazard sub-type 0xB00) |
| PlayerUpdateIdleAnim | — | streamed idle animation after 900 idle frames |
| PlayerTryLedgeGrab | → 0x0E | snap onto a ledge when rising against a wall |
| PlayerOnContact | → 0x24, 0x1B–0x1D, 0x2B, 0x4B | push-off into a fall, hazards, knockback |
| PlayerOnWallBump | → 0x42 / 0x2F | wall bonk |
| swimming | 0x46 0x47 0x49 0x4A 0x4C | `IsPlayerSwimming`, `PlayerSwimSfxUpdate` |

State numbers seen so far: 0x00 stand, 0x02/0x03/0x05 creep/walk/run, 0x08 jump, 0x0E ledge hang, 0x24 fall, 0x38–0x3C climb,
0x3D–0x41 rope, 0x46–0x4C water. Stand/walk pairs 0/1, 3/4, 5/7, 0x13/0x14 switch with
the gadget's stance (`PlayerApplyGadgetStance`).

## Physics

- `moveVel` (+0x120) is the movement velocity; `targetMoveVel` (+0x114) is what input
  wants. `PlayerAccelXZ/XYZ(maxStep)` move one toward the other with a capped step.
- Y is negative-up: jumps set `moveVel.y` to -0x1E000 .. -0x80000.
- `groundY`/`hasGround` come from `GetGroundHeight`; `PlayerIsOnGround` compares them.
- `groundType >> 12 == 1` means water; `waterY` is the surface height (ripples drawn there).
- `contact` (+0x50) is what the player touched: `>>28` type, `&0xFFF` object index
  (`GetObjectByIndex`).

## Named fields

| Offset | Field | Evidence |
|---|---|---|
| +0x18 | animSet | set by PlayerSetState from the state set |
| +0x24 | pos[3] | read by 139 functions |
| +0x30 | vel[3] | added to pos when state == 0x2D |
| +0x50 | contact | PlayerOnContact |
| +0x5C | animFrame | incremented per frame, clamped to the state's frame range |
| +0x5E | state | PlayerSetState |
| +0x60 | facing (SVECTOR) | written as 4 shorts by ledge grab; negated by TurnAround |
| +0x70 | stateFlags | copied from the state table |
| +0x74 | desiredDir (SVECTOR) | TurnTowardsY target |
| +0x80 | savedFacing (SVECTOR) | saved on water entry |
| +0x8C/+0x90 | animBlend / animBlendFrom | PlayerSetState |
| +0xB4 | climbTarget[6] | PlayerSetClimbTarget / PlayerUpdateClimb |
| +0xC0 | rope[8] | PlayerSetRope / PlayerUpdateRope |
| +0xE0 | attachFlags | bits set by the two setters above |
| +0xE8 | health | input zeroed and eyes closed when < 1 (tentative) |
| +0xF2/+0xF3 | currentGadget / savedGadget | |
| +0xF4/+0xF8 | tint / tintTarget | color fade in PlayerAnimateAndDraw |
| +0xFE/+0x100 | squashTimer / squashFrame | model scaling |
| +0x114/+0x120 | targetMoveVel / moveVel | accel functions |
| +0x12C/+0x136 | groundY / hasGround | PlayerIsOnGround |
| +0x138/+0x13C | groundType / waterY | water checks |
| +0x130 | lastPos | idle detection |
| +0x148.. | idleTimer, idleAnimLoading/Ready | idle animation stream |

## Also found in this range

- `LoadTimList` (0x80068D10): uploads TIM images. Besides standard TIM (0x10) it accepts
  two RLE-compressed 4-bit variants (0x10010, 0x20010) decoded by `TimRleDecode1/2`.
  Asset tools need this.
- Heap: `HeapInit` (0x80102800 base), `HeapAlloc`, `HeapFree`.

## State tables

All per-gadget state, transition and animation-event tables are dumped to
`notes/09-player-state-tables.md` by `tools/dump_player_states.py` (reads archive member
0x008). Tuning constants: `g_PlayerParams` (struct PlayerParams). Input snapshot the
conditions read: `g_PlayerScratch` (struct PlayerScratch). Left stick moves, right stick
gives the flick/rotation gestures used for gadgets.

## PlayerUpdate (0x80060780)

A dispatcher on `g_PlayerMode` (set by `PlayerSetMode`):

| Mode | Runs |
|---|---|
| 0 | normal pipeline (below) |
| 1 / 3 | callbacks at 0x800B4DC8 / 0x800B4DC0 (set by area/overlay code) |
| 2 | 0x80142808 (in the area overlay) |
| 5 / 6 / 7 | PlayerUpdateClimb / PlayerUpdateRope / PlayerUpdateHeldByObject |
| 8 | 0x8007C2A0 |
| 10 | PlayerUpdateIdleAnim (streamed idle animation) |
| 12 / 13 | squashed (0x80068ADC / 0x80068B1C) |

Before the switch, every frame: idle timer / idle-anim streaming, copy pad state into
`g_PlayerScratch` (or lock input), ground probe (`groundY`, `hasGround`), exit triggers
(contact type 8 → StageRequest 0x10/0x21), attach flags → climb/rope modes.

**Mode 0 pipeline:**
1. External pushes from `attachFlags`: 0x08 conveyor/boost (state 6, SFX 0x27), 0x40 spring
   (state 0x2C), 0x20 auto/air jump.
2. `GetSurfaceType` → `groundType`; in water, `waterY` and the **air meter** `g_PlayerAir`
   (600 full, drains in swim states, warning SFX, `PlayerDamage(5)` at 0).
3. Water states 0x46–0x4D have their own switch (jump table 0x800A5A78).
4. Gravity: `targetMoveVel.y = vel.y*0x1000 + velFrac.y + gravity*0x80`.
5. Gadget select (face buttons).
6. **Turn mode** (entry byte 4, low nibble) → `facing`.
7. **Move mode** (entry byte 5) → `moveVel` (stick × speed, keep momentum, swim, dash…).
8. Death: `health < 1` → state 0x22 + `XaPlay(0xD/0xF)`.
9. Surface physics: slope (`slopeX/Z`), per-surface accel table `g_SurfaceTable`
   (`surfaceKind`), wall bumps; damaging floors: kind 0 + hazard bit → `PlayerDamage(5)`,
   state 0x2D; kinds 1–3 → `PlayerDamage(1)`, state 0x4D (launched, SFX 0x33–0x35).
10. Water entry → state 0x46, `PlayerResetAir`.
11. `PlayerRunTransitions` for the base set, then the gadget set unless gadget == 9 (none).
12. Entry byte 6 (`frameAction`) called every frame.

**Common tail (all modes that move):** `PlayerOnContact`; `stateFlags` from the entry;
model tilt (`upVector`) per the entry's tilt mode; gadget extras (net aim on 0x81,
gadget 6 effect in 0x84, RC car toggled with R3); clamp `moveVel` (±0x40000 xz,
±0x80000 y); count `fallFrames` at terminal speed; `PlayerAnimateAndDraw`; integrate
`vel = moveVel >> 12` keeping the remainder in `velFrac`; anim events; blend / squash /
hurt-blink timers.

**State-table entry** (struct `PlayerStateEntry`, 8 bytes): `firstFrame`, `lastFrame`,
`turnTilt` (lo: turn mode, hi: tilt mode), `moveMode`, `frameAction`, `flags`.
`dump_player_states.py` now prints these decoded per state.

## Runtime player logger

`tools\pcsx-redux-playerlog.cmd` (player only) or `tools\pcsx-redux-alllog.cmd`
(game mode + monkey AI + player in one session) run `tools/pcsx/player_state_logger.lua`,
writing `notes/runtime/player-state-log.txt`:

```
STATE 0x00 stand -> 0x08 jump   caller=8006xxxx  gadget=0 hp=5 frame=124 held=0008 contact=... water=0 area=0x01 pos=(...)
GADGET 0 -> 1  pc=...  state=...
HEALTH 5 -> 4  pc=...  state=...
```

- `caller` is the function that called PlayerSetState (return address), so each
  transition points at the code that requested it.
- Known state names come from `tools/pcsx/player_state_names.lua`, generated by
  `tools/dump_player_states.py` from its KNOWN_STATES list. Add names there and re-run.
- Logging arms only when the game's `start` (0x800A3660) runs; the BIOS shell executes
  from the same RAM during boot and would otherwise flood the log.
- `pmark('text')` (or `mark('text')` with the combined launcher) adds a marker line.

Seen in the smoke test: during the title/attract sequence the gadget is set to 9 and back
to 0 (code at 0x8007B310 / 0x8007AED8). 9 is outside the 0–8 range of the state sets, so it
looks like a "none/uninitialised" value.

## Next

- Name the states from runtime: a `g_Player.state` logger like the monkey one.
- Dump the state tables / transitions per gadget into a readable file.
- `PlayerUpdate` itself (12 KB) still needs a pass.

## Gadget identification (static evidence, tentative)

`PlayerSelectGadget(gadget, previous)` special-cases per gadget (this is how the ids can be told apart without
playing). State-set sizes come from notes/09:

| Id | Evidence | Best guess |
|---|---|---|
| 0 | stop-voice 0x3D, RStick gestures 0x10/0x20 | Stun Club (known) |
| 1 | 4 states, gesture 0x0C | Monkey Net (known) |
| 2 | no player states of its own; stop-voices 0x25/0x26, initial facing copied by FUN_80099d20 | Monkey Radar (known) |
| 3 | 8 states; re-selecting while equipped cycles a 3-way mode (`DAT_800F51C0` mod 3, SFX 3); enables aim mode (`AimModeEnter`) and is the only gadget not clearing it | **Slingback Shooter** - confirmed by the tutorial text (notes/26): aim view with the L1 button, 3 bullet types (normal / flash / guided) changed with the right stick |
| 4 | 14 states, right-stick forward/back/side conditions, two looping SFX 0x2C voices, mask array cleared by FUN_8006DAC8 | **Super Hoop** - confirmed by the tutorial text: rotate the right stick to turn it into a dash shield |
| 5 | 3 states, model tilt in state 0x81, gestures 0x08/0x0C, no special select code | **Magic Punch** (gadget menu table, see below) |
| 6 | **Sky Flyer very likely**: per-frame action `PlayerAct_FlyerThrust` pushes moveVel[1] upward from PlayerParams (launch impulses `PlayerAct_FlyerLaunchA/B`); 6 states, ground checks + a per-frame action (FUN_80065ED4) in states 0x83/0x84, two looping SFX 0x1D/0x1E with pitch | probably the **Sky Flyer** |
| 7 | RC car (known): `RcCarSpawn` on re-select, R3 toggles | RC Car |
| 8 | 8 states, shares Stun-Club helpers (FUN_8007AA1C, RStick events 0x10/0x20, `PlayerAct_BounceBack`) | **Water Net** (not in the 8-cell gadget menu; used in water) |
| 9 | "none" | - |

Ids 5 and 8 are settled by the gadget menu table (section "Gadget ids confirmed by the gadget menu" below); a
runtime check (log `g_Player.currentGadget` while the pause menu equips each gadget) would still be nice.

### Gadget -> static model slot evidence (2026-10-06 night session)

The per-gadget update/draw functions each draw `g_PlayerStaticModels[slot]` (0x800EC060 + slot*0x10,
see notes/21-player-models.md): gadget 0 (club) slot 0, gadget 1 (net) slot 1 (`MonkeyNetCatchDraw`), gadget 2
(Monkey Radar) slot 2 (`RadarUpdateDraw`, long pole with red/white parts), gadget 3 slot 3 (`Gadget3UpdateDraw`),
gadget 4 slot 4 (`Gadget4UpdateDraw`, striped bar, two 0x2C loop sounds), gadget 6 slot 6 (`Gadget6UpdateSound`, the
0x1D/0x1E loops). Slots 7 and 11 are shared (RC car controller `RcCarDraw` uses slot 11). This fixes which model belongs to which id
(the names were settled later, see below).

### Gadget names from the tutorial texts (2026-10-06, tools/dump_text.py, notes/26-hint-text.md)

The hint texts name every gadget and describe how it is used, which settles the table above: 0 Stun Club, 1 Monkey Net,
2 Monkey Radar, 3 **Slingback Shooter** (3 bullet types), 4 **Super Hoop**, 6 Sky Flyer (very likely), 7 RC Car. The
guesses made here for 5 / 8 (Water Net / Magic Punch) were wrong: the menu table below gives 5 = Magic Punch,
8 = Water Net. The Time Net is described in a boss text and is not a separate selectable id.

### Gadget ids confirmed by the gadget menu (2026-10-06, notes/28-game-strings.md)

The pause-menu gadget screen (old export `menu.c`, case 9) draws 8 cells; cell `i` is shown when bit `i` of the
unlock mask `DAT_800F51C4` is set, its name / 3 description lines come from the pointer table at 0x800B25A0
(5 pointers per cell), and pressing a face button writes the cursor index **directly** into `g_GadgetSlots[n]` and
calls `PlayerSelectGadget(index)`. So the table order is the gadget id order: **0 Stun Club, 1 Time Net (the Monkey Net; the menu string reads Time Net), 2 Monkey
Radar, 3 Slingback Shooter, 4 Super Hoop, 5 Magic Punch, 6 Sky Flyer, 7 RC Car** (7 = RC Car agrees with
`RcCarSpawn`; new game mask 3 = club + net; `PickupPickKindA` tests bit 3 = Slingback ammo pickups). Id 8 is not
in the menu: the **Water Net**, equipped automatically in water. This corrects the earlier guesses 5 = Water Net /
8 = Magic Punch.

## Streamed idle actor blocks (from codex-inbox, notes/56)

Archive member 0x013 is eight 0xA000-byte blocks. `PlayerUpdate` picks block `index & 7` and reads 20 sectors at
`start + index * 20`. `PlayerIdleModelSetup` rebases block-relative u16 word offsets: +4 player frames, +8 state
table, +0xC transitions, +0x10 events, +0x14 secondary model, +0x18 secondary frames (secondary actors animate
with `ModelPoseBones` channel streams, notes/11).

## Area entry points

`AreaGetEntryPoint` (0x80010C3C) copies a 0x24-byte `AreaEntryPoint` (structs.txt: player position, camera eye,
facing; s32 triples) from the area blob, 16 slots at blob +0..+0x23F (exit count at +0x240). `PlayerSpawnAtEntry`
copies the low halfwords of +0x0C..+0x14 to `g_CameraEye` (from codex-inbox, notes/67).

## Controller input and stick gestures (`PadUpdate`, 0x800132xx; batch 111)

Per frame `PadUpdate` reads both ports (`g_PadBuf1` / `g_PadBuf2`), sets the held / changed / repeat words
(`g_PadHeld`, `g_PadChanged`, `g_PadRepeat`: repeat after 0x11 frames, then every 4 frames for the d-pad bits 0xF000)
and processes the analog sticks through the 40 sample ring `g_StickHistory` ({lx, ly, rx, ry} s8):

* each new sample is normalised: magnitude = (|v| >> 7) - 0x15 dead zone, scaled by 0x7F / 0x6A and clamped to 0x7F
  (`g_LStickMag` / `g_RStickMag`), direction = normalised SVECTOR (`g_LStickDir` / `g_RStickDir`); the stored sample
  becomes direction * magnitude;
* `g_RStickPushState`: 0 / 2 / 4 / 8 for right stick magnitude 0, < 0x47, < 0x7D, full;
* `g_LStickSpin` / `g_RStickSpin`: cross product of the last two samples (rotation sense and speed; Super Hoop,
  Sky Flyer);
* left stick flick detection (`g_LStickFlickState`): when the magnitude reaches 0x7D, the larger squared change over
  the last two samples decides a push (< 0x385, state 4, event bit 2) or a hard flick (state 8, event bit 3) in
  `g_StickGestureEvents`; the state latches to 0x80 until the stick returns. The swing gadgets read these bits
  (`GadgetSwingParamsInit`: hard flick = 400 / -0x40 swing parameters, push = 800 / -0x80).

`g_RStickCamDir` is the right stick direction rotated by `g_CameraRotMatrix` (camera-relative input).

