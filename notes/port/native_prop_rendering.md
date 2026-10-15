# Source-material-aware prop rendering (opt-in, host approximation)

`build_native_viewer.py --prop-materials` (or `pack_area.py --prop-materials`) writes a distinct pack,
`extract/codex/native/area_AA_materials.aev` (+ `.json`), and the build/check/capture/run steps consume it. The default
pack (`area_AA.aev`, `AEVIEW1`) is unchanged byte for byte. The viewer needs no flag: it loads either pack version.

This is a host OpenGL 2.1 presentation of source fields. It makes **no claim of visual parity** with the original.

## What the pack carries

`AEVIEW2`, version 2, same 64-byte header. Batches are 24 bytes: `{first, count, texture, kind, semi_texture, flags}`
(u32 LE). Everything else (vertices, texture records) is as in v1; split layers are appended after the normal textures.

Raw primitive metadata (`prop_materials.triangles`) is walked with one cursor over prop faces only (`kind == 1`). Before
use the packer checks the total triangle count, the `prop_<i>_mesh<m>` group of every face, OBJ-textured vs metadata-
textured, and the OBJ material name against the raw `(tpage & 0x19F, clut)`; any mismatch is an error.

| flags | meaning |
| --- | --- |
| bit 0 | raw primitive semi-transparency bit |
| bits 1-2 | blend mode: raw tpage mode on textured semi triangles; on bit 3+4 batches the mode from the source draw-mode byte; 0 otherwise |
| bit 3 | untextured semi |
| bit 4 | untextured draw mode resolved: bits 1-2 are the actual mode `(draw_mode_word >> 5) & 3`, where `draw_mode_word = 0xE1000200 \| ((descriptor byte +2 << 2) & 0x9FF)` |

Other bits are 0. Bit 4 requires bit 3 (and so bit 0), `texture = semi_texture = UINT32_MAX`; flag 4 on textured is invalid.
Bit 3 without bit 4 is the **legacy fallback** of existing packs written before bit 4: bits 1-2 must be 0, the mode is
unknown, the batch draws opaque with the old load warning. A freshly written pack uses bit 4 for every untextured semi batch. Non-prop batches always have `semi_texture = UINT32_MAX`, `flags = 0`.

* **Untextured props** now get `texture = UINT32_MAX` and the untextured modulation scale (1, not 2). Previously they
  inherited the previous OBJ `usemtl`.
* **Textured non-semi props** keep their existing page and PNG bytes.
* **Textured semi props** get two disjoint layers decoded from the area VRAM (`prop_texture_layers.split`), deduplicated
  per `(tpage & 0x19F, clut)`: `texture` is the opaque layer, `semi_texture` the semi layer. Per texel: `0x0000` is
  alpha 0 in both; bit 15 clear and non-zero is opaque only; bit 15 set (including `0x8000` black) is semi only; RGB uses
  the existing 5-bit `<<3` expansion. Bit 15 comes from the actual CLUT entry (4/8bpp) or the raw texel (15bpp). Colour
  mode 3 is an error. Addressing follows `tools/vram.texture`, including its linear index (x past 1023 continues on the
  next row); rows are bottom-left first like `png_rgba`.

## Viewer behaviour

The loader validates counts, ranges, legal flags, extra texture indices, the kind rules (flags only on props, bit 3 only on
untextured semi, bit 4 only on bit 3 batches with no textures, textured semi needs a distinct valid 256x256 layer pair with alpha 0/255 and no texel opaque in both) and
batch coverage. Legacy v1 batches load with `semi_texture = UINT32_MAX`, `flags = 0`.

Textured view, textured semi batch: the opaque layer is drawn with blending disabled (depth `LEQUAL`, alpha test `> .01`),
then the semi layer with blending enabled. Blending applies only to semi-texel layer pixels; the alpha test keeps
transparent texels out of the framebuffer. Non-semi and untextured batches draw as before. `--untextured` draws each batch
once. Blend enable, equation, function and constant are restored before the actor, indicators, UI and colliders.

