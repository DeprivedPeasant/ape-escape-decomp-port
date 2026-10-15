# Area VRAM animation list

`src/render/ae_vram_anim.c` ports the game function `AreaSetVramAnimList` (`800690E8`). Its only external operation is the public SDK call `MoveImage(rect, x, y)`, supplied through `AeVramAnimHooks.MoveImage` by the native renderer. SDK queue processing, VSync, DMA callbacks, interrupt masks and BIOS services are outside this module and have no ledger rows here.

## Area data

The input is the feature payload selected by `AreaLoad`: the area header is `0x80154000 + u32(member + 0x4D0)`, and the feature payload is `header + (feature_word >> 8) * 4` for feature id `0x20`. It contains consecutive 16-byte records terminated by a zero first byte.

A null list clears the active list and count. A non-null list stores its pointer and increments the existing count for every record. Type 2 records always call MoveImage; type 1 records call it when the second byte is also 1. The rectangle uses the record's x/y halfwords and width/height bytes; destination coordinates are the record's unsigned source-coordinate halfwords, following the original SDK argument order. The game ignores MoveImage's returned host result.

## Validation

`.venv/Scripts/python.exe tools/mips/check_vram_anim.py` runs 73 cases: three hand-built fixtures, 64 seeded random lists, and real members from areas `01`, `02`, `05`, `20`, `31`, and `45`. All 68 original game instructions execute. Both game mutations are caught; `--mutate` explicitly runs the same suite and `--skip-mutations` runs only the differential cases.

The checker intercepts the original MoveImage entry (`80028A38`) and compares its address/coordinate arguments and the transient RECT contents with the C hook. Its model supplies a nonzero return value on both sides. Main RAM outside the registered stack window (`801FF000..80200000`) and all scratchpad bytes are compared. No SDK instructions execute, and no SDK RAM masks or unreachable ranges are needed.

See [SDK boundary validation](validation/sdk_boundary_validation.md) for removed rows, hooks and the full ledger audit.
