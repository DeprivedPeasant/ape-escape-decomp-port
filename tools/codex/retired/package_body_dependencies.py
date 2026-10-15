"""Publish reviewed text-only exports needed by the cloud body adapter."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'notes/decomp/codex/cloud-body-dependencies'
TARGET=ROOT/'extract/cloud_evidence/player/body_dependencies'

def main():
    TARGET.mkdir(parents=True,exist_ok=True);files={}
    for path in sorted(SOURCE.iterdir()):
        if not (path.name.endswith('.c') or path.name.endswith('.asm.txt') or path.suffix=='.tsv'):continue
        data=path.read_bytes().replace(b'\r\n',b'\n');data.decode('utf-8')
        (TARGET/path.name).write_bytes(data);files[path.name]=hashlib.sha256(data).hexdigest()
    functions=len(list(TARGET.glob('*.c')));assert functions==27,functions
    report=dict(functions=functions,roots=10,failed=0,source='Codex private Ghidra SCUS_944.23 read-only export; 2026-10-06',
                roots_file='extract/codex/body-dependency-roots.txt',files=files)
    (TARGET/'manifest.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    text='''# Body-adapter dependency supplement

27 fresh function exports and MIPS listings, ten explicit roots plus named direct callees.
Read original listings alongside pseudocode: historical types and globals can be misleading.
This is reference evidence, not buildable game code; no game binaries or assets included.

Roots: BodyRecordColliderContact, CollisionRayTest, CollisionBuildProbePoints,
PlayerMoveRumbleSoundUpdate, PlayerTryLedgeGrab, ColliderHalfSpaceInside,
ColliderSlopeContactDir, ColliderTrenchHeight, LoopSlotSweep, LoopSlotClearAll.
Check functions.tsv / dependencies.tsv / data_references.tsv for scope and unresolved calls.
Additional dependencies may already be in the player, local_handoff or archive packages.

Codex response: grid contact sign-extension fixed; 768 signed-ID comparisons and
16,273 full movement comparisons across 101 areas passed against original instructions.
GetSurfaceType host query: 6,203 original-instruction comparisons passed.
Cloud revision 93f8d9af32b42800c271982b7a73f93d9291d877 staged unchanged under ignored extract/.
Synthetic cloud tests passed 7,114 player / 144 body checks (no sanitizer claim).

Build original movement demo: python tools/codex/build_native_viewer.py --native-player --run
Integration smoke: python tools/codex/check_player_viewer.py
Cloud sources are read from origin/claude/gallant-bohr-ms36p2 without checkout/merge.
Viewer uses update+body at 30 Hz, original input classes, jump hold and state animation.
Body collider-volume scan and ledge-ray callbacks are intentionally missing and flagged;
player sound/effect events and pose blending/interpolation are not rendered yet.
Mode 0/no-gadget prototype only; no real gameplay capture or end-to-end equivalence claim.
Existing player_frame_capture.lua and check_native_player.py support local capture and
seeded update comparisons; they do not yet provide continuous world/body replay.
'''
    (TARGET/'README.md').write_bytes(text.encode())
    print(f'{functions} functions, {len(files)} files published to {TARGET}')

if __name__=='__main__':main()
