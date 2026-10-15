# Effect and projectile spawning

Instruction-derived tree (SCUS_944.23, before implementation):

* SpawnParticle 800698B8 -> SpawnEffectGroup 800696EC -> SpawnOneParticle
  8006962C -> ParticleAlloc 80069548. Group entries 50 -> EffectSpawnFromDef
  80070284 -> EffectObjSpawn 80070130 -> EffectObjAlloc 8006FE7C;
  entry 51 -> TrailSpawn 8006ED78. EffectObjSpawn optionally calls MatrixLookDir
  8001484C. Other group entries contain base/spread particle counts using the
  shared halfword LCG at 800AE8F0.
* SpawnPuff 8006B3CC -> SpawnParticle(0x113).
* EffectBigBurst 8006B3F0 -> SpawnParticle(0x10D), TrailSpawn, two
  EffectSpawnFromDef calls, sixteen SpawnParticle(23), sixteen SpawnParticle(7),
  CameraShake 80048A00 (leaf, three LCG draws).
* SparkAddRandom 8006E0DC -> SparkAdd 8006E054 (leaf).
* ProjectileSpawn 80059304 -> GetGroundHeight 80043400 (existing world port,
  its GetSolidHeight host dependency), VectorNormalSS 800301E0 (existing SDK
  port), MatrixFromDirXZ 80014804 (hook), ApplyMatrixSV 800300B0 (existing SDK
  port), memset 800A3740 (zero eight local bytes), PlaySfx 800877F8 (hook),
  ProjectileAllocSlot 80084AC8, ProjectileInit 80084B48, ProjectileSetByte2
  80084BB8, ProjectileSetColor 80084B74, ProjectileSetUpdate 80084C78, and
  SpawnParticle (tree above). The callback stored by ProjectileSetUpdate is
  80084C04; it is not invoked during spawning.

The ten jump-table destinations are respectively 800597B8, 8005997C,
800599FC, 800599FC, 80059780, 80059448, 80059518, 80059638,
800596DC, 8005982C. Kinds 3/4 zero velocity; kind 6 treats the fourth
argument as a small variant number, rather than a vector pointer. Kind 6 is
limited to four records, counting kind bytes even without another active flag.

Layouts confirmed from instructions: particle 0x34 bytes (matches the VM's
52-byte state), effect object/definition 0x54, trail 0x10, spark 0x0C,
gameplay projectile 0x30, secondary projectile record **0x34** (the older
symbol comment saying 0x2C is incorrect). SpawnOneParticle takes three
arguments; the older comment's fourth velocity argument is unused.

## Implementation and validation

`src/effects/ae_spawn.{c,h}` contains hand-written structured C, with named
field offsets and one public function per original. SDK VectorNormalSS and
ApplyMatrixSV and the world GetGroundHeight port are reused directly; their
sources are unchanged. `effect_vm.c` and its header are unchanged. The VM's
state layout is used for the particle field constants; the existing header
does not expose field-offset definitions to reuse.

`tools/mips/check_spawn.py` loads the actual EXE and area-1 member (0x001
area index, member resolved through dump_areas), reconstructs the area's grid
pointer fixups, and compares all nonstack main RAM, all scratchpad bytes,
meaningful returns, and ordered host calls. It exercises every nonnull default
group in both type namespaces and all 50 script-table indices, partial/full
pools, cursor wrap, unaligned eight-byte positions, negative counts/widths,
halfword truncation, signed projectile divisions, all ten projectile kinds,
kind-6 variants and the exact four-projectile limit, primary/secondary pool
failure, real ground data, and randomized retained fields. Missing sound,
matrix, ground, and position-padding callbacks are tested for failure.

Default seed 94423: **954 cases**, approximately **10 seconds**, no differences.
All new functions are `diffed`:

| Function | Address | Default cases | Instructions executed / total |
|---|---|---:|---:|
| ParticleAlloc | 80069548 | 44 | 57/57 |
| SpawnOneParticle | 8006962C | 37 | 48/48 |
| SpawnEffectGroup | 800696EC | 24 | 115/115 |
| SpawnParticle | 800698B8 | 182 | 11/11 |
| TrailSpawn | 8006ED78 | 9 | 48/49 |
| EffectObjAlloc | 8006FE7C | 32 | 26/27 |
| EffectObjSpawn | 80070130 | 12 | 85/85 |
| EffectSpawnFromDef | 80070284 | 12 | 11/11 |
| ProjectileAllocSlot | 80084AC8 | 65 | 32/32 |
| ProjectileInit | 80084B48 | 32 | 11/11 |
| ProjectileSetByte2 | 80084BB8 | 32 | 2/2 |
| ProjectileSetColor | 80084B74 | 32 | 17/17 |
| ProjectileSetUpdate | 80084C78 | 32 | 7/7 |
| ProjectileSpawn | 80059304 | 209 | 468/468 |
| SpawnPuff | 8006B3CC | 32 | 9/9 |
| EffectBigBurst | 8006B3F0 | 32 | 96/96 |
| SparkAdd | 8006E054 | 32 | 33/34 |
| SparkAddRandom | 8006E0DC | 32 | 57/57 |
| CameraShake | 80048A00 | 72 | 111/111 |

