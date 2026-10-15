"""Prepare roots or package the user-authorized private player evidence export.

Run --prepare, run ExportPlayerEvidence.java in the private Ghidra project with
the resulting roots file, then run this script without arguments. Game binaries
are read locally and never copied. Output is reviewed reference evidence only.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import runpy
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'extract/cloud_evidence/player'
SCRATCH = ROOT / 'notes/decomp/codex/cloud-player'
ROOTS = ROOT / 'extract/codex/player-evidence-roots.txt'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def roots():
    addresses = set()
    for line in (ROOT / 'tools/symbols.csv').read_text().splitlines():
        if not line or line.startswith('#') or ',' not in line:
            continue
        address, name = line.split(',', 2)[:2]
        if '::' not in address and (name.startswith('Player') or name in
                                   ('PadUpdate', 'PadNormalizeSticks', 'PadSampleStickHistory')):
            addresses.add(address)
    return sorted(addresses)


def tables():
    # Import the existing decoder without invoking its note-writing main().
    env = runpy.run_path(str(ROOT / 'tools/dump_player_states.py'))
    blob = env['BLOB']
    unpack = env['struct'].unpack_from
    sets = []
    referenced = set()

    def callback(table, index):
        address = env['exe_u32'](table + index * 4)
        referenced.add(f'{address:08x}')
        return {'index': index, 'address': f'{address:08x}',
                'name': env['NAMES'].get(address, f'FUN_{address:08x}')}

    for set_id in range(10):
        anim, state, trans, event = [unpack('<H', blob, 0x14 + set_id * 0x10 + k)[0] * 4
                                     for k in (0, 4, 8, 12)]
        assert (trans - state) % 8 == 0 and trans >= state
        states, transitions, events = [], [], []
        for i in range((trans - state) // 8):
            first, last, turn_tilt, move, action, flags = unpack('<HHBBBB', blob, state + i * 8)
            states.append({'state': i + (0x80 if set_id else 0), 'first_frame': first,
                           'last_frame': last, 'turn_mode': turn_tilt & 15,
                           'tilt_mode': turn_tilt >> 4, 'move_mode': move,
                           'frame_action': callback(env['ACTION_TABLE'], action) if action else None,
                           'flags': flags})
        p = trans
        while blob[p] != 255:
            assert p < event, 'Transition list did not terminate before events'
            frm, cond, to, action = unpack('<BbBB', blob, p)
            transitions.append({'from': frm, 'condition': callback(env['COND_TABLE'], abs(cond)),
                                'negated': cond <= 0, 'to': to,
                                'action': callback(env['ACTION_TABLE'], action) if action else None})
            p += 4
        p = event
        while blob[p] != 255:
            st, frame, kind, arg = blob[p:p + 4]
            assert kind in env['EVENT_KIND'], f'Unknown event kind {kind}'
            n = env['EVENT_PAYLOAD'].get(kind, 0)
            item = {'state': st, 'relative_frame': frame, 'kind': kind,
                    'kind_name': env['EVENT_KIND'][kind], 'arg': arg}
            if n:
                item['payload_signed_words'] = list(unpack('<' + 'i' * (n // 4), blob, p + 4))
            if kind in (4, 5):
                item['callback'] = callback(env['EVENT_FN_TABLE'] if kind == 4 else env['ACTION_TABLE'], arg)
            events.append(item)
            p += 4 + n
            assert p < len(blob)
        sets.append({'set': set_id, 'gadget': set_id - 1,
                     'member_offsets': {'animation': anim, 'states': state,
                                        'transitions': trans, 'events': event},
                     'states': states, 'transitions': transitions, 'events': events})
    return sets, referenced


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', action='store_true')
    args = parser.parse_args()
    if args.prepare:
        _, dispatch = tables()
        selected = sorted(set(roots()) | dispatch)
        ROOTS.parent.mkdir(parents=True, exist_ok=True)
        ROOTS.write_text('\n'.join(selected) + '\n')
        print(json.dumps({'roots': len(selected), 'dispatch_callbacks': len(dispatch)}))
        return
    functions = list(csv.DictReader((SCRATCH / 'functions.tsv').open(), delimiter='\t'))
    assert functions and all(f['decompile_ok'] == 'true' for f in functions)
    selected = {f['address'].lower() for f in functions}
    assert set(ROOTS.read_text().splitlines()) <= selected
    decoded, dispatch = tables()
    assert dispatch <= selected, f'Missing dispatch callbacks: {sorted(dispatch - selected)}'
    OUT.mkdir(parents=True, exist_ok=True)
    files = [SCRATCH / name for name in ('functions.tsv', 'dependencies.tsv', 'data_references.tsv')]
    for f in functions:
        files.extend((SCRATCH / f['file'], SCRATCH / f['file'].replace('.c', '.asm.txt')))
    for path in files:
        assert path.is_file() and path.stat().st_size > 0
        assert b'\0' not in path.read_bytes()
        shutil.copyfile(path, OUT / path.name)
    (OUT / 'state_tables.json').write_text(json.dumps(decoded, indent=2) + '\n')
    dependencies = list(csv.DictReader((OUT / 'dependencies.tsv').open(), delimiter='\t'))
    external = sorted({(r['callee_address'], r['callee']) for r in dependencies if r['included'] == 'false'})
    totals = {'functions': len(functions), 'roots': sum(f['role'] == 'root' for f in functions),
              'decompile_failures': 0, 'sets': len(decoded),
              'states': sum(len(s['states']) for s in decoded),
              'transitions': sum(len(s['transitions']) for s in decoded),
              'events': sum(len(s['events']) for s in decoded),
              'dispatch_callbacks': len(dispatch), 'missing_dispatch_callbacks': 0,
              'external_direct_calls': len(external)}
    anomalies = [{'set': s['set'], 'state': r['state'],
                  'first_frame': r['first_frame'], 'last_frame': r['last_frame']}
                 for s in decoded for r in s['states'] if r['first_frame'] > r['last_frame']]
    manifest = {
        'purpose': 'User-authorized private player reference evidence, not buildable port source',
        'game': 'Ape Escape USA SCUS-94423',
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'ghidra': '12.1.2, ghidra-projects-codex/ApeEscape, SCUS_944.23, noanalysis',
        'selection': 'All main-memory Player* functions, PadUpdate/Normalize/Sample, decoded table callbacks, and their direct callees',
        'decompile_timeout_seconds': 120,
        'source_sha256': {str(p.relative_to(ROOT)).replace('\\', '/'): digest(p) for p in
                          [ROOT / 'extract/disc/SCUS_944.23', ROOT / 'extract/members/008.bin',
                           ROOT / 'tools/symbols.csv', ROOT / 'tools/structs.txt', ROOT / 'tools/protos.txt']},
        'validation': totals,
        'table_anomalies': {'reversed_frame_ranges': anomalies,
                            'interpretation': 'Preserved exactly from member 008; do not silently reorder. Semantics need original-code/runtime verification.'},
        'external_direct_calls': [{'address': a, 'name': n} for a, n in external],
        'limitations': ['Decompiler pseudocode is not original source and may misrepresent types, aliasing, signedness or GTE operations.',
                       'Direct callee selection is not recursive closure; dependencies.tsv lists remaining calls.',
                       'Indirect calls are represented by decoded state callbacks and may include additional unresolved sites.',
                       'Assembly listings omit raw instruction bytes; binaries and Ghidra databases remain local.',
                       'Dynamic globals require reconstruction from initialization and writes; no runtime RAM dump is included.',
                       'Rendering assets, full animation tracks and original-instruction differential inputs remain local.'],
        'files': {p.name: {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in sorted(OUT.iterdir())
                  if p.is_file() and p.name not in ('manifest.json', 'README.md')},
    }
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    readme = ['# Private player reference evidence', '',
              'User-authorized exception: selected decompiler/instruction exports and decoded player tables',
              'may be committed to the private repository. All other game-data exclusions remain in force.', '',
              'Start with `80060780_PlayerUpdate.c` and `8003272c_PlayerInit.c`, then',
              '`PlayerSetState`, `PlayerRunTransitions`, and the decoded `state_tables.json`.',
              'Use `functions.tsv` to locate functions, `dependencies.tsv` to check helper coverage,',
              'and matching `.asm.txt` listings when pseudocode is ambiguous.',
              'Structs and prototypes remain in `tools/structs.txt` and `tools/protos.txt`.',
              'Initialization constants are visible in PlayerInit; do not use zero-initialized RAM',
              'as gameplay tuning values. Instruction listings also expose scratchpad offsets.', '',
              'Generated by `tools/codex/package_player_evidence.py` after',
              '`tools/codex/ghidra_scripts/ExportPlayerEvidence.java` in the private Ghidra copy.',
              'Export scratch remains under ignored `notes/decomp/codex/cloud-player/`.', '',
              '## Verified coverage', '']
    readme += [f'- {key}: {value}' for key, value in totals.items()]
    readme += ['', '## Table anomaly', '',
               'Base state 0x4D has first_frame=511 and last_frame=492 in the real member.',
               'These values are preserved; this package does not infer why the range is reversed.']
    readme += ['', '## Limits', ''] + [f'- {s}' for s in manifest['limitations']]
    (OUT / 'README.md').write_text('\n'.join(readme) + '\n')
    for name, info in manifest['files'].items():
        assert digest(OUT / name) == info['sha256']
    print(json.dumps(totals, indent=2))


if __name__ == '__main__':
    main()
