# Animation, player draw, RC car, and Monkey Radar port

This batch adds the 16 requested EXE functions as readable `AeRam` C in
`src/player/ae_animdraw.c` and declares their offsets, globals, and hook signatures in
`src/player/ae_animdraw.h`. `src/game/` was not changed. The differential checker is
`tools/mips/check_animdraw.py`; it runs the original instructions against the C port over the
real EXE and four real area members (`05B.bin`, `064.bin`, `066.bin`, and `05E.bin`).

## Port and default differential evidence

Each row lists the default per-function case count and target-function instruction coverage.
The four rows with excluded instructions have separate proof or renderer-hook entries below.

| Address | Function | Cases | Instructions covered |
|---|---|---:|---:|
| `80014A84` | `MatrixFromFrontUp` | 120 | 187/187 |
| `80014D70` | `AnimDecodeFrame` | 80 | 49/49 |
| `80015240` | `AnimFramePtr` | 80 | 11/11 |
| `8001526C` | `AnimDecodeLerpPtr` | 100 | 156/156 |
| `800463A4` | `StepToward` | 100 | 16/16 |
| `80046584` | `IntSqrt` | 120 | 37/37 |
| `80063628` | `PlayerAnimateAndDraw` | 80 | 544/551; 7 excluded |
| `80068BC0` | `PlayerSwimSfxUpdate` | 48 | 82/82 |
| `8007C618` | `PlayerRumbleSlam` | 32 | 45/45 |
| `8007CA48` | `PlayerCanActivateGadget` | 64 | 38/38 |
| `80097E88` | `AreaGetMonkeyRecord` | 32 | 41/41 |
| `80098FEC` | `PlayerHeadLookUpdate` | 32 | 219/219 |
| `80099AE0` | `RcCarDraw` | 32 | 89/89 |
| `80099D70` | `RadarUpdateDraw` | 64 | 235/259; 24 excluded |
| `8009A17C` | `MonkeyFlag248Set` | 48 | 10/10 |
| `8009A1A4` | `MonkeyRadarSignal` | 80 | 199/209; 10 excluded |

The checked call tree also covers existing direct callees in the task paths: `AnimDecodeLerp`
(168/168), `ModelBuildBoneMatrices` (159/159), `AreaGetLevel` (7/7), and `GetLevelFirstArea`
(6/6). `MatrixFromFrontUp` calls the validated `VectorNormalSS` port; other pure GTE math is
called through the already-ported SDK math functions. `AreaGetMonkeyRecord` calls the existing
area ports, and `PlayerAnimateAndDraw` reuses the existing gadget and color-matrix ports.

## Call tree and native boundaries

`PlayerAnimateAndDraw` updates the animation frame and pose, builds bone matrices, updates head
look and swim/rumble/gadget state, performs the color transition and radar state updates, then
passes the resulting model, matrices, pose, and effect data into the draw hooks. `RcCarDraw`
builds the car transform and updates its state before passing the model and transform to the
renderer. `RadarUpdateDraw` computes signal, fill color, loop audio state, model transform, and
OT cursor state before handing off the visible model work. `MonkeyRadarSignal` reads the current
area's monkey records and computes the bounded signal from eligible target distances.

The requested renderer functions remain named hooks with their original call arguments:

| Hook | Native responsibility |
|---|---|
| `ModelTransformRigid`, `ModelTransformSkinned` | Transform rigid or skinned model vertices. |
| `ModelCalcEnvUVs` | Calculate environment-map texture coordinates. |
| `ModelDraw`, `ModelEmitPrims`, `ModelDrawEnvMap`, `ModelEmitPrimsCycleColors` | Dispatch model drawing and emit model/environment-map primitives. |
| `ModelFillVertexColors` | Fill a model's vertex colors. |
| `VertexWobble` | Apply renderer vertex wobble after game logic supplies its transform result. |
| `DrawShadow` | Draw a player or vehicle shadow. |
| `Draw_Nop` | Preserve the original no-op draw call and arguments. |
| `EffectRingDraw` | Draw the player effect ring. |
| `SfxVoiceSetPitch` | Set a sound voice's pitch. |

