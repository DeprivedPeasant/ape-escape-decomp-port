# Game modes (g_GameMode)

Captured with `tools\pcsx-redux-modelog.cmd`, which runs PCSX-Redux with
`tools/pcsx/gamemode_logger.lua`. The log goes to `notes/runtime/gamemode-log.txt`.

- `ENTER` lines come from an exec breakpoint on `main`'s dispatch `jalr` (0x8005BEB0):
  the mode is about to run.
- `SET` lines come from a write breakpoint on `g_GameMode`, with the writer's PC.

Lua breakpoints only fire with `-debugger -interpreter`. `-debugger` is saved in
PCSX-Redux's config; launch with `-no-debugger` for normal play.
`pcsx-redux.exe` is a launcher; the emulator process is `pcsx-redux.main.exe`.

## Session 1 (2026-10-06): boot → title → new game → stage 1 → hub → stage 2 → save

```
mode  0 ovl181  boot                        → 26 (set by ovl181 @8013627C, ~18 s)
mode 26 ovl180  title                       → 3 after ~60 s idle (title @80137BC0)
mode  3 ovl182  ~9 s, sets area=0xFFFF       → 26 (ovl182 @80136404)
mode 26 ovl180  title, New Game             → 8 (title @80137B70), area=1
mode  8 ovl188  → sets 10 → sets 11 itself (no return to main between)
mode 11 Mode_Stage  level 1 / area 0x01 (54 s)  exit 0x08 → 12
mode 12 ovl187  ~4 s                        → 13 (ovl187 @801367A4)
mode 13 Mode_Stage  area 0x58, level 0x1F   exit 0x07 → 9
mode  9 ovl189  ~4 s                        → 16, area=2
mode 16 ovl18B  sets area 0x62              → 17 immediately
mode 17 Mode_Stage  area 0x62, level 0x20 (40 s)  exit 0x16 → ExitSpecialArea → 10, area 2
mode 10 ovl188                              → 11
mode 11 Mode_Stage  level 2 / area 0x02 (2 m 25 s)  exit 0x08 → 12 → 13 (hub)
mode 13 Mode_Stage  hub, exit 0x17 → 21
mode 21 ovl186  flips 21 ↔ 14 internally (ovl186 @801370B0 / @801377BC) → 13
mode 13 Mode_Stage  back in the hub
```

The pause menu didn't change the mode (it runs inside Mode_Stage via OpenPauseMenu).

## Mode table (0x800AFA30) with identifications

Confidence: **seen** = observed this session and the behaviour fits; *guess* = inferred
from shared entry points or Mode_Stage's exit switch; blank = not seen yet.

| Mode | Entry | Code | Identification |
|---|---|---|---|
| 0 | 0x80136000 | ovl 0x181 | **seen** Boot / logos |
| 1 | 0x80136000 | ovl 0x181 | same overlay as boot |
| 2 | 0x80136894 | ovl 0x181 | 2nd entry of boot ovl. Calls RunMinigameExe — *guess* minigame launch |
| 3 | 0x80136000 | ovl 0x182 | **seen** after ~60 s on title, then back to title (session 1) or into the load screen, mode 14 (session 2). Probably the main menu / memory-card check. RunMinigameExe also returns here |
| 4 | 0x8005BEE4 | resident | |
| 5 | 0x80136000 | ovl 0x183 | |
| 6 | 0x8008D1F0 | resident | |
| 7 | 0x80136000 | ovl 0x184 | |
| 8 | 0x8013683C | ovl 0x188 | **seen** Stage start / intro after New Game (runs 8 → 10 → 11 internally) |
| 9 | 0x8013606C | ovl 0x189 | **seen** Leaving the hub via exit 0x07 (~4 s, picks the destination area). *Guess* time machine / era select |
| 10 | 0x8013683C | ovl 0x188 | **seen** Stage start (same overlay as 8) |
| 11 | 0x8005BF70 | Mode_Stage | **seen** In a stage (levels 1, 2) |
| 12 | 0x80136208 | ovl 0x187 | **seen** Stage clear (after exit 0x08), ~4 s, then hub |
| 13 | 0x8005BF70 | Mode_Stage | **seen** Hub (area 0x58, level 0x1F) — Time Station |
| 14 | 0x80136AB8 | ovl 0x186 | **seen** Save screen (state of ovl 0x186); **seen** as the load screen from mode 3 in session 2 |
| 15 | 0x80136208 | ovl 0x18A | |
| 16 | 0x80136208 | ovl 0x18B | **seen** Special-area entry (sets area 0x62), immediate → 17 |
| 17 | 0x8005BF70 | Mode_Stage | **seen** Special area 0x62 (level 0x20). *Guess* training room |
| 18 | 0x8013683C | ovl 0x188 | Stage start variant |
| 19 | 0x8005BF70 | Mode_Stage | Stage variant. Mode_Stage treats 0x13 specially (IsTimeAttack) — *guess* time attack |
| 20 | 0x80136208 | ovl 0x187 | Stage-clear variant (Mode_Stage sends 0x13 → 0x14) — *guess* time-attack result |
| 21 | 0x80136AB8 | ovl 0x186 | **seen** Save screen (entered via hub exit 0x17) |
| 22 | 0x80136AB8 | ovl 0x186 | Save/load variant |
| 23 | 0x8013683C | ovl 0x188 | Stage start variant |
| 24 | 0x8005BF70 | Mode_Stage | Stage variant |
| 25 | 0x80136208 | ovl 0x187 | Stage-clear variant |
| 26 | 0x80137AC0 | ovl 0x180 | **seen** Title screen |
| 27 | 0x801385B0 | ovl 0x1AC | |
| 28 | 0x80136AB8 | ovl 0x1AD | |
| 29 | 0x80136000 | ovl 0x1AE | |
| 30 | 0x80136000 | ovl 0x1AE | |
| 31 | 0x80136000 | ovl 0x1AF | |
| 32 | 0x8013668C | ovl 0x1AF | |
| 33 | 0x80137654 | ovl 0x1AB | |
| 34 | 0x8005BF70 | Mode_Stage | Stage variant |
| 35 | 0x80136AB8 | ovl 0x186 | Save/load variant |
| 36 | 0x8005BF70 | Mode_Stage | Stage variant |

## g_StageExitCode values seen

| Code | Meaning (from this session) | Next mode |
|---|---|---|
| 0x07 | Leave hub through the time machine | 9 |
| 0x08 | Stage clear | 12 |
| 0x16 | Leave special area (ExitSpecialArea) | 10 |
| 0x17 | Save from hub | 21 |

## Level / area values seen

| Level | Area | Where |
|---|---|---|
| 0x01 | 0x01 | Stage 1 |
| 0x02 | 0x02 | Stage 2 |
| 0x1F | 0x58 | Hub (Time Station) |
| 0x20 | 0x62 | Special area |

## Still to capture

- Minigames (modes 1/2/3 and RunMinigameExe), once one is unlocked.
- Load game from the title (probably mode 22 or 35).
- Death / game over, retry.
- Ending / credits (modes 27–33 are the late-game overlays 0x1AB–0x1AF).
