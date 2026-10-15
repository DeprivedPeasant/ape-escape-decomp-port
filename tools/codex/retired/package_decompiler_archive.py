"""Snapshot all local text decompiler exports for the authorized private repo.

Read-only source: notes/decomp. Never copy binaries or modify original exports.
Historical names and failures are retained and indexed rather than corrected.
"""
import csv
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'notes/decomp'
OUT = ROOT / 'extract/cloud_evidence/archive'
ALLOWED = {'.c', '.h', '.txt', '.csv', '.tsv', '.log', '.md', '.json'}
SECRET = re.compile(rb'(gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|sk-ant-[A-Za-z0-9_-]{20,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)')
HEADER = re.compile(r'/\*\s*(?:====\s+)?([A-Za-z_][\w]*)\s+@\s+([\w:]+)[^\n]*')
REQUIRED = {'CollisionUpdate': '8007247c', 'BodyProbeGround': '80072d28',
            'BodyLandOnGround': '80072dcc', 'GetGroundHeight': '80043400',
            'GetGroundHeightEx': '800433b0', 'GetSolidHeight': '8004d8c0',
            'PlayerAct_Land': '80066770', 'PlayerUpdate': '80060780',
            'Mode_Stage': '8005bf70', 'CollisionResolveProbes': '80073450'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-movement', action='store_true',
                        help='Write roots for the fresh private Ghidra movement supplement')
    args = parser.parse_args()
    if args.prepare_movement:
        roots = set()
        for line in (ROOT / 'tools/symbols.csv').read_text().splitlines():
            if not line or line.startswith('#') or ',' not in line:
                continue
            address, name = line.split(',', 2)[:2]
            if '::' not in address and (name.startswith(('Collision', 'Body', 'Coll', 'GetGroundHeight',
                                                        'GetSolidHeight', 'GetTerrain')) or
                                       name in ('Mode_Stage', 'PlayerUpdate', 'PlayerAct_Land',
                                                'PlayerAnimateAndDraw', 'PlayerSetState')):
                roots.add(address)
        target = ROOT / 'extract/codex/movement-evidence-roots.txt'
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('\n'.join(sorted(roots)) + '\n')
        print(json.dumps({'movement_roots': len(roots), 'file': target.relative_to(ROOT).as_posix()}))
        return
    OUT.mkdir(parents=True, exist_ok=True)
    copied, skipped, index = [], [], []
    timestamp = datetime.now(timezone.utc).isoformat()
    for path in sorted(SOURCE.rglob('*')):
        if not path.is_file():
            continue
        rel = path.relative_to(SOURCE)
        if path.suffix.lower() not in ALLOWED:
            skipped.append(rel.as_posix())
            continue
        data = path.read_bytes()
        assert b'\0' not in data, f'Binary input blocked: {rel}'
        assert not SECRET.search(data), f'Credential pattern blocked: {rel}'
        text = data.decode('utf-8-sig').replace('\r\n', '\n').replace('\r', '\n')
        canonical = text.encode('utf-8')
        dst = OUT / 'exports' / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(canonical)
        headers = list(HEADER.finditer(text)) if path.suffix == '.c' else []
        for i, m in enumerate(headers):
            body = text[m.start():headers[i + 1].start() if i + 1 < len(headers) else len(text)]
            bad = bool(re.search(r'decompile failed|DECOMPILE FAILED|decompilation failed|halt_baddata|WARNING: Bad instruction', body))
            index.append({'name': m[1], 'address': m[2],
                          'file': (Path('exports') / rel).as_posix(),
                          'line': text.count('\n', 0, m.start()) + 1,
                          'reference_only': 'true', 'warning_marker': str(bad).lower(),
                          'fresh_movement_export': str(rel.parts[:2] == ('codex', 'cloud-movement')).lower()})
        copied.append({'source': path.relative_to(ROOT).as_posix(),
                       'file': dst.relative_to(OUT).as_posix(), 'bytes': len(canonical),
                       'source_sha256': sha(data), 'sha256': sha(canonical),
                       'source_mtime_utc': datetime.fromtimestamp(path.stat().st_mtime, timezone.utc).isoformat(),
                       'indexed_function_sections': len(headers)})
    with (OUT / 'functions.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=['name', 'address', 'file', 'line', 'reference_only',
                                               'warning_marker', 'fresh_movement_export'])
        writer.writeheader(); writer.writerows(index)
    fresh = list(csv.DictReader((OUT / 'exports/codex/cloud-movement/functions.tsv').open(), delimiter='\t'))
    covered = {r['name']: r for r in fresh}
    for name, addr in REQUIRED.items():
        assert name in covered and covered[name]['address'] == addr
        assert covered[name]['decompile_ok'] == 'true'
    assert all(r['decompile_ok'] == 'true' for r in fresh)
    manifest = {'created_utc': timestamp,
                'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'authorization': 'User authorized all existing text decompiler output in private GitHub; COORDINATION.md Claims.',
                'snapshot_scope': 'All allowlisted text files present recursively under notes/decomp at snapshot time.',
                'normalization': 'UTF-8 without BOM and LF newlines; original and packaged SHA256 recorded separately.',
                'freshness': 'mtime is filesystem metadata, not a guarantee of symbol/type freshness. Historical snapshots retain old names.',
                'files': copied, 'skipped_extensions': skipped,
                'file_count': len(copied), 'total_bytes': sum(r['bytes'] for r in copied),
                'indexed_function_sections': len(index),
                'function_sections_with_warning_markers': sum(r['warning_marker'] == 'true' for r in index),
                'fresh_movement_function_count': len(fresh), 'required_movement_coverage': REQUIRED}
    manifest['fresh_export_settings'] = {
        'ghidra': '12.1.2, private ghidra-projects-codex/ApeEscape, SCUS_944.23',
        'mode': 'noanalysis, readOnly, ExportPlayerEvidence.java, timeout 120 seconds per function',
        'root_count': sum(r['role'] == 'root' for r in fresh),
        'roots': [{'address': r['address'], 'name': r['name']} for r in fresh if r['role'] == 'root'],
        'selection': 'Roots from --prepare-movement plus all initialized direct callees; not recursive closure.'}
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    readme = ['# Private decompiler evidence archive', '',
              'Read-only snapshot of all existing text exports under local notes/decomp, plus',
              'a fresh movement/collision supplement from the private Codex Ghidra project.',
              'User authorized upload to the private repository; game binaries, assets, BIOS,',
              'Ghidra databases, and third-party tool installations remain excluded.', '',
              '## Find code without loading the whole archive', '',
              'Search functions.csv by function name or address; file and line identify the section.',
              'Historical exports contain duplicate versions and outdated names/types. Resolve names',
              'through tools/symbols.csv, structures through tools/structs.txt and prototypes through',
              'tools/protos.txt. Prefer the fresh movement supplement or the player package when present.',
              'Do not treat successful decompilation as proof of correct types or reconstructed behavior.', '',
              '## Movement work: essential missing dependencies', '',
              'PlayerUpdate converts fixed-point movement to body velocity and retains fractions.',
              'The actual body position/collision integration occurs separately in CollisionUpdate.',
              'Mode_Stage shows update ordering. Ground probing and landing are BodyProbeGround and',
              'BodyLandOnGround; PlayerAct_Land alone is not the complete landing/collision path.',
              'GetGroundHeight/Ex combine terrain and solid queries; GetSolidHeight handles solid geometry.',
              'The fresh supplement includes all named Body*/Collision*/Coll* roots, ground queries,',
              'PlayerUpdate, PlayerAct_Land, animation and state helpers, and their direct callees.', '',
              '| Function | Fresh evidence |', '|---|---|']
    for name, addr in REQUIRED.items():
        readme.append(f'| {name} | [C](exports/codex/cloud-movement/{addr}_{name}.c) / '
                      f'[instructions](exports/codex/cloud-movement/{addr}_{name}.asm.txt) |')
    readme += ['', 'See fresh functions.tsv and dependencies.tsv for exact coverage boundaries.',
               'Indirect calls and other callers may need additional evidence; search the complete archive',
               'before reporting a missing helper. Some original exports have explicit failure markers or',
               'empty files: the archive preserves these, and the index flags common warning markers.', '',
               f'Snapshot: {len(copied)} text files, {len(index)} function sections, '
               f'{len(fresh)} fresh movement functions; no fresh decompiler failures.', '',
               'Generated by tools/codex/package_decompiler_archive.py. Original local files are unmodified.',
               'For cloud checkouts, use checkout-relative paths; no cloud Ghidra service is needed to read this.',
               'Do not load all exports into a single prompt. Read focused chunks and check instruction listings.',
               'Original binary differential validation and gameplay feel still require local verification.']
    (OUT / 'README.md').write_text('\n'.join(readme) + '\n')
    for item in copied:
        assert sha((OUT / item['file']).read_bytes()) == item['sha256']
    print(json.dumps({k: manifest[k] for k in ('file_count', 'total_bytes', 'indexed_function_sections',
                     'function_sections_with_warning_markers', 'fresh_movement_function_count', 'skipped_extensions')}, indent=2))


if __name__ == '__main__':
    main()
