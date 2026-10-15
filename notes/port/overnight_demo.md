# Overnight demo launch

From the repository in PowerShell:

```powershell
uv run python tools/codex/build_native_viewer.py --area 0x01 --stage-camera --indicator-proximity --sky-props --keyboard-only --run
```

This combines the native player/body prototype, original local camera regions/scripts,
hint-marker animation and proximity requests, area music/text, and the optional source
material renderer with the original special sky transforms. It uses local Claude's `.wt/player/src/player` when present.
Change the area to `0x05` for Molten Lava's supported indexed camera handler. The launch
checks packs before opening the window. All assets and binaries remain in ignored
`extract/` paths; no game data is embedded in the committed sources.

Keyboard-only avoids the current host's SDL controller-enumeration stall. Omit it to
exercise the default controller path, whose physical-device behavior remains untested.
Use the viewer's displayed controls; Escape closes it. `H` manually previews the nearest
hint marker. Automatic proximity updates execute local marker animation/SFX only: hint,
pickup and effect requests are counted but not executed, and live collider touches,
progress and the full player mode protocol remain incomplete.

The material renderer is an opt-in OpenGL approximation. The original semi command and
texture STP bits are preserved, including untextured blend modes. Type4/5 sky props use camera-relative background transforms; ordinary cloud cards remain a separate path. Fog, ordering-table
behavior, GTE precision, five-bit GPU arithmetic and runtime palette changes remain
unported. Fossil Field's cyan cloud cards now blend, but rectangular boundaries remain.
The local camera prototype also leaves several event/aim/flight dependencies unsupported.
This is an explorable prototype, not a complete playable game.

Focused verification tools: `check_untextured_blend.py`, `check_prop_blending.py` and
`check_prop_demo.py`, `check_sky_rendering.py` and `check_sky_demo.py`; their generated reports live alongside them. Original full-game
hard-fall, ledge-grab, gadget and attachment captures are still outstanding.

Combined configuration recheck: `uv run python tools/codex/check_overnight_demo.py`.
The generated validation/overnight_demo_validation.md records540deterministic SDL frames with
all features and stage banks enabled in areas01/05; audio uses the dummy driver.
See overnight_handoff.md for the consolidated completed work and remaining blockers.
