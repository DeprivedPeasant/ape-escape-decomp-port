# Model / animation engine (0x80014750–0x80020A88)

Names: `tools/symbols.csv` batches 9–10. Structs: `ModelHeader`, and the shared actor header
at the start of `Player` and `Monkey` (`tools/structs.txt`). Export: `notes/decomp/model.c`.

## Two model kinds

### 1. Skinned actor models (player, monkeys, gadgets, enemies)

The model *instance* is the actor itself; every actor starts with the same header:

| Offset | Field | |
|---|---|---|
| +0x00 | model | `ModelHeader*` |
| +0x04 | colors | CVECTOR[colorCount] |
| +0x08 | prims | primitive runs (below) |
| +0x0C | bones | 12 bytes each: vertCount u16, firstVert u16, offset s16[3], parent u16 |
| +0x10 | verts | SVECTOR (8 bytes) or 16 bytes with normals |
| +0x14 | boneMatrices | MATRIX per bone, `[0]` = root (shared buffer 0x800E9C60) |
| +0x18 | animSet | animation frames |
| +0x24 | pos | world position |
| +0x5C | animFrame | |

`ModelHeader` (0xC bytes): flags, boneCount, vertCount, colorCount, hasNormals. The data
follows in order: colors, bones, verts, prims (`ModelInit*` compute the pointers).

**Animation frames** are `(boneCount + 2)` words (frame f of an anim set starts at `f*(boneCount+2)*4`; word 0 unused
so far). Word 1 = root offset: x = bits 0-9 (signed), y = bits 10-21 (signed 12 bit), z = bits 22-31 (signed). Then one
word per bone: angle x = bits 0-9 signed ×4, y = bits 10-21 signed 12-bit ×4, z = bits 22-31 signed ×4 (PSX units, 4096 = 360°).
**Verified by rendering**: `tools/export_model.py pose 008 0xbc 0x1035c 0` poses Spike (stand animation, frame 0)
correctly with `R_bone = R_parent · Rz·Ry·Rx(angles)` and bone origin `t_parent + R_parent·offset`; a bone's `parent`
value `p` refers to bone `p-1` (0 = root), exactly as `ModelBuildBoneMatrices` indexes the matrix array.

Pipeline per frame (e.g. `PlayerAnimateAndDraw`):
1. `AnimDecodeFrame` / `AnimDecodeLerp(inst, frameA, frameB, t)` / `AnimDecodeLerpPtr`
   → angles into `g_AnimPose` (0x800F4B30).
2. `ModelBuildBoneMatrices(inst, flags)` → `RotMatrixZYX` per bone, parent-relative via
   `CompMatrix`, composed with `g_ViewMatrix`; optionally world-space copies in
   `g_BoneWorldMatrices` (0x800F4730) for attaching effects.
   (`ModelPoseBones` does the same straight from keyframes, with translation/scale tracks.)
   **Channel streams** (from codex-inbox, validated by Codex on 1,079 clips / 40,616 frames, notes/61): the
   animation pointer passed to `ModelPoseBones` (enemies, ZED, secondary idle actors, UI models, indicators) is a
   channel stream, not the fixed `(boneCount + 2)` word player frame: u32 channel count, `count` 4-byte
   descriptors (byte 0 = one-based bone index, byte 2 = 1 rotation / 2 translation / 3 scale, other bytes
   ignored), then `count` words per frame, each packed signed 10/12/10 (rotation * 4, translation as is,
   scale * 16, 4.12 fixed point). A bone without a translation channel keeps its stored offset (bone 0 too).
3. `ModelTransformSkinned` → per bone, GTE RTPT its vertex range into `g_ScreenVerts`
   (0x800E7D00). Rigid models: `ModelTransformRigid`.
4. `ModelDraw` → light vertex colors (GTE CC) into the scratchpad, then `ModelEmitPrims`.
   Models with normals: `ModelCalcEnvUVs` + `ModelDrawEnvMap` (environment mapping);
   `ModelDrawAuto` picks.

### 2. Mesh objects (level props)

`MeshInit`: header word 0 = flags (low byte = prop type, 0x1000 = verts in scratchpad,
0x100000 = vertex-color animation, 0x200000 = vertex-morph animation, 0x4000/0x8000 = loop/hold,
bits 12–15 = OT offset, 24–27 = draw flags, 28–31 = cull radius); `+0xE` colorCount,
`+0x12` vertCount, word offsets to verts / prims.

`DrawAreaProps` (called by Mode_Stage) walks the area's prop list (32-byte entries at
0x800B4D64, count 0x800F45D0):
- **visibility zones**: `GetZoneMask(pos)` for the player and the camera; a prop draws only
  if its 16-bit zone mask (+0x16) intersects;
- frustum test against the prop's radius;
- transform: `MeshTransformRot90` (props are placed with a 90° rotation step from
  inst+0x1C bits 10–11), or `MeshTransformMorph` (vertex animation, lerped with GTE GPF);
- `MeshDrawAnimColors` (DPCS between vertex-color frames) → `ModelEmitPrims`.

## Primitive lists use GPU command codes

`ModelEmitPrims` reads runs whose code byte (`& 0xFC`) is the PS1 GPU polygon command:

