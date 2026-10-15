# SDK boundary validation (2026-10-14)

The four affected modules now retain game functions only. Calls to SDK APIs enter named hooks with the SDK arguments; SDK GPU queues, callbacks, DMA and interrupt bookkeeping, VSync counters, BIOS wrappers and register dispatch are removed. Renderer/platform implementations remain host responsibilities. `tools/check_verified.py` is unchanged. Changes are left uncommitted in the working tree.

## Removed ledger rows

33 rows removed, grouped below. No remaining game row or minimum case count was lowered.

- `src/render/ae_vram_anim.c` (22): `SYS_OBJ_3E4` (`80028734`), `SYS_OBJ_4B4` (`80028804`), `SYS_OBJ_4F0` (`80028840`), `MoveImage` (`80028A38`), `SYS_OBJ_788` (`80028AD8`), `SYS_OBJ_2114` (`8002A464`), `SYS_OBJ_2160` (`8002A4B0`), `SYS_OBJ_23A8` (`8002A6F8`), `SYS_OBJ_23C4` (`8002A714`), `SYS_OBJ_28B0` (`8002AC00`), `SYS_OBJ_28E4` (`8002AC34`), `SYS_OBJ_2A18` (`8002AD68`), `VSync` (`8002E520`), `VSYNC_OBJ_A4` (`8002E5C4`), `VSYNC_OBJ_164` (`8002E684`), `VSYNC_OBJ_178` (`8002E698`), `VSYNC_OBJ_200` (`8002E720`), `DMACallback` (`8002E790`), `SetIntrMask` (`8002E8AC`), `ChangeClearPAD` (`8002F1F0`), `ChangeClearRCnt` (`8002F270`), `puts` (`800A37B8`).
- `src/render/ae_tex_copy.c` (7): `DrawSync` (`800286CC`), `LoadImage` (`80028978`), `StoreImage` (`800289D8`), `SYS_OBJ_2774` (`8002AAC4`), `SYS_OBJ_2794` (`8002AAE4`), `SYS_OBJ_27AC` (`8002AAFC`), `SYS_OBJ_28A0` (`8002ABF0`).
- `src/world/ae_items_init.c` (3): `SetBackColor` (`800315E0`), `SetColorMatrix` (`80031600`), `SetFarColor` (`80031630`).
- `src/player/ae_player_spawn.c` (1): `SetGeomScreen` (`8002FAF0`).

Two SDK-only exclusions were removed from `check_exclusions.csv`: the MoveImage packet-base patch and the VSYNC_OBJ_A4 asynchronous polling range. No SDK `ram_mask` rows were added. The existing bounded MIPS stack exclusions remain, and the texture-copy checker retains four genuinely unreachable game instructions (three negative color clamps and a negative frame rounding adjustment).

## Public hooks

| Module | SDK hooks |
|---|---|
| `ae_vram_anim` | `MoveImage(rect, x, y)` |
| `ae_tex_copy` | `DrawSync(mode)`, `LoadImage(rect, image)`, `StoreImage(rect, image)` |
| `ae_items_init` | `SetBackColor(r, g, b)`, `SetColorMatrix(matrix)`, `SetFarColor(r, g, b)`, `rsin(angle)`, `rcos(angle)`; existing diagnostic hook renamed `printf` |
| `ae_player_spawn` | `SetGeomScreen(distance)` now called directly; existing diagnostic hook renamed `printf` |

Each callback also receives its host context and `AeRam`. Pointer arguments are original addresses in that RAM image. Color values are unshifted SDK arguments, and SetColorMatrix receives the matrix address rather than unpacked GTE register words. SetProjection retains the game's halfword conversion before calling SetGeomScreen. The texture helpers return the host result; AreaTexCopyInit and AreaSetVramAnimList ignore it exactly as the original game does.

