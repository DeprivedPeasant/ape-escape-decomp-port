# Overnight run, 2026-10-15 (00:57 to 09:57)

Claude Code ran Codex jobs unattended (Sol for integration, Luna at maximum reasoning effort for porting batches),
reviewed each result, sent back the ones that broke project rules, and merged the rest one at a time.

## Result

- Ledger: 871 to 1078 `diffed` rows (about 207 new verified functions); 17 branch merges, 39 commits on master.
- Master at 53b69bf is green:
  - check_game_area: areas 01, 02, 05 and 44; loads plus all frames, 372 comparisons, 0 failures. The ORIGINAL
    PlayerUpdate runs on the MIPS side.
  - check_game, the 56 wrapper suites and 55/55 wiring mutations pass.
  - check_viewer_game passes (2,098,176 bytes equal).
  - check_verified reports 0 errors.
- **Viewer missing functions** (unported game logic the game reaches):
  - area 01: **none** (was 26);
  - area 02: 6 left (was 29): HalfPipeRangeTest, ObjFacePathPoint, ObjFollowPath, ObjMove, ObjSetPosToPathPoint,
    ZakoHooksAdd.
- The viewer runs the real PlayerUpdate; the temporary player bridge is deleted. The Monkey Net catch now runs
  through the original code path (PlayerUpdate -> PlayerRunAnimEvents / MonkeyNetCatchUpdate). It has not been
  checked by playing yet.

## Merged work

| Work | Branch | What it covers |
|---|---|---|
| PlayerUpdate | sol-playerupdate | Ported, wired, viewer bridge removed |
| Player / RC-car body movement | luna5-bodymove | CollisionUpdate cases 1 and 2 |
| HUD and message box | luna5-hud | Game logic; drawing via typed renderer requests |
| Pause menu | luna5-pausemenu | All modes |
| Menu window system | luna5-menuwin | |
| Stage fade | luna5-stagefade | |
| Frame-loop helpers and collider scripts | luna5-misc | 19 functions |
| Enemy AI and helpers | luna5-zakoai, luna6-zakohelpers | 3 handlers + 42 helpers |
| PlayerAnimateAndDraw / RcCarDraw / Monkey Radar | luna6-animdraw | Game logic |
| Area 02 overlay | luna6-ovl02 | Member 18C, 26 functions |
| Save / progress / hub / volume helpers | luna7-progress | 25 functions |
| Game-host wiring | sol-wiring, sol-wiring2..5 | Five passes |

### Module bugs found by integration
Each fix comes with a strengthened checker that fails on the old code.

- Fixes: Indicator_Register return value; CollProbe_Box vertical-reject return (and the check_collprobe blind spot);
  the CollisionRayTest copied-callback context; MessageBoxDrawWindow ordering entry; RibbonDraw; PlayerTryLedgeGrab
  padding; ScaleMoveVel.
- Three water-effect defects, including PlayerSplashStart's missing water-height store.
- The 18C draw-record texture step and Y store.
- Others listed in notes/port/game_module_bugs.md.

### Review rejections (sent back and fixed)
- Simulated MIPS register residue passed to hooks: menuwin, and ovl02's Atan2Safe arguments.
- Renderer-only claims that hid game-state writes: stagefade FadeSpiralDraw; ovl02 EffectDraw / SurfaceDraw.
- A duplicate public symbol: stagefade ScreenFlashDraw.
- A missing mutation suite: progress.
- A duplicate port: SetDAT_800BEF80, removed from progress by Claude.

## Not finished

At about 06:38 the PC ran out of memory ("paging file is too small", OS error 1455). Four Luna jobs crashed with
their work uncommitted in their worktrees. The 15-minute checks also stopped firing from about 06:23, so they were
not relaunched.

| Worktree | Mode overlay | State when it crashed |
|---|---|---|
| luna7-ovl181 | Members 181 / 182 (boot, intro) | Checker running; 182 paths and mutations left |
| luna7-ovl187 | Member 187 (Stage Clear) | 21 functions matching; mode loop being fixed |
| luna7-ovl188 | Member 188 (Stage Start) | RunB matching; one hook argument left |
| luna8-ovl189 | Member 189 (Leave Hub) | Compiles; checker being built |

**luna8-ovl18b** (member 18B, Special Area Enter) finished: 135 cases, 32/32 mutations. It is NOT merged:
- SpecialEnterUi_DrawScene has a diffed row with 0/184 instructions covered;
- it edits notes/inventory/functions.csv.

Fix those before merging.

When the mode overlays are merged, they need the overlay dispatch from wiring pass 5 to wire them, the same way 18C
is wired.

## Next steps
1. Relaunch the four crashed overlay jobs, two or three at a time to limit memory, from their worktrees.
2. Fix and merge ovl18b.
3. Run a wiring pass for the mode overlays.
4. Port the six area 02 missing functions (the path-follow object helpers and HalfPipeRangeTest).
5. Test the Monkey Net catch in the viewer.

## Morning follow-up (10:00 to 11:00)
- Merged: the 18B overlay (Special Area Enter), with DrawScene's game-state copy ported and the inventory untouched.
- Merged: **the real Monkey Net catch**.
  - The original catches monkey 0 in area 01 with a forward swing from 150 units, and the host now matches it
    through the swing, net contact, capture countdown and return from capture mode.
  - Fixes: the ActorAnimDecodeEntry route was aliased to ActorAnimDecode; the capture-flash buffer overlapped
    MatrixLookDir's temporary buffer.
  - New checks: tools/codex/check_net_catch.py and a catch scenario in check_game_area.
- Master 74eb849:
  - check_game_area: 485 comparisons, 0 failures;
  - check_game passes; check_game_wiring_mutations: 57/57;
  - check_net_catch and check_viewer_game pass; check_verified reports 0 errors;
  - 1089 diffed functions.
- **Codex usage limit reached at 10:25, resets 2026-10-21 21:25.**
  - Unfinished jobs are committed as WIP on their branches: luna7-ovl181 (181/182), luna7-ovl187, luna7-ovl188,
    luna8-ovl189, luna9-area02 (HalfPipeRangeTest and the path-follow helpers).
  - Relaunch each with a "continue from the working tree" prompt, at most about 4 at a time.
  - Then run the overlay wiring pass for the mode overlays.
