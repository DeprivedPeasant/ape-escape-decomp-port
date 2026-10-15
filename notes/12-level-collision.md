# Level geometry, collision and area data

Names: `tools/symbols.csv` batch 11. Structs: `LevelGrid`, `Collider`, `AreaHeader`
(`tools/structs.txt`). Per-area table and heightmaps: `tools/dump_areas.py` →
`notes/13-areas.md`, `extract/areas/`.

## Where level data comes from

Each area's load list (area records, see notes/01) loads an **area data blob** to
`0x80154000` with no postprocess. `AreaLoad(area, entry)` (0x80031B60, called from
StageRequest) sets everything up from it:

```
blob +0x4D0  byte offset of the AreaHeader
     +0x4D4  object instance table (count, 8-byte entries; 0x18-byte instances)
     +0x4D8 / +0x4DC / +0x4E0 / +0x4E4 / +0x4E8  anim sets, monkey anim tables, ...
     +0x4C4 / +0x504  sound data copied for SoundBankUpload
AreaHeader (word offsets relative to the header)
     +0x00 flags (bit0 no terrain draw, bit1 no heightfield)
     +0x04 terrain grid          +0x08 mesh table
     +0x0C props                 +0x10 collision
     +0x20 fog color  +0x28 fog on  +0x2C/+0x2E fog near/far  (draw distance = near*4+0x400)
     +0x34 features[8]: id byte 0x08/0x10/0x11/0x20/0x30-0x32/0x40/0x41 switch on per-area systems
```

Verified on all 101 areas (`notes/13-areas.md`; earlier versions only read the first 51).

## Area features (`AreaHeader.features[8]`)

Each entry: low byte = id, upper 24 bits = word offset of its data from the header. `AreaLoad`
resets all feature state first, then runs one setter per entry (per-area table in notes/13).

| Id | Setter | What it is |
|---|---|---|
| 0x08 | `AreaSetZoneBoxes` | visibility zone boxes `{count u32, ZoneBox[count]}` (center s16[3], half s16[3], mask u16); `GetZoneMask` ORs the masks of the boxes containing a point; props/colliders carry a zone mask and draw/collide only when it intersects the player's or camera's (0xFFFF/0x8000 when no box matches). Used by area 0x07 (3 boxes) |
| 0x10 / 0x11 | `AreaSetIndicatorModelA/B` | the two models (skinned, with animation set) used by the type 0x22 trigger indicators; 0x10 is in 43 of 51 areas, 0x11 in a few |
| 0x20 | `AreaSetVramAnimList` | VRAM animation list for `VramScrollUpdate` (texture scrolling / copying), 0x10-byte entries ended by a zero type; up to 30 entries (area 6) |
| 0x30 / 0x31 / 0x32 | `AreaSetFeature30/31/32` | three pointers stored at 0x800F5AA8/AAC/AB0; `AreaFeature30Update` runs a small state machine on them (loads overlay member 0x180); meaning still tentative |
| 0x40 / 0x41 | `AreaSetSpecialActor` | allocates a 0xAC-byte actor model (DAT_800B4DC4) for levels 4 0x21 0xC 0x15 0x16; the player code reads it (`PlayerUpdate`, `PlayerSetMode`, `SpecialActorPlayerNear`); sets bit 0/1 of `g_AreaSpecialFlags`. Present in areas 8, 9 and a few others |

### Type 0x22 trigger indicators

`Gimmick_ProxTrigger_Init` calls `Indicator_Register(kind, pos, angle)` (max 4 per area, `g_Indicators`,
struct `Indicator`); `IndicatorsUpdateDraw` (Mode_Stage) animates and draws the model (kind 0/1 = model
A, kind 2 = model B) with a shadow, frustum-culled. When the player enters the radius
`Gimmick_ProxTrigger_Update` → `Indicator_Activate`: kinds 0/1 start a CD read of archive member
0x14 plus a sector offset into a 0x800-byte heap block and switch to a hint/story state
(`StageRequest(3,...)`, `PlayerSetMode(0xB)`) — so these are most likely **tutorial/hint spots**
(tentative; the display side is the message box `MessageBoxOpen` / `MessageBoxSetText` / `MessageBoxUpdateDraw`, i.e. these are **hint / tutorial text spots**); kind 2 spawns pickups (`SpawnPickups`) instead. The 69 instances' unique ids are progress flags.

