# native_indicators

Portable state for world indicators. Functions derived from the
Ghidra pseudocode and MIPS listings of `Indicator_Register` (0x800716EC) and
`IndicatorsUpdateDraw` (0x800719F8, animation slice 0x80071A90..0x80071B68).
Also includes local state and requests from `Indicator_Activate` (0x800717B4).
Original-instruction validation reports are generated in validation/indicators_validation.md
and validation/indicator_activation_validation.md.

## Slot layout (24 bytes, little endian)

| off | size | field |
|---|---|---|
| 00 | s16 | kind |
| 02 | s16 | angle (low 16 bits of the argument) |
| 04 | 3 x s16 | position (8 bytes copied to 04..0B) |
| 0A | s16 | timer (inside the 8 byte copy, then zeroed) |
| 0C | s16 | direction sin (low 16 of rsin) |
| 0E | s16 | zero |
| 10 | s16 | direction cos (low 16 of rcos) |
| 12 | 2 bytes | padding, never written by register or step |
| 14 | u16 | animation frame |
| 16 | u16 | state |

Slot stride 0x18 and the array base (0x800BBC50 + 0xE4 = 0x800BBD34) match the asm
(`sll/addu` x3 then `<<3`).

## ae_indicator_register

Mutations in original order: kind, angle, 8 byte position (lwl/lwr/swl/swr),
frame=0, state=0, `rsin(angle)`, dir zero, `rcos(angle)`, timer=0, count+1.
Returns the previous count. The original calls `exit(1)` when count == 4; here
that case (and null pointers or unloaded math) returns -1 before any write, a
deliberate portable divergence. Trig receives the full 32 bit angle (a2 is
passed unchanged to rsin/rcos); only the stored angle is truncated. Trig uses
`ae_camera_rsin/rcos`.

## ae_indicator_step

Only the frame/state/timer slice. Frame = u16 + 1 as a 32 bit value (65535
becomes 65536 for the comparisons and is stored as low 16 bits = 0). Kind is
the signed s16.

- kind 0/1: state 0: frame > 28 -> 0. State 1: frame > 88 -> 0 and state = 0.
  Other states: no wrap. Writes model A frame.
- kind 2: state 0: frame > 57 -> 0. State 1: frame > 87 -> 0 and state = 0.
  Other states: no wrap. Writes model B frame. Then if s16 timer > 0, timer-1
  (every state, per 0x80071B54).
- kind < 0 or > 2: only the frame is stored; no model write.
- Frame is stored in all cases (0x80071B68). Other bytes untouched. Null slot
  is a no-op; null model pointers skip only that write.

## ae_indicator_activate

Kinds 0/1 reject state 1 or a busy message box; other states are accepted.
Accepted hint activation writes state 1 / frame 29 and returns the supplied
32-bit sector plus hint and sound 0xF requests. Kind 2 requires state 0 and
signed timer <= 0; it writes state 1 / frame 58 / timer 90, and requests three
ordered SpawnPickups(1, position, 3, 0) calls at the slot position with Y minus
0x140 wrapped to signed16, then sound 0xF. Other signed kinds do nothing.
Null arguments reject without writes; valid blocked activations clear requests.

The original hint branch opens the message box, allocates 0x800 bytes, reads
archive member 0x14 at the supplied sector, queues CD commands 0xE/2/0x20,
requests stage mode 3, enters player mode 0xB, clears HUD bit 0x10, aim and fixed
camera mode. These services are returned as a hint request, not executed here.
The local slot mutation is therefore only one part of the original transaction.

## Not implemented (remaining interactions)

Hint message box and CD hint-ready logic (tail of
IndicatorsUpdateDraw), save progress and time attack interactions, physics,
and all drawing: GTE RTPS, frustum/depth tests, MatrixFromDirXZ, pose/skin/draw
calls and shadow. Also the model frame addresses (0x800BBCB0, 0x800BBD20) are
supplied by the caller as pointers.

## Validation

Strict compilation and 2,000 full original registration / 6,000 animation-slice
comparisons pass. Activation checks execute the complete original routine with
controlled service callees: 8,000 cases compare every slot byte and ordered
hint/pickup/sound calls, including wrapped pickup Y, signed timer, unusual states
and busy-message gating. This does not validate the external services or a full
original gameplay frame.
