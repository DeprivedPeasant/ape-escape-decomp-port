# Native sky-prop rendering (opt-in, AEVIEW3)

Status: independently reviewed, compiled and validated by Codex (see the end). Opt-in only. Existing AEVIEW1/AEVIEW2 packs and
their drawing are unchanged. This is a host OpenGL approximation of the original draw order and depth: no full GTE,
OT or fog-colour parity is claimed, and the original scene's appearance is not claimed to be reproduced. The type 3
cyan cloud cards are deliberately not sky types (native_sky.md) and are not altered; they are not necessarily fixed.

## Use

```
uv run --offline python tools/codex/build_native_viewer.py --area 0x05 --stage-camera --indicator-proximity --sky-props --keyboard-only --run
uv run python tools/codex/pack_area.py 0x05 --sky-props
```

`--sky-props` implies `--prop-materials` and does not change movement flags. The viewer needs no extra flag: it detects
the pack version. Output files are distinct (`area_XX_sky.aev` / `.json`), so the v1 (`area_XX.aev`) and v2
(`area_XX_materials.aev`) packs are never rewritten by it.

## AEVIEW3 layout

Same 64-byte header as v1/v2 with magic `AEVIEW3\0` and version 3. Batch records are 28 bytes, seven little-endian u32:
`first, count, texture, kind, semi_texture, flags, sky_type`. Vertices (8 floats) and textures follow as before.

- `sky_type`: 0 for normal batches, 4 or 5 for the original special sky props only. Anything else, or non-zero on a
  non-prop (`kind != 1`) batch, is rejected by the loader. `flags` keep the v2 meaning (same mask 0x1F, no reuse).
- The sky type takes part in the packer's batch key, so triangles of different policies never merge into one batch.
- v1/v2 batches load with `sky_type = 0`.

## Geometry

For each type 4/5 placement (from `sky_props.load(member)`, mapped by placement index) the packer takes the actual
source triangle vertex indices and raw signed-16 vertices, not the OBJ coordinates, and applies the original
`MeshTransformRot90` quarter turn `(rotation >> 10) & 3` with 16-bit wrap once, in Python (q0 `x,y,z`; q1 `z,y,-x`;
q2 `-x,y,-z`; q3 `-z,y,x`). No placement translation is applied (the original ignores placement x/y/z for 4/5). The
stored vertex is `x/256, -y/256, z/256` of the quarter-turned values (Y unchanged by the player offset, which happens at
run time). OBJ UV and RGB modulation and the source semi/blend fields are preserved from the existing path. The packer
checks mesh index per placement and that the raw metadata triangle count of every sky placement equals its source
triangle count, rejecting a mismatch. Terrain, world type 3 props and colliders keep their vertices. Bounds are computed
from the stored vertices; the focus terrain bounds are unchanged. The report adds `sky_placements` / `sky_triangles`.

Validated: all 11,170 sky triangles preserve source corner order, UV and RGB. Their packed positions match original pre-GTE quarter-turn instructions.

## Run time (viewer)

`scene.vertices` stays the immutable stored (quarter-turned, before-player-offset) geometry. A background pass runs before
the ordinary world loop and draws only sky batches:

- `glPushMatrix` / `glLoadIdentity`, mirror X as for the world, then the same `camera.pitch` / `camera.yaw` rotation
  but no distance and no target translation (original: view rotation kept, zero view translation).
- For each sky batch an owned scratch vertex buffer (sized to the largest sky batch, checked allocation) is filled from
  the immutable vertices: stored `x,-y,z` x 256 are converted back to signed 16 bits with a range check (no
  implementation-defined cast; also checked at load), then `ae_sky_vertex(type, 0, playerY, in, out)` applies the
  runtime Y offset (rotation 0 because the quarter turn is already in the pack), and the result returns as
  `x/256, -y/256, z/256`.
- `playerY = wrap16(round(-actor_position[1] * 256))`: `actor_position[1]` is render-up = -PSX Y / 256, the raw native
  actor convention. Without an actor, host policy: `playerY = 0`. `camera.target` is not used (it includes eye anchors
  and offsets). Type 5 ignores playerY.
- Host depth approximation: depth test off and depth write off for the pass (the original pins the batch to the far
  background OT bucket `+0x3FFC`). Fog parameter 0 is implied: the viewer has no fog. Colour, 5-bit quantisation and
  within-bucket order remain approximate. Existing texture, split semi layer and blend logic is reused unchanged through
  one shared batch-draw helper; `--untextured` still draws opaque once.
- Restored before the world loop: depth test on, `GL_LEQUAL`, depth mask true, modelview (`glPopMatrix`), blend state,
  and the main vertex/texcoord/colour array pointers. The world loop skips sky batches, so a legacy v1/v2 draw sequence
  is identical. Colliders are never blended. New error branches return `fail`.
- The capture depth-coverage statistic does not count sky (no depth write).

## Not claimed

Original GTE projection / ordering table / fog colour parity; the original scene's visual match; fixing the cyan cloud
cards; any change to input or the 30 Hz phases.

## Codex validation

Strict default/player builds pass. All 101 areas / 79 placements / 11,170 sky triangles match source metadata and 4,513 original coordinate keys. All 202 frozen v1/v2 packs remain byte identical; ordinary geometry, textures, UV and RGB remain identical. Twelve real GL cases cover zero camera translation, semi blending and world depth/matrix/pointer restoration. Seven malformed packs are rejected, including fractional/out-of-range sky coordinates. See validation/sky_render_validation.md.

540 walking/world frames exactly match prior output; legacy pixels are identical in three areas. Three untextured captures and three combined stage-camera/proximity launches pass. See validation/sky_demo_validation.md. Source-backed integer helpers have their own original-path report in validation/sky_validation.md.

Sky-only GL probes deliberately write no depth. The legacy capture qualification reports no meaningful geometry but saves the image; those probes verify zero depth coverage and exact expected RGB rather than relaxing the product guard. Captures with ordinary visible world geometry pass normally.

The stage-camera Fossil capture displays sky behind the level, but a cyan card boundary remains visible. The elevated survey camera looks down into the sky dome's lower gradient; neither view is claimed to match original pixels. Original fog/depth cue on ordinary type3 cards and full projection/OT behavior remain unresolved.
