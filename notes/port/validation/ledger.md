# Verified-function ledger and checker exclusion registry

Two reviewed files record what the RAM-image differential checkers (`tools/mips/check_bodies.py`, `check_player_input.py`,
`check_monkey_base.py`, `check_monkey_level1..10.py`) actually prove, so that a checker cannot be weakened silently.

| File | Content | Enforced by |
|---|---|---|
| `tools/verified.csv` | one row per ported function: `address,name,c_function,source,checker,status,min_cases,notes`. `status` is `diffed` (ported, passes its checker) or `wip` (ported, checker not finished; monkey levels 8-10 for now). `min_cases` = the per-function case count of the checker at its default scale | `tools/check_verified.py` (fast, no MIPS execution, found by `tools/run_checks.py`) |
| `tools/mips/check_exclusions.csv` | one row per deliberate blind spot: `checker,kind,where,reason` | the harness itself (`tools/mips/exclusion_registry.py`): an undeclared use stops the checker with exit code 3 |

Exclusion kinds: `ram_mask` (address range left out of the RAM comparison: `800B4D00:8`, or `monkey+0xC8:4` for the offset in all 8
monkey slots; also the hook-argument peek mask), `shared_random` (an uninitialised stack word / caller register / leftover `$v0` for
which both sides get the same seeded value: `AE_STK_<block>+<offset>` through `set_stale*`, `<function> $s2`), `stack_window`
(`801FF000..80200000`), `unreachable` (`<function> LO..HI ...`: instructions excluded from the coverage requirement, the reason is
the proof), `dropped_mutation` (documentation only: a mutation removed as dead / equivalent code).

## What runs when

* Every checker, at its default scale and seed, collects instruction coverage and writes `extract/checks/results/<checker>.json`
  (per function: cases, coverage percent, uncovered ranges that no `unreachable` row declares; overall pass/fail; git HEAD). Runs
  with another `--cases`, `--seed`, `--trapv`, `--mutate` write nothing.
* `tools/check_verified.py` (static part): every `ae_*` function of `src/core/ae_sdk.c`, `ae_pad.c`, `src/bodies`, `src/monkeys` and the
  player-input files has a ledger row and vice versa; every row's checker exists and mentions the function; every exclusion row names an
  existing checker. Results part (also run at the end of `tools/run_checks.py`): for every `diffed` row the last default-scale run must
  have passed and reported at least `min_cases` cases. Missing or stale results (another git HEAD) are warnings: the slow checkers are
  not re-run by it. Undeclared coverage gaps are warnings too (see the results files).
* Not original functions (host glue, support code) are listed with the reason in `NOT_ORIGINAL` of `tools/check_verified.py`.

## Rules

* **Lowering a `min_cases` value, changing `status` away from `diffed`, adding a row to `check_exclusions.csv` or widening one needs the
  project owner's review; the commit message must say why.** Raising `min_cases` (after a checker got stronger) is free.
* **Porting a function:** add its ledger row in the same commit (`address` of the original, `name` as in the checker's `FUNCS` table,
  `c_function`, `source`, the checker path, `status` `wip` until the checker is finished, `min_cases` from the default-scale run, which
  prints the per-function table; the value is also in `extract/checks/results/<checker>.json`). Declare every blind spot of the new
  checks in `check_exclusions.csv` with its reason (the harness tells you the exact `where` string when something is missing); do not
  add masks, stale-stack values or unreachable ranges without a row.
* A new checker script goes in `check_*.py`, uses `monkey_harness.MonkeyHarness` (or calls `exclusion_registry.write_results` itself) and
  finishes with `h.finish(EXTRA_COVERAGE, default_run=..., show_coverage=...)` like `check_monkey_level4.py`.

## SDK boundary

SDK and BIOS functions are never ledger rows. Game code calls their public APIs through named hooks with the SDK arguments; the native renderer or platform layer implements those hooks. Differential checkers intercept the same API entry points on the MIPS side and compare hook calls, arguments, results and game RAM. SDK internals do not run on either side.

Exception: pure-maths LIBGTE / libc routines that game logic depends on bit for bit (RotMatrix, CompMatrix,
VectorNormal*, ApplyMatrix*, ratan2, rsin / rcos, SquareRoot0, ...) are exact C ports in src/core/ae_sdk*.c: they are
the native implementation (pure arithmetic, no hardware), so game code calls them directly and they keep ledger rows.
Hardware-facing SDK / BIOS functions (GPU, VRAM, DMA, timing, interrupts, SPU, CD, memory card, BIOS) are always hooks.
