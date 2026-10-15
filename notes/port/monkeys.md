# Catchable monkeys (native demo)

Code: `src/monkeys/native_monkeys.{c,h}` (pure C), viewer integration in `src/app/native_viewer.c`, packing in
`tools/codex/build_native_viewer.py`. Check: `uv run python src/monkeys/check_monkeys.py`.

The goal here is a playable feature, not a faithful AI. Data is original, behaviour is an approximation.

## Original data (decoded, notes/05-monkeys.md, notes/15-monkeys-exits.md)

- Monkey count, spawn node candidates (+0x08), patrol groups (+0x88), nav nodes (+0x488, 0x18 bytes: flags, x/y/z)
  and node links from the monkey section of the area blob at +0x4E4. `ae_monkeys_load` parses these and the check
  compares count, node count, max-active and spawn positions with `tools/dump_monkeys.py`.
- Links: per node `count` (byte +0xA) u16 entries, packed in node order starting at nav + (s16 at +2) (the relocation loop
  in `MonkeySpawnAll`; the u16 at +6 that `dump_monkeys.py` mentions is not the link base). Id = low 14 bits, 0x4000 = special
  link flag (`NavNodeLinkFlag`); links are symmetric in area 0x01.
- Model: LOD0 of the monkey section (header word 2), exported to glTF (`extract/gltf/<member>_<offset>/actor.gltf`) and
  packed with `pack_actor` (`--monkey-actor`). Clips are `monkey_set0_clipNN` (NN = animation index in hex).

## Approximation (all constants in `native_monkeys.h`, marked as such)

| What | Choice |
|---|---|
| Spawn | first usable candidate (not on a moving collider / bone node), placed on the floor via the ground callback |
| Patrol | waypoints = spawn node, then all patrol-group nodes in order; BFS over links (non-special first), walk at 16 units/tick, idle 30-90 ticks at each waypoint |
| Flee | player within 2560 units (xz, ~10 m): run at 40 units/tick; at each node take the linked node farthest from the player; calm down beyond 3840 |
| Catch | `ae_monkeys_try_catch(m, pos, radius)` (3D distance) marks free monkeys caught, plays the caught clip 30 ticks, then removes them |
| Max active (4) / LOD / jumps / gadget interactions | ignored: all monkeys always update; special links are walked straight |
| Yaw | faces the walk direction with the same convention as Spike; the model's forward axis was not checked in a window |

Clips (animation index, from notes/17-monkey-states.md): walk 0 (state 0x00), flee 2 (state 0x03; state 0x02 / anim 1 is the alternative),
idle 13 (state 0x0E activity loop), caught 4 (state 0x05, non-looping; a stand-in for the real capture sequence).

## Demo

Enabled by default with `--native-player` in `build_native_viewer.py`; `--no-monkeys` turns it off (the viewer then gets no
`--monkey-actor`, so hidden check runs and traces are unchanged: the checks build their own argument lists without it).
Monkeys update on the 30 Hz native player tick only.

- **Monkey Net**: swing the right stick (arrow keys) near a monkey: while the net is in states 0x80 / 0x81 a sphere
  of radius 0xC0 centred 0xC0 units in front of Spike, 0x60 above his feet, calls `ae_monkeys_try_catch`
  (APPROXIMATION: the original catches through a type 8 hit-volume contact, MonkeyAiStateUpdate around 0x80041FB8).
- **N**: developer catch, calls `ae_monkeys_try_catch` with radius 640 (2.5 m) around Spike; this is the call the Monkey Net should use.
- The window title shows `Monkeys caught x / y`. The stdout line `MONKEYS caught ...` is printed on each N press.
- No catch sound yet: no obvious effect id in notes/29-sound-effects.md.