Coverage is **1244/1247**, with all reachable instructions executed. The three
registered gaps are 8006EE28 (TrailSpawn), 8006FED0 (EffectObjAlloc), and
8006E0C8 (SparkAdd). Each is a negative branch after incrementing an unsigned
byte cursor; the intermediate is 1..256, so the branch is impossible even with
a malformed byte cursor. There are no undeclared gaps in these functions.

Reused SDK routines have complete coverage through this checker. Reused
GetGroundHeight has informational coverage 275/318 here; the remaining
terrain-region, out-of-grid, and heightfield-disabled branches belong to its
existing independent checker `check_world.py` (459 ledger cases), rather than
new exclusions or weakened requirements for the new spawning functions.

`--mutate` builds 45 deliberately incorrect copies and detects all 45, with
zero survivors. The first volley-limit mutation exposed a missing exact-four
scenario; that scenario was added and the mutation then failed. Mutations
cover cursor/pool selection, retained field writes, script/group index splits,
count rounding, delay order/spacing, sound-driven projectile setup, velocities,
and caller helpers. `--trapv` passes. `tools/check_verified.py` passes after
adding `src/effects/ae_spawn.c` to SCAN and appending 19 `diffed` ledger rows.
Existing checker results absent from this worktree produce warnings, not
errors; this change's default result is present in extract/checks/results.

## Hooks and host contract

* `PlaySfx` 800877F8: queues sound id 0x4C for projectile kind 6. The sound
  system remains another job. PlaySfxAt is not invoked by these spawn trees.
* `MatrixLookDir` 8001484C: builds the 32-byte orientation matrix from an
  optional direction passed to EffectObjSpawn. It runs even with count <= 0.
* `MatrixFromDirXZ` 80014804: builds the direction matrix for kind-6 shots;
  ApplyMatrixSV rotates the variant's velocity through it. These two hooks
  should be replaced by the corresponding ae_pose functions when available.
* `world`: the existing AeWorldHooks table for GetGroundHeight; its
  GetSolidHeight hook 8004D8C0 performs collision-grid/box ground queries.
  The ground function itself is called directly, not replaced in the checker.
* `position_padding`: supplies the unwritten fourth position halfword for
  SpawnEffectGroup 800696EC / EffectBigBurst 8006B3F0, separately. Original
  eight-byte copies preserve this value in effects and trails. Missing this
  callback fails closed when required; empty groups and single particles do
  not need it. The checker seeds distinct random group and burst values.

No renderer is called during these routines; rendering and the callback at
80084C04 remain outside this spawn-only module. Keeping its original numeric
callback address in the record does not execute that callback.

Native temporary position/vector/matrix buffers are marshalled in disjoint
borrowed RAM ranges 801FF020..801FF0BF. These are ordinary address-taking API
buffers, with no simulated CPU registers, frames, or delay slots. Callers must
not re-enter spawn functions on the same AeRam during a hook, and must not
retain these addresses. The standard private-stack window is registered and
excluded from RAM comparison. Matrix-hook local pointers are normalized to a
local-buffer tag while their input halfwords, nonlocal pointer identities,
output effects, and call order are checked; this is separately registered.
No game-state RAM masks were added. The registry additions require the project
owner's review before committing, as prescribed by the ledger rules; no
commits or application-settings changes were made.

## Findings and limits

Particle allocation searches 100 free slots, then 100 priority-zero slots,
then overwrites slot zero; it advances the cursor on every probe. Allocation
does not clear a particle. SpawnOneParticle clears only the low byte of its
x velocity and leaves many reused fields intact. EffectObjAlloc's clears are
then overwritten by the definition copy inside EffectObjSpawn. Effect delays
are assigned in descending remaining-count order, with spacing six in the
wrapper. Secondary allocation failure retains earlier primary projectile
writes while returning 1; kind 6's four-record limit returns before ground
query or any write. Kind 7 truncates to halfwords before several comparisons
and divisions; using a single wide vector calculation would differ.

Validation does not claim to prove actual sound/matrix/solid-ground hook
implementations or rendering; it verifies dispatch, data passed to them, and
the continuation under deterministic hooks. The padding value is intentionally
caller-owned because the original does not initialize it. No guessed gameplay
behavior or unresolved field-layout decisions were introduced.
