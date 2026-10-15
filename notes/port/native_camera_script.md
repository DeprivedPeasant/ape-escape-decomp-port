# Native camera script VM (local subset)

`native_camera_script.c/.h` implement a bounded subset of `CameraScriptRun` (0x8004C318).
The script address at slot +0x84 is a numeric original address; `original_base` maps
`data[0]`. No native pointers, no game bytes embedded, inputs `data` read-only.

## API

`int ae_camera_script_run(slot, controls[0x50], data, size, original_base, result)`

- slot, controls and result are required. 1 = success, 0 = invalid / unsupported.
- On 0 nothing is modified (work is done on copies, committed once at the end).
- On 1, `result->remove_slot` is set: `UINT32_MAX` = none, else the byte at slot +2
  (opcode 2's `slot->linkedSlot`).
- Script address 0 or signed timer (+0x88) nonzero: success, no change, remove none,
  `data` not examined (matches the original's early exit).

## Opcodes (halfword units, little endian)

| op | operands | effect | yields |
|---|---|---|---|
| 1 | s16 | next PC = (PC+2) + s16 bytes (32-bit wrap), sets +0x84 | yes |
| 2 | - | remove request; +0x84 = PC+2 | yes |
| 3 | u16 | controls+0x34 = operand, controls+0x3F = 1 | no |
| 4 | u16 | controls+0x36 = operand, controls+0x40 = 1 | no |
| 20 / 25 | 7 | timer(+0x88)/step(+0x8A) = op0, state +0x90 = 8 / 9, eye +0x0A/0C/0E = op1..3, focus +0x94/96/98 = op4..6, 8 eye bytes (incl. padding) copied to +0xA8; PC += 16 | yes |
| 23 | 2 | timer/step = op0, scriptActive +0x03 = low byte of op1; PC += 6 | yes |

Other opcodes (including 5-19, 21, 22, 24, 26+) return 0 (unsupported, no guessing).
The original prints "camera script error" for unknown opcodes and yields; not modelled.

## Bounds

- `original_base` even, `base + size <= 2^32`, PC even, each fetched instruction and all
  of its operands inside `[base, base+size)`.
- A sequential next PC equal to the end of the blob is accepted (the next call fails);
  a next PC of 2^32 is rejected. A jump target must have a full instruction available.
- At most 1024 commands per call (a non-yielding opcode 3/4 chain without a yield fails).

## Caller contract

The caller applies the removal after success: `CameraSlotRemove(remove_slot)` must still
do the real global / slot mutations; this module covers only the VM-local slot and
control-block part. The caller loads the real blob and passes base 0x80154000 (or the
appropriate base). In the original, opcode 2 calls `CameraSlotRemove` first and +0x84 is
written afterwards (loop exit); the host applies the removal after this function, so if
the removed slot is the running one, the host must decide whether the +0x84 write survives.

## Validation still required

Not built or run here (no shell). Needed: strict compile, comparison of all 252 local
scripts / 633 commands against the original, synthetic guards (bounds, wrap, rollback,
1024 cap, unsupported opcode, timer/script-zero no-op).
