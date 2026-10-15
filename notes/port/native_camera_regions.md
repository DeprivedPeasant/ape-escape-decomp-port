# native_camera_regions: area CAM region loader and CameraRegionTest

Scope: parse the area CAM section into 0x30-byte region records (with the
runtime name linking of `CameraResetAll`, 0x80047754) and the exact
`CameraRegionTest` (0x8004BD98). Not covered, still PENDING: region lifecycle
(`CameraRegionUpdate`), slot creation (`CameraSlotCreate`), script allocation
(`EventCamSetScript`), slot selection and blending. This text was written
without a compiler; nothing below is a test claim (see "Validation required").

## Layout (from the original instructions)

- Area blob +0x4D8: u32 BYTE offset of the CAM section (AreaLoad sets
  `g_CamData = blob + offset`). Blob size must be >= 0x4DC. Offset 0 = no CAM
  section: empty success, `cam_offset` 0.
- CAM header (16 bytes): bytes 0..2 = `CAM` (the original checks this and, if
  absent, falls back to a built-in empty block; the loader treats it as an
  error). +4 u16 region count, +0xE u16 byte offset of the name table relative
  to the CAM start.
- Regions start at CAM+0x10, stride 0x30. Fields used here: +0 s16 name index
  (runtime), +2 u16 flags, +4/+6/+8 u16 center x/y/z, +10/+12/+14 s16 widths
  x/y/z, +0x10 32-byte camera-name identifier.
- Name table: 8-byte header, u16 `name_count` at +0, then `name_count` entries of
  0x28 bytes: +0 u16 flags, +8 32-byte identifier.

## Name linking (CameraResetAll)

For each region, if `name_count > 0` the 32-byte identifiers are compared with
the entries in table order; the first match wins. Region +0 = matched index
(low 16 bits), region +2 = entry flags (replaces the on-disc flags), as done
before slots are allocated. `name_count == 0`: id/flags left as on disc. The
original prints `CameraName Error %s` for an unmatched name and leaves the
fields; the loader instead fails closed with a reason (no fabricated id). In
the original data every nonzero name table resolves. `named_count` stores
`name_count`.

The original writes `0x78` ('x') over the first CAM byte at reset; the loader
never modifies the caller's blob (the copy is the only thing patched). The
later `flags & 0xFFF0` mask and slot-index merge in `CameraResetAll` are slot
state and not done here; they do not change bits 0x6000 / 0x8300 used by the
test.

## Absent CAM and count 0

`count == 0`: the name table is not read at all (the original loop does not
touch it). This matters: a valid area such as Fossil Field has CAM +0xE = 0,
which would point at the magic and fake a count of 16707. `cam_offset` is kept
for an empty-but-present CAM. Offset 0 gives `cam_offset` 0.

## Strict bounds

Everything is checked against the blob size before dereference, using
size_t subtraction so no addition can overflow: header 16 bytes; records:
`count <= (size - off - 16) / 0x30`; name table header (8 bytes) at
`off + tbl`; entries: `name_count <= (size - tbl - 8) / 0x28`. Counted tails
are compared against what remains of the blob, not against any fixed capacity
(count is u16, so at most 65535 * 48 bytes are copied). On any failure the
caller's `AeCameraRegions` is unchanged; on success old records are released
and the new copy is owned.

## CameraRegionTest

`ae_camera_region_test(records, bytes, count, point)` returns the first index,
-1 for no hit, -2 for invalid input (NULL point, NULL records with count > 0,
bytes < count * 0x30). No sorting; count is the u16 value.

Per region in order: skip when `(flags & 0x6000) == 0x6000` or
`(flags & 0x8300) == 0x8100`. The widths are signed s16, halved by division
truncating toward zero (the original adds the sign bit then `sra 1`). Each
center is read as u16, then center -/+ half is wrapped to the low 16 bits and
sign-extended before comparing. Tests are strict (`lo < p < hi`) in the order
X (+4/+10), then Z (+8/+14), then Y (+6/+12). All reads are byte-assembled
little-endian; no unaligned dereference, no signed overflow.

## Validation required (Codex)

- Compile with strict warnings (C11).
- Differential run: 5000 original MIPS CameraRegionTest cases, including edge
  flags (0x6000, 0x8100, 0x8300 combinations) and boundary points
  (lo, lo+1, hi-1, hi), negative and odd widths, centers near wrap.
- All 101 original area blobs: load, check 85 with CAM, 78 non-empty, 246
  region records, every nonzero name table resolving, FossilField (area 1)
  count 0 with the offset derived at runtime.
- Failure cases leave the struct unchanged; truncated blobs at each bound.

## Notes

- `CameraResetAll` also sets projection 0x148; that is outside this module.
- Runtime visited flags and slot indices are mutated later by the update
  code, not here.
- The error string is a static buffer (not thread safe).