Two inline blocks are represented by named native hooks. `RendererPrimitiveBlock` receives the
packet cursor, cycle-table pointer, radar model, and the original branch predicate (one at the
call site); the renderer owns packet construction and ordering-table linking. `RendererWobbleGte`
receives the camera matrix, wobble vector, and player pointer and returns the IR value consumed by
`VertexWobble`. Their MIPS oracle patches cover only those replaced blocks; the surrounding game
logic remains instruction-diffed.

Other functions owned by active jobs remain named hooks: `PlayerStopGadgetSounds`,
`SoundEmitterPitch`, `SoundLoopStop`, and `TurnTowardsY`. Additional public SDK/hardware calls
remain named (`SetRotMatrix`, `SetTransMatrix`, `SDK_80031230`, `SDK_800302D0`, sound, rumble,
and controller entry points). `SetColorMatrixMode` is a direct call to its already-ported C
function. Gadget-owned calls reached through this path, including the gadget-3 aim projection and
line draw, remain under the existing gadget/native-renderer hooks.

## Coverage exclusions

All exclusions are append-only entries for `check_animdraw` in
`tools/mips/check_exclusions.csv`:

* `PlayerAnimateAndDraw 80063958..80063970` is the inline COP2 wobble transform delegated to
  `RendererWobbleGte`.
* `RadarUpdateDraw 80099F3C..80099F94` is the packet construction and ordering-table link
  delegated to `RendererPrimitiveBlock`.
* `RadarUpdateDraw 80099F24` is an impossible divide-by-zero path: `MonkeyRadarSignal` returns
  0 through 32, so the packet denominator `40 - signal` is at least 8.
* The ten `MonkeyRadarSignal` instructions at `8009A2D0`, `8009A2E8`, `8009A2F8..8009A300`,
  `8009A318`, `8009A334`, and `8009A344..8009A34C` are negative-distance corrections, invalid
  denominator branches, and signed-overflow checks. `IntSqrt` returns nonnegative distance,
  denominators are positive, and the bounded numerators cannot overflow signed arithmetic.
* The stack window `801FF000..80200000` is masked because the MIPS routines use call frames there
  while the C ports use named temporary buffers in that reserved window. Persistent RAM, return
  values, scratchpad, hook sequences, and pointed-to stack payloads used by hooks are compared.

## Findings from the instruction diff

* `PlayerSwimSfxUpdate` sums absolute velocity components with wrapping 32-bit addition and then
  performs a signed comparison against 9. Treating that sum as unsigned changes overflow cases.
  When stopping a swim loop, the original tests the left voice and passes right then left to
  `SoundLoopStop`; it does not require both voice slots to be active.
* The radar voice indices are at offsets `+0x14` and `+0x16` in the radar record, distinct from
  the swim sound-loop offsets. The inactive radar fill uses packed color `0x00202020`.
* The radar renderer's cycle-table pointer is derived from the cycle-table base and the primitive
  color offset. At the packet replacement point, `$v0` holds the already-evaluated branch
  predicate, so the native hook's phase argument is one rather than the modulo remainder.
* `MonkeyRadarSignal` uses different contribution formulas on the two sides of distance 513:
  the near-distance reciprocal is used below the threshold, while the score-correction ratio is
  used at and above it. The old pseudocode had these branches reversed.
* `RcCarDraw` passes the output cycle-color buffer as the fourth argument to
  `ModelEmitPrimsCycleColors`. The original leaves `$a3` from `ModelPrepCycleColors` in place;
  recomputing it from the cycle count changed the hook call.
* The tests include speed overflow and negative velocity, multiple real area layouts, zero and
  nonzero radar signals, active car state, and valid/edge matrix vectors. Invalid randomized
  matrix vectors that trap in original GTE arithmetic are avoided rather than weakening the
  oracle comparison.

The default checker and the required mutation run are recorded by
`tools/mips/check_animdraw.py`; ledger rows in `tools/verified.csv` use the per-function case
counts above.
