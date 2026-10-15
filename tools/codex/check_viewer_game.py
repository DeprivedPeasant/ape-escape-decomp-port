"""Compare hidden GL viewer frames with a standalone, SDL-free game host.

Both run the complete PlayerUpdate port on the same scripted packets. Compare all
RAM and scratchpad bytes, not just selected actor/camera fields. Artifacts stay
under ignored extract/codex-task/viewer_game/.
"""
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from codex.build_native_viewer import build, pack_monkey_actor
from codex.pack_actor import pack_actor
from codex.pack_area import pack, OUT
from codex.setup_sdl import locate


def standalone(harness='tools/codex/viewer_game_harness.c', name='viewer_game_host'):
    compiler = shutil.which('gcc')
    sources = set((ROOT/'tools/codex/viewer_game_sources.txt').read_text().splitlines())
    sources.update('src/player/'+name+'.c' for name in (
        'native_player', 'native_player_body', 'native_player_volume',
        'native_player_pad', 'native_gadget_slots'))
    sources.update(('src/app/native_player_host.c', 'src/app/viewer_game.c',
                    'src/collision/native_ground.c',
                    'src/collision/native_collision.c', 'src/camera/native_camera_math.c',
                    harness))
    headers = b''.join(p.read_bytes() for p in sorted((ROOT/'src').rglob('*.h')))
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I',
             str(locate()/'include'), '-DAE_NATIVE_PLAYER']
    cache = OUT/'viewer_obj'
    cache.mkdir(exist_ok=True, parents=True)
    objects = []
    for relative in sorted(sources):
        source = ROOT/relative
        key = hashlib.sha256(source.read_bytes()+headers+repr(flags).encode()).hexdigest()[:20]
        obj = cache/(source.stem+'_'+key+'.o')
        if not obj.exists():
            subprocess.run([compiler, *flags, '-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    target = OUT/(name+'.exe')
    subprocess.run([compiler, *objects, '-o', str(target), '-lm', '-static-libgcc'], check=True)
    return target


def run(command, log):
    result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                            text=True, timeout=90)
    log.write_text(result.stdout+result.stderr)
    if result.returncode:
        raise AssertionError(f'{log}: exit {result.returncode}\n{result.stdout}\n{result.stderr}')
    return result.stdout


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=120)
    args = parser.parse_args()
    if args.frames < 1:
        parser.error('--frames must be positive')
    output = ROOT/'extract/codex-task/viewer_game'
    output.mkdir(parents=True, exist_ok=True)
    viewer = build(ROOT/'src/player')
    host = standalone()
    actor = pack_actor()
    reports = []
    for area in (1, 2):
        scene = pack(area, sky_props=True)
        member = ROOT/f'extract/members/{scene["member"]:03X}.bin'
        baseline = output/f'host_{area:02X}.ram'
        host_log = run([host, area, member, args.frames, baseline, 'scripted'], output/f'host_{area:02X}.log')
        for repeat in range(2):
            ram = output/f'viewer_{area:02X}_{repeat}.ram'
            command = [viewer, scene['pack'], '--game', '--keyboard-only',
                       '--actor', actor['path'], '--ground', member,
                       '--collision-exe', ROOT/'extract/disc/SCUS_944.23',
                       '--player-member', ROOT/'extract/members/008.bin',
                       '--walk-test', output/f'trace_{area:02X}_{repeat}.json',
                       '--frames', args.frames, '--game-state', ram,
                       '--capture', output/f'area_{area:02X}_{repeat}.ppm']
            monkey = pack_monkey_actor(scene['member'])
            if monkey:
                command += ['--monkey-actor', monkey]
            stdout = run(command, output/f'viewer_{area:02X}_{repeat}.log')
            actual, expected = ram.read_bytes(), baseline.read_bytes()
            if actual != expected:
                differences = [(hex(0x80000000+i), a, b) for i, (a,b) in enumerate(zip(actual,expected)) if a != b]
                raise AssertionError(f'area {area:02X}: {len(differences)} different bytes: {differences[:20]}')
            assert f'GAME frames={args.frames} ram_error=0' in stdout
            assert sorted(set(line for line in stdout.splitlines() if line.startswith('GAME missing '))) == sorted(set(line for line in host_log.splitlines() if line.startswith('GAME missing ')))
        # Hidden windows must tick with neutral input even without keyboard focus.
        neutral_host = output/f'neutral_host_{area:02X}.ram'
        neutral_viewer = output/f'neutral_viewer_{area:02X}.ram'
        run([host, area, member, 8, neutral_host, 'neutral'], output/f'neutral_host_{area:02X}.log')
        neutral_command = [viewer, scene['pack'], '--game', '--keyboard-only', '--actor', actor['path'],
                           '--ground', member, '--collision-exe', ROOT/'extract/disc/SCUS_944.23',
                           '--player-member', ROOT/'extract/members/008.bin', '--frames', 8,
                           '--game-state', neutral_viewer, '--capture', output/f'neutral_{area:02X}.ppm']
        if monkey:
            neutral_command += ['--monkey-actor', monkey]
        neutral_log = run(neutral_command, output/f'neutral_viewer_{area:02X}.log')
        assert 'GAME frames=8 ram_error=0' in neutral_log
        assert neutral_host.read_bytes() == neutral_viewer.read_bytes()
        missing = sorted(set(line for line in host_log.splitlines() if line.startswith('GAME missing ')))
        reports.append(dict(area=area, frames=args.frames, repeats=2,
                            compared_bytes=len(actual), sha256=hashlib.sha256(actual).hexdigest(), missing=missing))
    (output/'report.json').write_text(json.dumps(dict(failures=0, areas=reports), indent=2)+'\n')
    # Keep the actual-run missing list reproducible alongside the checker report.
    note = ROOT/'notes/port/viewer_game.md'
    text = note.read_text()
    start = text.index('## Missing functions reached')
    end = text.index('## Changed files', start)
    rows = {}
    for report in reports:
        for entry in report['missing']:
            name, address = entry.removeprefix('GAME missing ').rsplit(' @', 1)
            rows.setdefault((name,address), []).append(f"{report['area']:02X}")
    section = ['## Missing functions reached', '',
               'Generated by `tools/codex/check_viewer_game.py` from the actual final runs. '
               'Areas 01/02 use real PlayerUpdate and PlayerAnimateAndDraw, with two '
               f"{args.frames}-frame repeats each. Counts: " +
               ', '.join(f"{r['area']:02X}: {len(r['missing'])}" for r in reports) + '.', '',
               '| Name | Address | Areas |', '| --- | --- | --- |']
    section += [f"| {name} | `{address}` | {', '.join(areas)} |"
                for (name,address),areas in sorted(rows.items())]
    note.write_text(text[:start]+'\n'.join(section)+'\n\n'+text[end:])
    print(json.dumps(dict(failures=0, areas=reports), indent=2))


if __name__ == '__main__':
    main()
