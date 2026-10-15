# native_indicator_world

Optional presentation of the area's world indicators (hint markers) in the native
viewer. It wires the already-derived slot state (`native_indicators`: register
0x800716EC and the animation slice of IndicatorsUpdateDraw) to prepared actor
assets. Generated validation is in validation/indicator_world_validation.md.

## Launch

```
uv run --offline python tools/codex/build_native_viewer.py --area 0x01 --world-indicators --run
```

or directly: `native_viewer.exe area.aev ... --collision-exe SCUS_944.23 --indicators-pack extract/codex/native/indicators_01.aei`.
Not on by default; without the flag the viewer behaves as before (the sources are
only linked into every build). `pack_indicators.py` writes the pack and the
`member_offset.aea` files it names (the actor files must sit in the pack's directory).

## Pack format (AEIND1, little endian)

Header 16 bytes `<8sII>`: `AEIND1\0\0`, area, count (<= 4). Then `count` records of
84 bytes `<hhhhhHII64s>`: kind s16 (+0), angle s16 (+2), position s16 x3 (+4),
unused u16 (+10, ignored), source object u32 (+12), hint sector u32 (+16), actor
file basename char[64] (+20). File size must be exactly `16 + 84 * count`.

Load checks: magic, area equals the scene area, exact size, count <= 4, kind 0..2,
basename null terminated within 64 bytes and made of printable ASCII without `/`,
`\`, `:` or `..` (joined to the pack's directory, bounded to 512 bytes), the
original EXE math tables (`ae_camera_math_load`), and for each record its own
`AeActor` with exactly one `indicator10_loop` (29 frames) and
`indicator10_activate` (60) for kinds 0/1, or `indicator11_loop` (58) and
`indicator11_activate` (30) for kind 2. Extra clips are allowed; missing, duplicated
or wrong-length required clips fail. Any failure releases everything and leaves
`*out == NULL`.

Registration calls `ae_indicator_register` with the original tables, the 8 byte
position (3 halfwords, zero padding halfword) and the sign-extended angle. The
saved hint sector is exposed for manual preview; source object remains metadata.

## Animation and drawing

`ae_indicator_world_tick` calls `ae_indicator_step` once per slot (one logical
30 Hz game tick); it never starts an activation. Slots loop until an explicit
activation request. Draw does not advance anything. Raw slot frame -> clip: `raw < loop frames`
selects the loop clip at `raw`; otherwise the activation clip at
`raw - loop frames`; beyond the activation length draw fails closed (error; earlier valid entries may already have drawn). The viewer treats that as a fatal error.

Placement: `(x, -y, z) / 256`, yaw `180 + atan2(dirSin, dirCos) * 180/pi` degrees
about +Y, which is `Ry(angle + 180)` equal to the original
`MatrixFromDirXZ` `[-cos 0 -sin; 0 4096 0; sin 0 -cos]` after the Y-flip the actor
vertices already carry. The yaw accessor is geometric; draw preserves the actual
quantized sin/cos entries in a column-major OpenGL matrix instead of normalizing
them through a floating yaw. This retains their small horizontal scale error. Draw uses `ae_actor_pose` then `ae_actor_draw` and enables
the three client arrays first (the actor sets its own pointers).

Viewer: world draws after the scene and before Spike, depth tested like the rest
(also in the full-scene inspector). Textures upload after the GL context exists;
the world is freed before the context is destroyed.

## Clock

The viewer ticks from its own double accumulator fed by the main loop `dt`
(clamped to 0.1 s like the UI clock), one world tick per 1/30 s, independent of
player on/off and pause. `--walk-test` and `--ui-test` use fixed dt = 1/30, so each
loop iteration is exactly one tick and the first drawn frame is raw frame 1.
Render-only captures (`--capture`, `--frames` without those) use real elapsed time:
the first iteration usually has no tick yet, so the first drawn frame is raw 0 and
later frames depend on wall time. Use the fixed-dt modes for reproducible output.
The `--walk-test` trace gets `"indicator_frames":[raw frame per slot]` after the
`contacts` field when the pack is loaded (`[]` for a zero-count pack).

## Limits (not modelled)

Kind 1's original draw flag 0x40 is omitted (documented, not guessed). No shadows,
GTE frustum culling or sorting, automatic trigger activation, CD hint logic,
save progress, pickups or time attack interaction. Each record loads its own actor,
whereas the original shares one model frame global per model A / B; the world draws
each slot's own frame.

## Manual hint preview

With indicators loaded, H selects the nearest kind0/1 marker in viewer world
space, opens its actual packed hint sector and runs its original local activation
state plus sound 0xF. The nearest selection has no proximity threshold: this is
an explicit preview control, not original collider contact or progress logic.
An active message box or already-activating marker blocks preview. Without a
world pack, H retains the existing selected-sector preview. Brackets still select
arbitrary hint sectors; Enter advances text and Backspace closes the box.

For reproducible startup preview add `--indicator-hint 0` to the build script
with `--world-indicators`. The viewer accepts the same flag with an explicit UI
pack. Invalid indices, missing hint sectors and a busy UI reject the request.
The UI opens immediately from locally prepared hints; no CD readiness is emulated.
Player mode 0xB, stage transition, progress and pickups remain unimplemented.
Player movement continues while preview text is open.

`ae_indicator_world_activate` is a separate API returning local state plus
unexecuted engine requests; it does not implicitly open messages or spawn items.

## Validation

check_indicator_world.py loads all101 areas /115 indicators and checks20,700 idle
frames plus13,800 activation clip frames. Strict default/player builds and15
malformed loader cases pass. Two90-frame SDL walking runs repeat exactly; a
90-frame manual hint preview checks marker0's full activation and return to idle,
while other markers continue looping. Invalid index and busy UI previews reject.
These are native presentation checks, not original console capture comparisons.