Indicator models / animations (from codex-inbox, notes/63): area features 0x10 / 0x11 hold the indicator model (+4) and a `ModelPoseBones` channel stream (+8), word offsets relative to the feature. Feature 0x10 serves kinds 0/1: idle frames 0..28, activation jumps to frame 29 and returns to idle after 88. Feature 0x11 serves kind 2: idle 0..57, activation 58, back after 87.

## Terrain = heightfield

`g_LevelGrid` (0x800EBFA0, struct `LevelGrid`), filled by `AreaLoadGridAndProps`:

```
grid data: width u16, depth u16, colorCount u32,
           cells     u32[w*d]   hi16 surface attribute; bit 11 enables the selector (cellWord >> 8) & 0x70 = bits 12-14 (triangle split variant; from codex-inbox, notes/67)
           colors    CVECTOR[colorCount]
           cellTiles 8 bytes [w*d]  (u8[4] corner color indices + 4 bytes tile/texture info)
           heights   s16[(w+1)*(d+1)] corner heights, then s16[w*d] cell-center heights
```

- **Cell word, low 16 bits = render attributes** (hi16 = surface attribute for collision). From
  `DrawTerrain` (validated by `dump_areas.py` → notes/13): bit0 cell is drawn, bit1 near/detailed
  block (`DrawTerrainBlockNear`) vs far, bit2 permutes the packet geometry together with the UV / colour slots (the world-corner
  association stays the same: corners X-Z-, X+Z-, X-Z+, X+Z+ = UV slots 0, 2, 1, 3 and colour bytes 0, 6, 1, 7; codex-inbox), bits 3-6 tile index 0-15, bits 7-9 UV orientation 0-7 (bit 10 never set; tables
  `g_TerrainUvQuad` etc. hold 4 corner (u,v) pairs, 0x00/0x3F = a 64×64 tile). The tile index
  picks both a 64×64 tile in a 4×4 atlas on TPAGE 0x18 (u += (i%4)*0x40, v += (i/4)*0x40) and the
  CLUT: `(0xF0 + i) * 0x40 | 0x37` (row 240+i, x 0x370).
- **Terrain texture atlas**: TPAGE 0x18 (VRAM x 512, y 256, 4bpp, 256×256 px = 4×4 tiles of 64×64) with a
  separate CLUT per tile index (row 0xF0+index at VRAM x 0x370). `tools/vram.py` rebuilds VRAM from the area's
  TIM lists (notes/20-textures.md); a tile sheet drawn with each tile's own CLUT shows the expected
  grass / dirt / edge pieces. Bit 2 rotates the packet corners and the uv / colour slots (1,3,0,2) together, so it does not change which world corner gets which uv / colour (see above).
- `cellTiles` bytes are **colour indices** into `colors[]` (the palette is lit by the GTE and kept in
  scratchpad 0x1F800000+idx*4; checked: every byte < colorCount in all terrain areas). Bytes 0..7:
  t0==t4, t2==t5, t3==t7 almost always, i.e. two triangles; a whole quad (GT4) uses t0,t1,t6,t7, the
  split-triangle cases use t0..t3 or t4..t7. Exact corner order still tentative.
- **Areas with `AreaHeader.flags & 3 == 3` (16 of 51) have no terrain; their grid block is junk**
  (heights spanning ±32768, colour bytes out of range). Skip them in tools.
- Cells are 0x200 units; the grid is centered on the origin (x + width*0x100).
- Y is negative-up.
- **Rendering** (`DrawTerrain`, from Mode_Stage): 2×2-cell blocks with a precomputed
  `blockMask`; near blocks detailed (`DrawTerrainBlockNear`, GT4 + `SubdivideGT4`), far
  blocks coarse (`DrawTerrainBlockFar`); fog via GTE far color.
- **Collision** (`GetGroundHeight`): picks the cell, picks the triangle from the split
  bits and quadrant, interpolates the plane (corner + center heights), returns slope
  (→ Player.slopeX/Z) and the surface attribute (scratchpad 0x1F8003F8 → Player.surfaceKind,
  hazard bit, etc.).