| Code | Primitive | Emitter |
|---|---|---|
| 0x20 | F3 flat tri | EmitPolyF3 |
| 0x24 | FT3 flat textured tri | EmitPolyFT3 |
| 0x28 | F4 flat quad | EmitPolyF4 |
| 0x2C | FT4 flat textured quad | EmitPolyFT4 → SubdivideFT4 |
| 0x30 | G3 Gouraud tri | EmitPolyG3 |
| 0x34 | GT3 Gouraud textured tri | EmitPolyGT3 |
| 0x38 | G4 Gouraud quad | EmitPolyG4 |
| 0x3C | GT4 Gouraud textured quad | EmitPolyGT4 → SubdivideGT4 |

Large textured quads near the camera are subdivided recursively (`SubdivideGT4/FT4`); the
level geometry renderer (0x80044EF4) uses `SubdivideGT4` too.

### Primitive record layouts (decoded from data, `tools/export_model.py`)

Validated on 643 actor models found in the archive members and all 4,729 area static meshes
(every vertex index < vertCount, every colour index < colorCount, runs end where the next mesh
starts). Runs are sorted by code in the order `ModelEmitPrims` handles them: FT3 0x24, FT4 0x2C,
GT3 0x34, GT4 0x3C, F3 0x20, F4 0x28, G3 0x30, G4 0x38. Bit 1 of the code byte (0x2A, 0x3E…) is
the semi-transparent flag. Byte 3 of a record is the code; vertex indices are u16 at +4,+6,+8(,+0xA).

| Kind | Stride | Layout after the indices |
|---|---|---|
| textured (FT3/FT4/GT3/GT4) | 0x1C | uv0 +0xC (u,v), CLUT +0xE, uv1 +0x10, TPAGE +0x12, uv2 +0x14, uv3 +0x16 (quads), colour index/indices +0x18 (Gouraud: one per corner; flat: one) |
| untextured (F3/F4/G3/G4) | 0x10 | colour index/indices +0xC |

Colour indices point into the model's `colors[]` (CVECTOR), which `ModelDraw` lights per frame.
Static mesh header: `+0xE` colorCount, `+0x10` vertex offset (words), `+0x12` vertCount, `+0x14`
primitive offset (words), colours at `+0x18`, 8-byte verts. Meshes with animation flags carry
extra frame data after the primitives. Area mesh table: `{count u32, {u16 wordOffset, u16}[count]}`.
Actor models are findable by scanning for a consistent ModelHeader (see `tools/export_model.py`); list and
roles in `notes/16-models.md`. With `hasNormals` the vertex block is **two arrays**: `vertCount` 8-byte positions
followed by `vertCount` 8-byte normals (4096 = 1.0, y negative-up), so the primitives start at
`verts + vertCount*16`. Textures: each primitive carries its TPAGE / CLUT (see notes/20-textures.md).
Verified visually (rest pose = zero angles, bone offsets accumulated; model space has +x pointing down the body):
member 0x008 +0xBC is **Spike** (red hair, blue trousers, boots); the area blobs' monkey LOD models (monkey section
header words 2-4) render as monkeys. Animation frames (10-bit angles) are not applied by the exporter yet.

## What this means

- **Port:** models are close to GPU packets already. A native renderer can consume the same
  data (vertices + colors + per-primitive UV/CLUT/TPAGE) with a GPU-command-compatible
  backend, or convert at load time to a modern vertex format. Subdivision can be dropped
  with perspective-correct texturing.
- **Level editor / tools:** a model exporter needs ModelHeader + bones + 10-bit packed
  animation frames + the primitive run format; props need the mesh header, zone masks and
  90° placement bits.

## Not yet named here

0x8001C918 (another env-mapped emitter), 0x8001D4A8 / 0x8001D530 (translucent? draw pair used
by RcCarDraw and PlayerDrawSquashed), 0x8001F434 (drawn when the player is in water —
ripple?), 0x8002020C / 0x800207B4, and the exact primitive record layouts inside each emitter.

## Effects and particles (survey)

- `SpawnParticle(type, pos)` → `SpawnEffectGroup(type, pos, 0x800B175C, 0x800B1828)`. Types < 0x100 spawn
  one particle (`SpawnOneParticle`; templates indexed by type from table 0x800B175C). Types >= 0x100 are
  **effect groups**: the group list pointer sits at `0x800B1428 + type*4` (types ≥ 0x1C8 use a second table at
  `0x800B1108 + type*4`); each list entry is `{kind s16, y offset s16, ...}` terminated by kind 0:
  kind 0x32 = effect object from definition `0x800B1BD4 + idx*0x54`, kind 0x33 = trail segment, any other
  kind = a particle type spawned `count + rand*scale>>8` times.
- 100 particle slots (0x34 bytes) at 0x800B9900 drawn by `ParticlesDraw` (billboards); the bytecode
  interpreter `EffectScriptUpdate` (0x80069BEC) drives emitters (loop / random loop / end / move / random offset).
- Standalone collision bodies (`CollisionBodyAlloc`, 128 x 0x38 bytes at 0x800BD1A0, flags at 0x800BEDA0; formerly
  named EffectAlloc) are hit volumes of attacks, projectiles and gadgets, registered with
  `CollisionBodyRegister(type, body*, owner)` (formerly EffectStart). The struct `CollisionBody` (structs.txt) is the
  same block that starts at an object's position (`obj + 0x24`).
