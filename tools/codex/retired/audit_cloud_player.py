"""Produce local EXE evidence and focused exports for the cloud player handoff."""
import csv
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[3]
REVIEW = ROOT / 'extract/codex/cloud-player-review'
OUT = ROOT / 'extract/cloud_evidence/player/local_handoff'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exe_path = ROOT / 'extract/disc/SCUS_944.23'
    exe = exe_path.read_bytes()
    off = lambda address: address - 0x80010000 + 0x800
    result = subprocess.run(['python', str(REVIEW / 'check_tables.py'), str(exe_path),
                             '--dump', str(REVIEW / 'surface.json')], text=True,
                            capture_output=True, check=True)
    table_check = json.loads((REVIEW / 'surface.json').read_text())
    table_bytes = exe[off(0x800A5A78):off(0x800A5A78) + 32]
    handlers = {0x80061520: 'H1 stop then buoyancy', 0x80061540: 'H2 buoyancy',
                0x80061624: 'H3 pinned', 0x8006163C: 'H4 normal gravity'}
    mapping = [{'state': f'0x{i + 0x46:02X}', 'address': f'0x{value:08X}',
                'handler': handlers[value]} for i, value in enumerate(struct.unpack('<8I', table_bytes))]
    scratch = ROOT / 'notes/decomp/codex/cloud-local-player'
    functions = list(csv.DictReader((scratch / 'functions.tsv').open(), delimiter='\t'))
    assert all(row['decompile_ok'] == 'true' for row in functions)
    for row in functions:
        for name in [row['file'], row['file'].replace('.c', '.asm.txt')]:
            (OUT / name).write_bytes((scratch / name).read_bytes().replace(b'\r\n', b'\n'))
    for name in ['functions.tsv', 'dependencies.tsv', 'data_references.tsv']:
        (OUT / name).write_bytes((scratch / name).read_bytes().replace(b'\r\n', b'\n'))
    report = {'cloud_source': json.loads((REVIEW / 'source.json').read_text()),
              'exe_sha256': hashlib.sha256(exe).hexdigest(),
              'check_tables_stdout': result.stdout, 'table_check': table_check,
              'state_jump_table': {'address': '0x800A5A78', 'file_offset': hex(off(0x800A5A78)),
                                   'bytes_hex': table_bytes.hex(), 'mapping': mapping},
              'normalization': {'core': '0x80030210..0x800302CC', 'lookup_table': '0x800A9C14',
                                'lookup_index': 'normalized sum-of-squares minus 0x40, signed 16-bit entries',
                                'return': 'v0 retains the sum of three signed-component squares, not its square root',
                                'evidence': 'SQR0 then sums at 80030230/34; later instructions leave v0 unchanged. VectorNormalSS wrapper returns it.',
                                'limits': 'GTE saturation/normalization rounding for arbitrary 3D vectors not differentially validated here.'},
              'tests': {'synthetic_player': '7114 checks, 0 failures with local GCC and WITH_CODEX_MATH',
                        'sanitizers': 'not run: installed MinGW toolchain does not provide the cloud ASan/UBSan environment',
                        'original_game_trace': 'not captured: controlled gameplay actions require human PCSX-Redux operation'},
              'export': {'roots': sum(r['role'] == 'root' for r in functions),
                         'functions': len(functions), 'failed': 0, 'ghidra': '12.1.2 private copy, noanalysis readOnly'}}
    (OUT / 'report.json').write_bytes((json.dumps(report, indent=2) + '\n').encode())
    readme = ['# Local response to cloud player handoff', '',
              'Cloud implementation remains on claude/gallant-bohr-ms36p2; no player sources changed.',
              'Read report.json for exact branch commit, EXE hash, table check, decoded jump mapping and limits.', '',
              '## Original table check', '', '```text', result.stdout.rstrip(), '```', '',
              '## States 0x46..0x4D handler mapping', '',
              '| State | Entry | Handler |', '|---|---|---|']
    readme += [f"| {r['state']} | {r['address']} | {r['handler']} |" for r in mapping]
    readme += ['', '## Integration and contact evidence', '',
               '- Mode_Stage calls PadUpdate, CameraUpdate, PlayerUpdate, other actor/gimmick updates, then CollisionUpdate.',
               '- PlayerUpdate splits fixed-point moveVel into whole-unit vel and velFrac; it does not perform ordinary position integration.',
               '- CollisionUpdate clears body contact (+0x2C, player +0x50), handles type 1/player probes, applies x/z motion and invokes BodyLandOnGround for y motion.',
               '- BodyLandOnGround samples at predicted y minus half body height; if floor is above predicted y, it clamps position and changes velocity to floor minus old y. Otherwise it applies vel.y plus its extra parameter.',
               '- A vertical step exceeding 0xFF restores the previous body snapshot and may trigger damage; a simple unconditional floor snap is not equivalent.',
               '- BodyRecordColliderContact writes collider index at body +0x34 (player +0x58), distinct from player contact +0x50.',
               '- CollisionResolveProbes and body-body collision paths may set contact. Review these before wiring the host motion adapter.',
               '- VectorNormalSS core lookup is at 0x800A9C14. Its return is sum of squares; matching instruction listing is authoritative over incomplete GTE pseudocode.', '',
               '## Validation limits and next steps', '',
               'The installed GCC passed all 7114 synthetic player checks with Codex math enabled.',
               'These are not original gameplay tests and do not validate the omitted host collision/landing path.',
               'Use the Codex per-frame capture tool for human-driven gameplay, then the player checker.',
               '`tools/codex/player_frame_capture.lua` is an additive logger; it does not edit the existing Claude logger.',
               'Load it after game boot, call aeplayer_trace_start(), mark each scenario using aeplayer_trace_mark(),',
               'and call aeplayer_trace_stop() after standing, walk/run/turn, tapped/held jump, ledge fall and hard fall.',
               'Default output: ignored extract/codex/cloud-player-review/player_trace.jsonl. Capture the no-gadget/base-state scope; unsupported gadget/water/attach behavior is explicitly rejected.',
               '`python tools/codex/check_native_player.py <trace.jsonl> --player-dir <source-directory> --report <ignored-report.json>`',
               'compiles a C bridge against the specified unchanged player module and original EXE/member inputs.',
               'It reports first divergence overall and per represented field; unsupported bits cannot pass validation.',
               'The Lua tool requires a runtime check in PCSX-Redux; no human gameplay capture was available in this session.',
               'Bridge compilation passed local GCC with -Wall -Wextra -Werror; a synthetic checker fault-injection confirmed',
               'an unchanged frame passes and a deliberately corrupted pos[0] is reported at the correct frame.',
               'Do not label seeded one-frame update checks as an end-to-end world-motion replay.',
               'The viewer stand-in should be replaced only with an explicit host adapter implementing the above semantics.', '',
               f'Fresh export: {len(functions)} functions, zero decompiler failures. All original binaries remain excluded.']
    (OUT / 'README.md').write_bytes(('\n'.join(readme) + '\n').encode())
    manifest = {p.name: {'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                for p in sorted(OUT.iterdir()) if p.is_file() and p.name != 'manifest.json'}
    (OUT / 'manifest.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode())
    print(result.stdout)
    print(json.dumps(mapping, indent=2))


if __name__ == '__main__':
    main()