The API comparison revealed two differences previously hidden by lower-level hooks: the VRAM temporary rectangle now uses the original caller address `801FFEE0`, and SetColorMatrixMode's restore branch passes the saved matrix at `800E78E0` rather than its identical copy at `800EBFF0`.

## Checker results

All public SDK entry points used by these functions are intercepted before SDK instructions execute. Both sides use the same Python models. Checks compare the hook sequence and SDK argument words, returns, main RAM outside the registered stack window, and all scratchpad bytes. Texture events additionally capture uploaded/read-back image hashes; MoveImage captures the transient RECT contents at call time. The item trig model reads the original quarter-wave table and returns a host value on both sides.

| Checker | Default differential cases | Original game instructions covered | Mutations caught |
|---|---:|---:|---:|
| `check_vram_anim.py` | 73 | 68/68 | 2/2 |
| `check_tex_copy.py` | 13 | 187/191, four declared unreachable | 11/11 |
| `check_items_init.py` | 43 | 489/489 | 23/23 |
| `check_player_spawn.py` | 37 | 553/553 | 29/29 |

All 65 mutations were caught, with at least two per remaining game function. Mutations now target game behavior and public SDK call arguments, including the restore-matrix pointer, fog color channel, texture upload buffer and DrawSync mode. The VRAM mutations change entry selection and list counting instead of injecting unrelated marker writes.

Commands run with `.venv/Scripts/python.exe`: all four checkers at default settings; `--mutate` for all four (VRAM also runs its mutations by default); `--coverage` for texture, items and spawn (VRAM always prints coverage); and `tools/check_verified.py`. The ledger check exits successfully with **0 errors**, and 32 warnings because unrelated checker result files are absent from this worktree. Logs are under ignored `extract/codex-task/` and default result artifacts under ignored `extract/checks/results/`.

## SDK rows elsewhere

The entire ledger was checked by address and name against the call-graph inventory used by `notes/06-sdk-inventory.md`, the identified SDK address ranges, and the local PsyQ 4.5 signature label/object inventory. The remaining matches are **20 LIBGTE rows at 15 unique original addresses**, all outside the four requested modules. They are reported here for follow-up and left unchanged as requested.

| Address | Ledger name | Source |
|---|---|---|
| `80014498` | Square0 | `src/core/ae_sdk.c` |
| `800145D0` | ratan2 (SDK) | `src/core/ae_sdk.c` |
| `8002FB50` | rsin (SDK) | `src/core/ae_sdk.c` |
| `8002FC20` | rcos (SDK) | `src/core/ae_sdk.c` |
| `800300B0` | ApplyMatrixSV | `src/core/ae_pad.c` |
| `800300B0` | ApplyMatrixSV (SDK) | `src/core/ae_sdk.c` |
| `8003019C` | VectorNormalS | `src/bodies/ae_bodies.c` |
| `800301B0` | VectorNormal | `src/core/ae_pad.c` |
| `800301B0` | VectorNormal (player-side) | `src/player/native_player.c` |
| `800301B0` | VectorNormal (SDK) | `src/core/ae_sdk.c` |
| `800301E0` | VectorNormalSS | `src/core/ae_pad.c` |
| `800301E0` | VectorNormalSS (SDK) | `src/core/ae_sdk.c` |
| `80030210` | VectorNormal core | `src/core/ae_sdk.c` |
| `800303C0` | SquareRoot0 (SDK) | `src/core/ae_sdk.c` |
| `80030BD0` | RotMatrix (SDK) | `src/core/ae_sdk.c` |
| `80030FA0` | RotMatrixYXZ (SDK) | `src/core/ae_sdk.c` |
| `8003019C` | VectorNormalS | `src/core/ae_sdk.c` |
| `8002FCC0` | RotMatrixZYX (SDK) | `src/core/ae_sdk_matrix.c` |
| `8002FF50` | CompMatrix (SDK) | `src/core/ae_sdk_matrix.c` |
| `80030E60` | ScaleMatrix (SDK) | `src/core/ae_sdk_matrix.c` |
