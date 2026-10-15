# Monkeys (2026-10-06)

Source: `notes/decomp/monkey.c` (export of 0x80042E18, depth 2). Names in `tools/symbols.csv` batch 5.

## Why these are the monkeys

- Up to 8 slots of 0x250 bytes at `g_Monkeys` (0x800E5500), count in `g_MonkeyCount`.
- Each frame `MonkeySelectActive` ranks them by distance to the player and keeps the
  nearest (up to 4) in `g_ActiveMonkeys`. Only those get AI and rendering.
- AI walks a nav graph (`g_NavNodes`, debug string `goal: id=%d; gnode=%d, grp=%d`).
- When `g_CurrentGadget == 7` (the RC car, 8th gadget in the game's order), the AI
  targets `g_RcCar` instead of `g_Player`. That's the game's "monkeys chase the RC car"
  behaviour. Same object is drawn by `RcCarDraw` from Mode_Stage.
- `SoundEmitterUpdate` emitters of type 0x6000 read their position from
  0x800E5524 + i*0x250, i.e. the monkey's position (+0x24). The siren/voice sounds
  follow the monkeys.

## Per-frame flow

```
MonkeyUpdateAll (0x80042E18)
├─ per slot: clear flag bit 21 / tick +0x158 timer
├─ MonkeyThinkActive      (skipped while 0x800F4450 & 2)
│   └─ for each active: pick target (player / RC car)
│       ├─ MonkeyNavVisitUpdate  (staggered: frame % max == slot)
│       ├─ MonkeyPickGoalNode    (when +0xEA >= 0xA8D)
│       ├─ MonkeyUpdateTargetDistance
│       └─ MonkeyAiStateUpdate   (switch on +0x78, 0x43 states)
├─ MonkeyUpdateVisuals    (LOD every 3rd frame, anim, shadow, draw)
├─ 0x8003A334 (empty stub)
├─ DelayedSfxUpdate
└─ MonkeySelectActive     (when count/max change or every 30 frames)
```

## Monkey struct (0x250 bytes), fields known so far

Defined in `tools/structs.txt` and applied by `tools/ghidra_scripts/ApplyStructs.java`
(category `/ApeEscape`). It's applied to `g_Monkeys` (Monkey[8]), `g_ActiveMonkeys`
(Monkey*[4]) and `g_CurMonkey` (Monkey*), so the decompiler shows field names.
To change a field, edit `tools/structs.txt` and re-run the script; it replaces the struct.

| Offset | Type | Meaning |
|---|---|---|
| +0x18 | u32 | current animation set ptr |
| +0x24 | int[3] | position |
| +0x78 | u8 | AI state (0..0x42); per-state flags at 0x800AE944 (8 bytes each); handlers via jump table 0x800A4AB0 |
| +0x79 | u8 | ? (1 + group check → state 0x22) |
| +0x7A | u8 | activity: 0 idle, 1 active, 2 skipped, 4 → 1 on selection |
| +0x7B | u8 | slot index (used to stagger work and index nav visit marks) |
| +0x7C | u8 | monkey id |
| +0xD5 | u8 | current LOD (0..3) |
| +0xD7..+0xE8 | | patrol groups: count, current group, step, per-group sizes, visited mask |
| +0xE6 | s16 | goal nav node |
| +0xEA | u16 | frames since last goal pick |
| +0x168 | s16 | shadow size (0 = none) |
| +0x20C | int | distance to current target |
| +0x210 | int | distance to player (used for active selection) |
| +0x248 | u32 | flags (bit 21 transient, bit 27 nav-stuck, bit 30 ..., bit 31 disabled) |

## Spawns and nav graph (per area)

`MonkeySpawnAll` (from AreaLoad) reads the **monkey section** of the area blob (`+0x4E4`,
`g_MonkeyAreaData`): s32 header {magic 0x11, nav section offset, 3 LOD models, anim sets,
max active (low 16 bits of word 10, capped at 4)}. The models and anim sets are the same
in every area; the nav section differs:

- `+0x08`: one 16-byte entry per monkey = spawn node candidates (s16, -1 terminated);
  a leading -1 ends the list → number of monkeys in the area (max 8).
- `+0x88`: per monkey 8 patrol groups × 8 node ids (what MonkeyPickGoalNode walks).
- `+0x488`: nav nodes, 0x18 bytes (flags, x/y/z, link count, links; bit 0 = node rides a
  moving collider).

`tools/dump_monkeys.py` → `notes/15-monkeys-exits.md`: per-area monkey count, spawn
positions, patrol groups, and the area exits. 223 monkeys across the 101 area blobs (the earlier 147 / 51 only covered areas up to 0x34, see notes/51-area-overlays.md)
(the game is usually counted at 204 — the rest presumably come from areas / modes not
reached through these records; not yet checked).

The Monkey Radar (gadget 2) is `MonkeyRadarSignal`: strength from monkeys in the aim cone,
or a direction toward exits leading to areas that still have monkeys.

## Runtime AI logger

`tools\pcsx-redux-monkeylog.cmd` runs `tools/pcsx/monkey_ai_logger.lua` and writes
`notes/runtime/monkey-ai-log.txt`. It puts write breakpoints on `aiState` and `activity`
of all 8 slots and logs only real changes:

```
STATE 0x01 -> 0x05 (handler 8004xxxx) by pc=8003xxxx  gadget=1  slot=2 id=  5 act=1 dist= 812 pos=(...)
ACT   1 -> 2 by pc=...  state=0x..  slot=...
AREA  area=0x01 mode=11 monkeys=6 gadget=0
```

`handler` is the new state's entry in the jump table at 0x800A4AB0. `pc` is the code
that made the transition. Type `mark('text')` in PCSX-Redux's Lua console to drop a
labelled marker into the log at a moment of interest.

The title-screen attract sequence has no monkeys, so the logger has only been
load-tested; the first real data will come from a play session.

## States seen at runtime (playtest 2026-10-06, see notes/10-playtest-2026-10-06.md)

- 0x00 → 0x02 → 0x01: spawn / idle. 0x01 starts idle activities, each a start/loop/end
  triple sharing handlers 0x80041510 / 0x80041520 / 0x80041530: 0x0D–0x0F, 0x10–0x12,
  0x13–0x15, 0x3A–0x3C.
- 0x03 ↔ 0x08: alerted, keeping distance from the player.
- 0x05 → 0x25 → 0x06: attack (the player lost health right after it).
- activity 3 = caught (set at 0x80041FB8 when netted).

## Next

- Name the remaining AI states (jump table 0x800A4AB0), using `mark()` in the next session.
- Fill in the `unk_XX` fields as the AI states get named.

## AI state machine (decoded from the dispatch table, 2026-10-06 night session)

`MonkeyAiStateUpdate` (0x800412B0) does the shared bookkeeping, then jumps through
`g_MonkeyStateDispatch` (0x800A4AB0, 0x43 entries) into 24 small code blocks that call the real
state handlers; every handler returns into the common tail at 0x80041A38 (facing, ground height,
anim pick, nav arrival checks, attack selection). `g_MonkeyStateTable` (0x800AE940, 8 bytes per
state) gives the animation index and behaviour flags per state. The full state → handler map is
generated into `notes/17-monkey-states.md` (`tools/dump_monkey_states.py`).

Findings:
- Idle activities are triples (start / loop / end): states 0xD-0xF, 0x10-0x12, 0x13-0x15, 0x3A-0x3C,
  handled by `MonkeyAi_IdleActivityStart/Loop/End`; the loop does a sight check every 8 frames and the
  end picks another activity (1 in 6) or `MonkeyPatrolDecide`.
- `Monkey.alerted` (+0x79) selects `MonkeyAi_MoveAlerted` vs `MoveUnalerted` for the movement
  states; `MonkeyDecideAction` branches on it too.
- States 4, 0x1D, 0x2C-0x2E, 0x31 are **ballistic jumps** along nav links (linear xz, quadratic y,
  t = +0x96, duration +0x128); 0x2F is free fall with a landing hook; 0x30 walks to the nearest nav
  node flagged 0x2000.
- States 5 (wind-up) and 6 (recovery) are inline in the dispatcher (attack timing, `MonkeyEnterState2F`
  is used when something knocks the monkey off).
- Behaviour probabilities come from a per-monkey 0x88-byte record at `DAT_800b4cf0` (not decoded yet).
