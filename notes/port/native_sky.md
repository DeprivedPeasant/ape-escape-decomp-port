# Native sky props (mesh type 4 / 5)

Status: independently compiled and validated by Codex; see validation/sky_validation.md. Renderer integration is pending.

## Original facts (DrawAreaProps 0x80017094..0x8001717C, as supplied by the coordinator)

- A mesh whose header flags low byte is 4 or 5 runs a special path instead of the normal placement draw.
- `SetTransMatrix(0x800AE840)`: the matrix is identity rotation with all-zero translation. The existing view rotation
  is kept. Scratch offsets x/y/z (0x3C8/0x3CA/0x3CC) are cleared to 0.
- Type 4 sets offset Y = `-signed16(g_Player.posY)`, stored as 16 bits. Type 5 uses Y = 0.
- Calls `MeshTransformRot90(instance, out)` then `ModelEmitPrimsAlt(otBase + 0x3FFC, out, instance)`; the dispatcher
  flag 7 pins the OT bucket (background).
- The normal view translation is restored after each prop; fogParam is 0 during the group and restored afterwards.
- Placement x/y/z are ignored. The quarter turn is `(instance+0x1C >> 10) & 3` (high bits, rounded down; source
  rotations need not be multiples of 1024). This differs from the OBJ exporter, which uses generic trig.

## MeshTransformRot90

Raw signed-16 mesh coordinates: q0 `(x,y,z)`, q1 `(z,y,-x)`, q2 `(-x,y,-z)`, q3 `(-z,y,x)`, plus offsets, each result
stored as 16 bits with wrap (not clamp). Type 4: `Y = wrap16(y - wrap16(playerY))` (playerY = -32768 negates to 32768,
wraps to -32768, then the sum is stored 16-bit). Type 5: `Y = y`.

## API

`native_sky.h` (C11, pure, no game data):

- `ae_sky_vertex(type, rotation, playerY, in[3], out[3])` returns 1, or 0 for type other than 4/5 or NULL (out
  unchanged). `in`/`out` may alias. Wrap uses modular uint32 arithmetic, no out-of-range signed cast.
- `ae_sky_quarter_turn(rotation)`, `ae_sky_policy(type, &policy)` (no placement, zero camera translation, no fog,
  background bucket).

`sky_props.py`: `load(member)` returns type 4/5 placement records only: `prop_index`, `mesh_index`, `mesh_flags`,
`type`, `rotation` (raw u16), `original_quarter_turn`, `placement` (signed tuple) with `placement_ignored = True`,
`vertex_count`, `vertices` (raw signed-16 tuples), `triangles` (raw vertex index triples in source primitive order;
quads emit (0,1,2) then (2,1,3)). Meshes are validated through `prop_materials.parse_mesh` for every placement with a
valid mesh index (placements with an out-of-range index are skipped, as in the OBJ exporter and prop_materials); any
malformed source raises `ValueError` with no partial result. Duplicate placements each yield a record; zero-primitive
meshes give empty triangle lists. CLI prints per-member counts.

## Position convention

PSX signed units, int16, as stored in the mesh. No scaling to OBJ units.

## Not ported / not claimed

- GTE projection, OT insertion and ModelEmitPrimsAlt are still unported; no GPU parity is claimed.
- Observed: Fossil member 0x05B prop 6 / mesh 6, type 4, flags 0xF0080004, placement (0,-976,0), rotation 0.
- The cyan cards (props 7/8/9, type 3, flags 0xF0081003) are deliberately excluded; this helper does not fix all
  cloud appearance.

## Validation required (Codex)

Strict compile, all 101 source areas / 79 sky placements, 2,000 original policy-loop cases and 14,705 original pre-GTE vertex cases (plus the same in-place checks) pass. Nine failure guards pass. See check_sky_props.py and sky_reference.py. Projection/visibility/complete emitters remain outside this proof; renderer integration is next.

## Blockers

None for the files here. Placement record layout `<HHhhhH>` and mesh header offsets were taken from
`export_area_obj.py`/`prop_materials.py`; the type = low byte of the mesh flags word follows the supplied evidence and
is checked against all actual extracted areas by Codex.
