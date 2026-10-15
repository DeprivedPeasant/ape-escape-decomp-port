# Overnight handoff — 7 October 2026

Final checkpoint: 08:00 Australia/Sydney implementation deadline reached; morning summary saved after08:02. Overnight follow-up paused, no active delegated workers.

The native demo now combines source-backed camera state, stage hint-marker
presentation/proximity, stage music/text and material/sky rendering. It remains
an explorable prototype; monkey capture, complete gadgets, progression and stage
completion are not implemented end to end.

## Launch

From `<repo>` in PowerShell:

```powershell
uv run python tools/codex/build_native_viewer.py --area 0x01 --stage-camera --indicator-proximity --sky-props --keyboard-only --run
```

Use area `0x05` for Molten Lava and its supported scripted camera handler. The
builder prefers local Claude's `.wt/player/src/player`, checks the packs
and opens the demo. Escape closes it; H manually previews the nearest hint.
Keyboard-only bypasses a host SDL gamepad enumeration stall. Physical controller
behavior still needs testing. See `overnight_demo.md` for the launch limitations.

## Implemented and integrated overnight

- Shared integer reverb for music and effects, with original return volume,
  existing frame clock/counter ADSR, and shared voice-budget regression checks.
  Standalone reference defaults and previously approved listening exports remain.
- Original camera follow/orbit and ground clearance, local region/slot selection,
  snapshot/activation, blending, motion/focus, local script VM and lifecycle.
  Molten Lava's indexed handler is implemented. Unsupported dependencies report
  status and hold the last valid camera rather than inventing behavior.
- All 115 known stage indicators across 101 active areas load and animate,
  including the previously unsupported one-bone assets. Local proximity requests
  advance marker animation and sound. Hint/pickup/effect requests are recorded;
  the complete contact/progress/player-mode protocol remains unimplemented.
- Opt-in source prop semi-transparency/STP layers and all four blend modes,
  including untextured draw-mode decoding and corrected untextured material
  inheritance. Special type4/5 sky props have their original transforms and a
  background render pass. Legacy packs and rendering remain available.
- A bounded Sonnet5.5 bridge delegates implementation in isolated worktrees.
  Codex reviews full source, verifies hashes/ownership, compiles and independently
  tests before integrating explicit files. No active worker remains at this
  checkpoint. Old smoke/blocked job artifacts are preserved.

## Verified foundations awaiting renderer integration

Original fog configuration/factor and primitive depth-gate helpers; restricted
GTE AVSZ/DPCS and depth-only RTPS helpers; SDK MulMatrix0/CompMatrix/ApplyMatrixLV
and table-backed SquareRoot0. Long VectorNormal reuse is verified after original
signed16 narrowing. These helpers are separate from the viewer: original
CameraLookAt and complete primitive color/depth/order integration are still needed.
See `native_camera_matrix_plan.md` for the bounded next implementation contract.

## Validation evidence

| Work | Evidence / scope |
|---|---|
| Integer reverb | 54 cases, 3,538,998 stereo frames and final RAM match pinned external ProcessReverb; all nine original presets. `sonnet/reverb_pipeline_validation.md`. |
| Shared music/effect return | 705,600 frames match the external reference; bypass/missing-preset and callback/voice-budget checks pass. Ignored `extract/codex/audio-audit/shared-reverb-validation.json`. |
| Camera follow | 5,000 whole-original follow comparisons and 20,751 ground queries; 44,829 SDK math comparisons. Camera reports beside their checkers. |
| Stage composition | 101 area records, 102 initializations and 2,940 scoped original camera ticks; unsupported dependencies explicitly excluded. `validation/camera_stage_validation.md`. |
| Indicators | 101 areas / 115 placements, 20,700 idle and 13,800 activation frame checks. Original controlled-contact proximity tests are separate from live gameplay. `validation/indicator_world_validation.md`, `validation/indicator_proximity_world_validation.md`. |
| Prop materials / sky | All 101 source packs; 18,219,008 source VRAM texel checks; 79 sky placements; original integer transform and GL state-restoration probes. Legacy pack bytes/images preserved. |
| Combined morning launch | 540 frames in repeated Fossil Field and Molten Lava walks, identical traces and images, zero camera status, marker tick every frame; Molten selects kinds4/5 and enters both regions. `validation/overnight_demo_validation.md`. |
| Renderer math foundations | 534,288 original depth-gate checks; 498,747 AVSZ and 3,142,550 restricted DPCS comparisons; 617,446 depth-only RTPS comparisons; 30,069 SDK matrix, 21,728 ApplyMatrixLV, 10,274 long normalization and 90,368 SquareRoot0 original calls. Each generated report states its domain and exclusions. |

Original-instruction tests use extracted EXE bytes with controlled service inputs.
GTE/reverb corroboration uses pinned external emulator code in ignored test-only
artifacts. Neither establishes complete console gameplay, hardware mixing or scene
parity. Combined SDL audio tests use the dummy driver; they do not establish
listening quality or physical-device behavior.

Focused integration recheck:

```powershell
uv run python tools/codex/check_overnight_demo.py
```

The full new Time Station candidate is local and ignored:
`extract/native_audio/136_native_integer.wav` (about 186 seconds). Its new integer
reverb mix still needs human listening; the earlier held-note fix was approved.
Screenshots/traces for the combined launch are under
`extract/codex/native/overnight_combined_*`.

## Remaining blockers and strongest next steps

1. Work with local Claude on live collision-body contact, displaced colliders and
   attachment evidence, then automatic hint/player-mode/progress coordination.
   This is the strongest route toward a playable one-area slice. Its player
   sources and capture tooling were left untouched overnight; requests/facts
   are in `notes/codex-inbox.md` with open status.
2. Capture real hard falls, ledge grabs, gadget states and contact/attachment frames.
   The new overnight 1,153-frame original capture is attract-demo area63/mode36,
   not the requested gameplay scenarios. Its supported one-frame comparisons
   do not prove continuous body simulation. Existing human captures are preserved.
3. Finish original CameraLookAt/base/roll/surface matrix composition and then
   primitive color/projection/order before wiring fog. Ordinary Fossil Field
   type3 cloud cards still show cyan rectangular boundaries; special sky props
   are a separate path. No cloud fix or console visual parity is claimed.
4. Test physical controller initialization and listen to the new Time Station
   export. Continue unsupported camera event/aim/fixed/flight work only with
   original evidence and explicit lane ownership.

All game assets, binaries, decompiler evidence, downloaded tools and external
reference sources remain ignored. Explicit-path commits preserve other-agent
changes; no push, shared branch reset/merge or global settings changes were made.
Recorded bridge client costs total approximately USD12.73 across its jobs; this
is not actual billing or total Codex usage.
