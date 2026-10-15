# prop_materials

`tools/codex/prop_materials.py` preserves raw primitive metadata for area props. It is a read-only helper: it does not
touch `pack_area`, the viewer or the OBJ exporter.

## Why

`tools/export_area_obj.py` caches materials by `(tpage & 0x19F, clut)`. That drops the semi-transparent bit
(code bit 1) and tpage bits 5-6 (blend mode, `(tpage >> 5) & 3`).
This helper keeps those fields per triangle for the optional material-aware native renderer.

## API

`triangles(member)` loads `extract/members/<member:03X>.bin` (`dump_areas.MEMBERS`) and returns a flat list, one dict
per prop triangle, in OBJ emission order (placement order, primitive order, quads as `(0,1,2)` then `(2,1,3)`):

| key | meaning |
|---|---|
| `prop_index` | placement index in the area prop table (the `prop_<i>_mesh<m>` object) |
| `mesh_index` | mesh-table index |
| `primitive_index` | index of the source primitive within the mesh walk |
| `triangle_index` | 0, or 1 for the second triangle of a quad |
| `primitive_offset` | byte offset of the primitive record in the member |
| `code` | GPU code with the semi bit masked (`& 0xFC`) |
| `semi` | code bit 1 |
| `tpage`, `clut` | raw u16 fields; `None` when untextured |
| `blend_mode` | textured: `(tpage >> 5) & 3`. Untextured: `(draw_mode_word >> 5) & 3` (meaningful only when `semi`) |
| `draw_mode_byte` | untextured only: primitive descriptor byte at `primitive_offset + 2`; `None` when textured |
| `draw_mode_word` | untextured only: `0xE1000200 \| ((draw_mode_byte << 2) & 0x9FF)`; `None` when textured |
| `material_key` | `(tpage & 0x19F, clut)`, the exporter's cache key; `None` when untextured |

Untextured `tpage`, `clut` and `material_key` stay `None`. The untextured draw-mode source is original evidence: the
original F4 emitter's semi branch reads the byte at primitive+2, shifts it left 2, masks 0x9FF and ORs 0xE1000200 before
emitting the GP0 draw-mode word; the G4 emitter reads the same copied byte. A GPU batch-ID diagnostic identified the
FossilField cyan cloud rectangles as prop8/mesh8 untextured F4 semi primitives 15-19 (offsets 0x6E40..0x6E80,
byte +2 = 4, so blend mode `(4 << 2 >> 5) & 3 = 0`, average). It is not a CLUT/STP effect. All 256 byte values were
checked against each original F3/G3/F4/G4 packet path (1,024 total cases). Nothing is
inferred from a preceding textured primitive, its tpage, or colour.

`load(member)` additionally returns `skipped` (placements with `mesh >= nmesh`, which the exporter also skips) and the
parsed `meshes` (cached, so duplicate placements reuse one parse). Primitives are parsed by `export_model.walk_prims`.

## Validation

Malformed sources raise `PropMaterialError` rather than being partially accepted: table, header, colour, vertex and
placement ranges outside the file; `walk_prims` reporting a bad vertex/colour index or truncated record; a primitive
run that reaches end of file without a terminator. A mesh with zero primitives is accepted (the exporter emits nothing
for it). Only referenced meshes are parsed, matching the exporter.

## Scope

This is metadata preservation, not original GPU rendering parity. The optional renderer validates static STP-bit
gating against original VRAM texels. Ordering-table ordering, fog, runtime CLUT changes and the full original
emitter (GTE, fog, ordering table) are not ported. This helper only preserves
metadata; the untextured blend mode above replaces the earlier "unknown" statement for untextured semi primitives.

## Validation still required

Not run in the authoring environment (no extracts available): run over all area members, compare triangle counts and
order against the OBJ prop faces, and check `material_key` against the exporter's materials, including the cloud
materials.


Codex validation completed:101areas/391890triangles, source flags and textured material
keys agree;24542semi triangles.3962untextured faces inherit the previous OBJ material,
which is an exporter omission and must be corrected by an owned native pack adapter.
See validation/prop_material_validation.md. Optional rendering is described in native_prop_rendering.md;
untextured metadata/packet checks are in validation/untextured_blend_validation.md.