| PSX mode (B = framebuffer, F = texel) | OpenGL |
| --- | --- |
| 0: B/2 + F/2 | `glBlendColor(.5,.5,.5,.5)`, `CONSTANT_COLOR / CONSTANT_COLOR`, `FUNC_ADD` |
| 1: B + F | `ONE / ONE`, `FUNC_ADD` |
| 2: B - F | `ONE / ONE`, `FUNC_REVERSE_SUBTRACT` (destination - source) |
| 3: B + F/4 | `glBlendColor(.25,.25,.25,.25)`, `CONSTANT_COLOR / ONE`, `FUNC_ADD` |

`glBlendColor` / `glBlendEquation` come from `SDL_GL_GetProcAddress` (Windows `opengl32` exports GL 1.1 only). They are
looked up only when the scene needs them (modes 0 and 3 need the colour; mode 2 the equation) and loading fails with a
clear message if missing.

Resolved untextured semi batches (flags bits 3+4) are drawn once in the textured view, with no texture, using the mode
table above, before the normal draw call's state is left: blend enabled, `glDrawArrays`, blend left. Their mode is included
in `scene.blend_modes` so the blend procedures load, and they produce no unknown-mode warning. Colliders never texture or
blend. `--untextured` still draws one opaque pass per batch. Blend state is restored before the next batch, actor,
indicators and UI.

Legacy bit 3 batches (no bit 4) keep the old behaviour: opaque, one stderr warning at scene load, mode unknown.

The source: in the original F4 emitter's semi branch the byte at primitive+2 is shifted left 2, masked 0x9FF and ORed with
0xE1000200 to form the GP0 draw-mode word; the G4 emitter reads the same copied byte. All 256 byte values matched the
each original F3/G3/F4/G4 packet path (1,024 total cases). A GPU batch-ID diagnostic identified the cyan cloud
rectangles as FossilField prop8/mesh8 untextured F4 semi primitives 15-19 (offsets 0x6E40..0x6E80, byte +2 = 4, mode 0
average); this is not a CLUT/STP problem. Nothing is inferred from a preceding textured primitive, tpage or colour.
Only this host approximation is implemented: GTE, fog, ordering-table emission, dither and 5-bit arithmetic are not
ported, and no console pixel parity is claimed.

## Not modelled

8-bit OpenGL blending instead of PSX 5-bit arithmetic and dither, fog/depth cue, ordering-table order (the existing
source batch order and depth buffer are kept, semi layers also write depth), mask-bit behaviour, GTE precision, affine
texture mapping, and runtime CLUT changes (CLUTs are read once from the reconstructed area VRAM).

## Evidence

* PSX-SPX GPU miscellaneous (semi-transparency modes, saturation, CLUT bit 15 gating textured pixels):
  https://psx-spx.consoledev.net/ps1/gpu/misc/
* PSX-SPX VRAM (0x0000 transparent, 0x8000 valid black): https://psx-spx.consoledev.net/ps1/gpu/video-memory-vram/
* Project analysis: the private polygon emitter copies the source command word (semi bit) and the raw tpage into the
  GPU packet (EmitPolyGT4, 0x8001B588/590 and 0x8001B8FC/904; tpage at 0x8001B58C/594). No code is copied.

## Validation


Codex verification completed: strict default/player builds;101 actual area packs and
18,219,008 independent original-texel checks;101 legacy byte-equality checks;9 real viewer
GL blend cases;540 deterministic walking frames equal prior movement/world traces and
3 untextured captures. See validation/prop_render_validation.md, validation/prop_blend_validation.md and
validation/prop_demo_validation.md. The source-layer checks predate bit 4; textured blend probes and
the 540 movement frames were repeated after it. See validation/untextured_blend_validation.md for
the original packet branches, all actual untextured semi records, flags, legacy byte
equality, host GL formulas, fallback/debug behavior and malformed guards.

The updated Fossil Field capture blends the cyan cards, but rectangular boundaries
remain visible. This correction resolves the opaque fallback, not the whole cloud
appearance. Original fog/depth cue, source draw ordering and runtime geometry/color
updates need further evidence. Keep the mode opt-in.
