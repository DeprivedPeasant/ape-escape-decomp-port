"""Read an immutable cloud player revision into ignored local build inputs.

Does not switch/merge branches, edit src/player, or download dependencies.
Fetch the desired ref separately, then pass it here. Local module directories can
instead be supplied to build_native_viewer.py --player-source-dir.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_REF = 'origin/claude/gallant-bohr-ms36p2'


def stage(ref=DEFAULT_REF):
    commit = subprocess.check_output(['git', 'rev-parse', '--verify', ref + '^{commit}'], cwd=ROOT, text=True).strip()
    out = ROOT / 'extract/codex/cloud-player' / commit
    files = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', commit, 'src/player'], cwd=ROOT, text=True).splitlines()
    assert files, 'No native player module in requested revision'
    manifest = {'commit': commit, 'ref': ref, 'files': {}}
    out.mkdir(parents=True, exist_ok=True)
    for name in files:
        relative = Path(name).relative_to('src/player')
        target = out / relative
        data = subprocess.check_output(['git', 'show', commit + ':' + name], cwd=ROOT)
        if target.exists():
            assert target.read_bytes() == data, 'Immutable snapshot was modified: ' + str(target)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        manifest['files'][relative.as_posix()] = hashlib.sha256(data).hexdigest()
    (out / 'source.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode())
    for name in ['native_player.c', 'native_player.h', 'native_player_body.c', 'native_player_body.h', 'native_player_volume.c', 'native_player_volume.h']:
        assert (out / name).is_file(), 'Required source missing: ' + name
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', default=DEFAULT_REF)
    args = parser.parse_args()
    print(stage(args.ref))
