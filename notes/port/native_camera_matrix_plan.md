# Next camera matrix / fog integration

This is a handoff plan, not a claim that the following pipeline is implemented.
The current viewer uses host projection and look-at math. Its opt-in original camera
state updates do not establish original rendered pixels.

## Available foundations

- `native_sdk_matrix.c`: original MulMatrix0 / CompMatrix, complete original CPU/GTE
  comparison; destination padding and exact aliases included.
- `native_sdk_apply.c`: original ApplyMatrixLV split/truncate/wrap pipeline,
  complete original CPU/GTE comparison; writes only 12 output bytes.
- `native_sdk_sqrt.c`: table-backed original SquareRoot0, complete original CPU/GTE comparison over the supported nonnegative signed32 domain.
- `native_camera_math.c`: table-backed original sine/cosine, bounded normalization.
  `check_camera_matrix_dependencies.py` establishes that the long-input VectorNormal
  can reuse this normalization after explicit signed16 narrowing, in its nonoverflow
  domain. Preserve original narrowing; do not substitute full32 normalization.
- `native_prop_sz.c`, `native_prop_gte.c`, `native_prop_depth.c`, `native_prop_fog.c`:
  separately verified depth, AVSZ, restricted DPCS, depth gate and fog-factor helpers.
  They are not wired into the renderer.
- Private ignored `notes/decomp/codex/overnight-prop-view/`: original CameraLookAt,
  SDK matrix operations, VectorNormal, SquareRoot0, roll trig and GetSurfaceType.
  Use EXE instruction execution for proof; a Ghidra function listing can omit branch
  blocks or collapse GTE register-transfer macros.

## Bounded next implementation

1. Load the existing SquareRoot0 (`0x800303C0`) helper with its actual EXE table.
   Its unsigned shift behavior and nonnegative domain have been verified; preserve
   its failure contract. Keep game table bytes external.
2. Build a separate CameraLookAt adapter (`0x80046670`), with explicit inputs for
   the original base matrix (`0x800AF0A0`) and a surface query callback. Preserve
   whole-buffer write masks, original long normalization narrowing, optional roll
   and SDK matrix semantics. Original final normal/color matrix destination is
   `0x800EBFD0`; surface state is written to `0x800F45CC`.
3. Run complete original CameraLookAt using the existing test CPU and pinned external
   GTE backend. Supply narrowly documented surface and memset service endpoints.
   Compare view, control and normal matrices, side effects and preserved bytes.
   Include zero horizontal direction, eye/target signed extremes, positive/negative
   roll, source base matrix and exact aliases supported by the constituent routines.
   Detect original ADD traps before executing their continuation and fail closed.
4. Only then establish the actual per-prop transform composition and RTPT/SZ/AVSZ
   inputs. Finish CC/color-matrix and primitive packet/depth/order evidence before
   wiring original fog into the viewer. The far field in the area header is not
   the current fog-factor coefficient; world Z is not GTE OTZ.

An adapter can stage translation into a 16-byte vector before ApplyMatrixLV and
copy back only its 12 result bytes. A MATRIX has just 12 translation bytes at
offset20; do not accidentally treat its tail as a writable 16-byte VECTOR.

Keep this opt-in and compare existing movement/world traces and legacy images.
Fossil Field cloud cards are ordinary type3 props, separate from the type4/5 sky
background path. Their remaining cyan rectangular edges are unresolved; no tested
helper above proves a fix for that visible artifact.

## Gameplay priority

The larger playable-slice gaps are live collider/body contact, attachment and
progress, automatic hint/player-mode coordination and unsupported camera events.
Coordinate these with local Claude through the inbox; do not modify its player
module or capture tooling. Obtain real hard-fall, ledge-grab, gadget and attachment
segments before claiming full frame-by-frame gameplay parity.