## Structures on top: collision shape grid + collider boxes

`GetSolidHeight` (0x8004D8C0), called by GetGroundHeight; the lower result wins.

1. **Shape grid** (`g_CollShapeGrid`, same cell size): s16 per cell = first shape in that
   column; shapes (`g_CollShapes`, 0x14 bytes) are stacked by height band (+2) and linked
   (+0x10), each with a type (+0x12) dispatched through `g_CollShapeHandlers` (0x800AF180).
   This is how bridges, platforms and multi-level buildings work.
2. **Colliders** (`g_Colliders`, struct `Collider`, 0x38 bytes, count `g_ColliderCount`):
   boxes (center, half size), quadrant-masked (`GetColliderQuadrantMaskXZ`, one bit per XZ sign quadrant), with a type byte
   (9 = **water volume**, used by `GetSurfaceType`) and a flow/current vector.

Contact results go to the structure at `PTR_DAT_800AF2E4` (type, index, surface, slope).

### Shape records and types

Shape table (`collision[1]`): `count u16`, then 0x14-byte records (verified on all 101 areas):

```
+0x02 s16 height band (units of 0x200, y negative-up)   +0x06/+0x08/+0x0A s16 params p0,p1,p2
+0x0C/+0x0E s16 extra (contact info copied to PTR_DAT_800af2e4)
+0x10 s16 next shape in the same column (-1 = end)       +0x12 u16: low byte = type, high bits flags
```

A handler is `h(type, params, &local{x,_,z}, &out)`: local is the probe position relative to
the cell centre (±0x100), `out` is the surface height above the band base; it returns 1 on
a hit and may set `g_CollContactSlopeX/Z`. p0/p2 are half extents, p1 a height. `g_CollShapeHandlers`
has 89 entries (10 handlers); the data only uses types 0-63 (`notes/13-areas.md` has counts):

| Types | Handler | Meaning |
|---|---|---|
| 0-2, 54, 60-63, 68, 78-81 | `CollShape_Box` | flat box; 0x3C-0x3F = half-cell edge boxes. Type 0 = 25,142 of ~31k shapes |
| 7-10 (64-67) | `CollShape_Ramp45` | 45° ramp over a cell, 4 directions |
| 11-18 | `CollShape_Ramp2` | 1:2 slope, 2 chained types per direction |
| 19-34 | `CollShape_Ramp4` | 1:4 slope, 4 chained types per direction |
| 35-37, 59, 69-73, 82-84 | `CollShape_Cylinder` | round pillar (35: 102 in data); 36/37 half-pipes; 70-73, 82-84 never collide |
| 38-41 | `CollShape_DiagonalBox` | half-cell triangular box, 4 orientations (~1k each; the high bits of +0x12 hold 0x400/0x800/0xC00) |
| 3-6, 42-45, 74-77 | `CollShape_WallThin` | thin edge wall, only in wall-probe mode |
| 46-53 | `CollShape_WallEdge` | edge/corner wall bands (wall-probe mode) |
| 55-58 | `CollShape_WallDiagonal` | diagonal wall (wall-probe mode) |
| 85-88 | `CollShape_Wedge` | box with a linear ramp (unused in data) |

Ramps are chained across neighbouring cells with consecutive types so the surface is continuous
(e.g. Ramp4 types 19..22 cover 0..0x200 of rise in one direction). Wall-probe mode is
`g_CollWallProbe` (set by the ledge grab and a few other probers; tentative semantics).

## Collision pass

`CollisionUpdate` (0x8007247C, Mode_Stage) walks `g_CollisionBodies` (linked list, type 1 =
player): resets `Collider.touchedBy`, then per body type runs ground probing
(`BodyProbeGround`), landing / hazards / fall damage (`BodyLandOnGround`), wall pushes
(`BodyRecordColliderContact`, records the containing collider; the push is `CollisionResolveProbes`), and the player's ledge grab (`CollisionRayTest`); finally clamps
positions to the grid bounds.

## Props

