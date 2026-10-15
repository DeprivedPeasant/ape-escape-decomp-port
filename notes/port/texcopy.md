# Area texture-copy setup

`src/render/ae_tex_copy.{c,h}` ports the nine texture-copy and PsyQ system-object
entries listed in `extract/codex-task/task.md`. The code keeps the rectangle,
heap table, GPU queue counters and status pointers in the original RAM image.
The native renderer owns the actual draw-sync, image-load and image-store work.

## Instruction call tree

This tree follows the `jal`, `j` and indirect `jalr` targets in the SCUS-94423
instruction listing. The `SYS_OBJ_27AC` and `SYS_OBJ_2794` entries share a
fall-through wait loop; `SYS_OBJ_2774` also branches into the tail at
`8002AB78`, beyond the 32-byte range Ghidra assigned to its entry.

```text
AreaTexCopyInit(rect, index, rgb, unused)
  HeapAlloc(4096) -> source image buffer                    [hook]
  HeapAlloc(4096) -> variant image buffer                   [hook]
  StoreImage(rect, source buffer)
    SYS_OBJ_3E4(format, rect)                               [hook]
    indirect GPU image handler(state, rect, 8, image)       [native hook]
  save rect's first 8 bytes at 0x800B98D0 + index * 8
  DrawSync(0)
    optional Printf(format, mode), when graph debug >= 2    [hook]
    indirect GPU sync handler(mode)                         [native hook]
  LoadImage(stack RECT, source buffer)
    SYS_OBJ_3E4(format, rect)                               [hook]
    indirect GPU image handler(state, rect, 8, image)       [native hook]
  for color weights 1 through 7:
    transform each source pixel into the variant buffer
    LoadImage(stack RECT at the frame's VRAM x/y, variant buffer)
    DrawSync(0)
  HeapFree(source buffer)
  HeapFree(variant buffer)

SYS_OBJ_2774(wait)
  if wait == 0:
    SYS_OBJ_28B0()                                          [hook]
    SYS_OBJ_27AC()
  else:
    optional SYS_OBJ_23C4() when queue distance is nonzero [hook]
    read GPU status flags and return queue distance / 1
  queued tail at 8002AB78 reads the same queue and status state

SYS_OBJ_2794()
  SYS_OBJ_23C4()                                            [hook]
  SYS_OBJ_28E4()                                            [hook]
  on success, fall through into SYS_OBJ_27AC's wait loop
  on timeout, branch to the shared SYS_OBJ_28A0 epilogue

SYS_OBJ_27AC()
  while queue read and write indices differ:
    SYS_OBJ_23C4()
    SYS_OBJ_28E4()
  while status A bit 0x01000000 is set or status B bit 0x04000000 is clear:
    SYS_OBJ_28E4()
SYS_OBJ_28A0()                                              [shared epilogue]

HeapFree(pointer)
  scan the 64 pointer words beginning at 0x800BF690 and clear every match
```

`DrawSync`, `LoadImage` and `StoreImage` use the SDK's dispatch data at
`0x800A8838`. The image hooks receive the exact four arguments sent by the
original indirect handler: its state word, the PSX `RECT` address, the value 8,
and the image buffer address. They do not emulate VRAM or GPU packets.

## Area inputs and pixel operation

`AreaLoad` uses the 8-byte source rectangle at `0x800A47F0` (x=880, y=128,
w=16, h=128) and reads the 24-bit tint from the active area header at
`0x80154000 + header_offset + 0x24`. `AreaTexCopyInit` starts its destination
rectangle from the EXE `RECT` at `0x800A5A00` (x=576, y=256, w=16, h=128),
then writes the saved source rectangle to the 8-byte slot table at
`0x800B98D0 + index * 8`.

The differential cases load the real members for areas 0x1B / 0x097,
0x1C / 0x099, 0x1E / 0x09F and 0x22 / 0x0AA. These have the area texture-copy
flag set. Their tint words are respectively `0x00000000`, `0x00000000`,
`0x00808080` and `0x008C8C8C`.

Each nonzero 16-bit source pixel retains bit 15. Its five-bit RGB channels are
expanded by shifting left three, moved toward the corresponding unsigned tint
byte by `weight / 8`, clamped to 0..255, then packed back to RGB555. The MIPS
sequence adds 7 before an arithmetic shift for negative deltas, which is
truncation toward zero. The resulting shade weights are 1/8 through 7/8.

## Port status and validation

Default run: `.venv/Scripts/python.exe tools/mips/check_tex_copy.py --coverage`
(`--seed 94423`, 42 differential cases). The checker compares returns, all RAM
below the declared top-of-RAM stack window, scratchpad bytes and ordered hook
calls including the image payload hashes.

| Function | Cases | Instruction coverage |
|---|---:|---:|
| `DrawSync` | 4 | 26/26 |
| `LoadImage` | 4 | 24/24 |
| `StoreImage` | 4 | 24/24 |
| `SYS_OBJ_2774` | 7 | 8/8, plus its shared tail 30/30 |
| `SYS_OBJ_2794` | 4 | 6/6 |
| `SYS_OBJ_27AC` | 4 | 31/31 |
| `SYS_OBJ_28A0` | 2 | 4/4 |
| `AreaTexCopyInit` | 8 | 173/177; four proven unreachable |
| `HeapFree` | 5 | 14/14 |

In `AreaTexCopyInit`, `8005E978`, `8005E998` and `8005E9B8` clamp a negative
interpolated channel to zero. The source is 0..248, the tint is an unsigned
byte, and the weight is 1..7, so interpolation cannot be negative. `8005EA2C`
adds 3 before signed division for a negative frame number; the loop uses only
positive frame numbers. These four instructions are registered in
`tools/mips/check_exclusions.csv`. The checker also declares the stack window
because the MIPS save area and the native C stack have different layouts.

The mutation run contains 38 non-equivalent C mutations (at least four per
table entry); all 38 were caught.

`.venv/Scripts/python.exe tools/check_verified.py` recognizes all 575 diffed
ledger entries and no WIP rows, but currently exits with five duplicate-name
errors for `ae_GimmickSwingCarryPoint`, `ae_GimmickSwingAngle`,
`ae_GimmickSwingFirstStep`, `ae_GimmickSwingAdvance` and
`ae_GimmickSwingProbeWall` in the existing `ae_gimmick_a.c` / `ae_gimmick_b.c`
rows. It also reports 29 unrelated missing checker-result warnings. None of
the errors involve the nine texture-copy rows; the ledger is append-only for
this task, so those existing records remain untouched.

## Hooks left open

* `HeapAlloc` (8008C09C) allocates the two 4096-byte RAM buffers.
* `SYS_OBJ_23C4` (8002A714) services the GPU command queue.
* `SYS_OBJ_28B0` (8002AC00) initializes the SDK's GPU wait state.
* `SYS_OBJ_28E4` (8002AC34) performs one timer / GPU wait step.
* `SYS_OBJ_3E4` (80028734) checks and reports invalid GPU rectangles.
* `Printf` is the graph-debug callback used by `DrawSync`.
* `gpu_draw_sync`, `gpu_load_image` and `gpu_store_image` are native-renderer
  boundaries; they receive the original low-level call arguments above.

`SYS_OBJ_28A0` is an epilogue entry (`lw $ra`, `lw $s0`, `jr $ra`), not an
independent operation. The standalone C entry takes the incoming return word
as `preserved_return` and returns it unchanged. Differential cases verify both
zero and nonzero values, since the MIPS epilogue does not write `$v0`.
