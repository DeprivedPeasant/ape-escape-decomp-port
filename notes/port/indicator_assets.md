# Known one-bone indicator assets

The shared scanner requires at least two bones. `indicator_assets.py` adds a narrow decoder for known area feature10/11 pointers, while retaining the shared primitive walker and model dictionary. It validates a one-bone header, color/vertex/normal spans, a contiguous root vertex range, parent0, and valid nonempty primitives before the animation stream. It does not expand general scanning or normalize unknown parents.

Model vertices and normals use separate 8-byte arrays, as in the existing parser. Parent0 is the root; the glTF hierarchy uses p-1 for nonzero parents. All19 actual one-bone feature models have34vertices/15colors/no normals, root parent0. They correspond to20 registrations in19areas, alongside12 other supported registrations previously excluded by whole-area load failure.

`feature_sources.py` retains accurate scanner_supported metadata and adds export_supported. It exports all71 known indicator sources directly, including19 single-bone models, with142 clips. It uses the original area load-list VRAM and direct channel ranges. `notes/63-feature-actor-bundles.md` remains generated. Prepared data stays ignored under extract.

Validation: `check_indicator_assets.py` independently reads packed channel words and computes continuous Rz*Ry*Rx geometry, then compares all1672 one-bone native clip poses. This checks asset decoding, not integer/GTE rendering rounding. `check_indicator_world.py` covers all101areas/115placements/20700idle frame checks; interactions remain separate.