`AreaHeader.props`: count then 12-byte placements {mesh u16, rotY u16, x, y, z s16, ?}.
Instances (32 bytes, `g_AreaProps`): MeshInst + position (+0x10) + zone mask (+0x16) +
rotation (+0x1C: a 90° code, or a real matrix for mesh type 3 — max 32, "RT Model Over!!").
Drawn by `DrawAreaProps` (notes/11-model-engine.md).

## Interactive objects ("gimmicks")

Area blob `+0x4D4` → `{count u32, groups[count] (8 bytes), instances (struct Gimmick, 0x18)}`.
Each instance is tied to a **Collider** (`+0x04`), whose `propIndex` is the prop it moves or
re-meshes; `link` (`+0x0C`) chains activation (switch → door). Behaviour comes from the
per-area handler list `g_AreaGimmickHandlers[area]` = `{initFn, updateFn, group*}` per
group (in the EXE or the area's code overlay): `GimmickSpawnAll` calls init at load,
`GimmickUpdateAll` calls update every frame. `GimmickEvent` / `GimmickActivate` handle
presses, hits and chained activation; `GimmickSaveStates` / `GimmickRestoreStates`
persist them across area reloads (`g_GimmickSaveTable`).

Generic behaviours in the main EXE: switches, lift / swing / slide doors, breakables,
proximity triggers (type 0x22, 69 of them, unique ids = progress flags). Rare types and
type 0x00 use area-overlay handlers (set pieces). Per-area listing and type table:
`tools/dump_objects.py` → `notes/14-area-objects.md` (415 objects, 33 types).

## For a level editor

**Validated visually (area 1)**: `export_area_obj.py` + `preview_obj.py` reproduce the first stage: a 20x20
cell lawn with trees inside a huge backdrop ring (ocean, cliffs, sun) made of props. This confirms the
terrain tile atlas (below), the UV orientation tables, the mesh / primitive layouts (notes/11) and prop placement.

`tools/export_area_obj.py` writes `extract/obj/area_XX.obj` per area (terrain with vertex colours,
prop markers, collider boxes). It parses every area without errors; props sit on or above the
heightfield (many exactly on it), collider counts/sizes agree with the section lengths.

Everything needed to *read* a level is now located: terrain grid (heights, colors, cell
attributes, tiles), props (mesh + placement), collision shapes and colliders, fog. Still to
decode: the exact tile corner order, the
per-area feature ids and what type 0x22 triggers show. Monkeys come from the monkey
section (blob +0x4E4) and blob +0x240 is the **area exit** list — both in
`notes/15-monkeys-exits.md` (`tools/dump_monkeys.py`).

## Collision response roots and shape corrections (from codex-inbox, notes/69)

The grid / collider wall and ceiling responses dispatch through the pointer tables 0x800B20E0 / 0x800B2108 by class:
0 `CollProbe_Box`, 0x2000 `CollProbe_Ramp` (linear / overhang / wedge), 0x4000 `CollProbe_Triangle` (diagonal cut box),
0x3000 / 0x9000 `CollProbe_Radial` (pillars, climb targets, half-pipe / ring / curved slope / rope / trench helpers),
0x5000..0x8000 `CollProbe_NoContact`. Codex validated the four response roots against the original instructions.
Corrections: 257 negative-band shapes (types 0 / 38) exist in 11 areas; `GetSolidHeight` may write their contact
metadata while still returning 32767 and `GetGroundHeight` keeps those surface flags. Overhang types 64 and 66 do
occur (one each). Collider +0x10 = geometry / handler word, +0x12 = surface / contact type, solid-mode flag tested by
`ColliderTypeActive` = +0x14 bit 15 (distinct from the disabled flag +0x18 bit 15).
Grid band responses (codex-inbox 17:20): classes 0x1000 / 0x5000 `CollProbe_ThinWall`, 0x6000 / 0x7000 `CollProbe_EdgeWall`,
0x8000 `CollProbe_DiagonalWall`; collider class 0x1000 for geometry types 74-77 `CollProbe_Strip`. `CollProbe_HalfPipe`
handles types 36 / 37 and is the fallback of the trench types 83 / 84; `CollProbe_Slope` covers the curved quarter-turn
ramps 70-73; `CollProbe_Rope` never pushes; `CollProbe_Radial` types 59 / 89 return status 2 (climb targets).

