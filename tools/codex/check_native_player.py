"""Compare captured original PlayerUpdate frames with the cloud module.

Requires original data and an existing player source directory. Seeds each update
from captured RAM to isolate movement logic; does NOT validate world collision or
claim an uninterrupted end-to-end replay. Reports the first differing field.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('trace', type=Path)
    ap.add_argument('--player-dir', type=Path, default=ROOT / 'extract/codex/cloud-player-review')
    ap.add_argument('--exe', type=Path, default=ROOT / 'extract/disc/SCUS_944.23')
    ap.add_argument('--member', type=Path, default=ROOT / 'extract/members/008.bin')
    ap.add_argument('--report', type=Path)
    ap.add_argument('--audit-all', action='store_true', help='continue independently seeded comparisons after unsupported frames, excluding them')
    a = ap.parse_args()
    src = [ROOT / 'tools/codex/player_review_bridge.c', a.player_dir / 'native_player.c', a.player_dir / 'native_player.h']
    digest = hashlib.sha256(b''.join(p.read_bytes() for p in src)).hexdigest()[:16]
    out = ROOT / 'extract/codex/cloud-player-review' / ('player_review_' + digest + ('.dll' if sys.platform == 'win32' else '.so'))
    out.parent.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which('gcc')
    if not compiler:
        raise RuntimeError('Installed GCC unavailable')
    command = [compiler, '-std=c99', '-Wall', '-Wextra', '-O1', '-g', '-shared',
               '-I' + str(a.player_dir), str(src[0]), str(src[1]), '-lm', '-o', str(out)]
    if sys.platform != 'win32':
        command.append('-fPIC')
    subprocess.run(command, check=True, capture_output=True)
    dll = C.CDLL(str(out))
    dll.review_load.argtypes = [C.c_char_p, C.c_char_p]
    if not dll.review_load(str(a.exe).encode(), str(a.member).encode()):
        raise RuntimeError('Original data load failed')
    dll.review_name.restype = C.c_char_p
    dll.review_step.argtypes = [C.POINTER(C.c_uint8), C.POINTER(C.c_int32), C.POINTER(C.c_int32), C.POINTER(C.c_int64)]
    dll.review_step.restype = C.c_uint32
    fields = [(dll.review_name(i).decode(), *(dll.review_meta(i, k) for k in range(4)))
              for i in range(dll.review_count())]
    checked = 0
    issue = None
    by_field = {}
    unsupported = 0
    first_unsupported = None
    skipped = mismatched = 0
    field_mismatches = {}
    animation_only = 0
    scenario = 'unmarked'
    scenarios = {}
    for line in a.trace.read_text().splitlines():
        record = json.loads(line)
        if record.get('kind') == 'mark':
            scenario = record['label']
            continue
        if record.get('kind') != 'frame':
            continue
        row = scenarios.setdefault(scenario, {'frames': 0, 'compared': 0, 'matched': 0, 'mismatched': 0, 'unsupported': 0})
        row['frames'] += 1
        if record['mode'] != 0:
            detail = {'frame': record['frame'], 'scenario': scenario, 'reason': 'Player mode outside module scope', 'mode': record['mode']}
            first_unsupported = first_unsupported or detail
            issue = issue or detail
            skipped += 1
            row['unsupported'] += 1
            if a.audit_all:
                continue
            break
        pre, expected = bytes.fromhex(record['pre']), bytes.fromhex(record['post_update'])
        assert len(pre) == len(expected) == 0x150
        original_state = int.from_bytes(expected[0x5e:0x60], 'little')
        if original_state >= 0x80:
            detail = {'frame': record['frame'], 'scenario': scenario, 'reason': 'Original entered gadget state outside module scope', 'state': original_state}
            first_unsupported = first_unsupported or detail
            issue = issue or detail
            skipped += 1
            row['unsupported'] += 1
            if a.audit_all:
                continue
            break
        ram = (C.c_uint8 * len(pre)).from_buffer_copy(pre)
        inputs = (C.c_int32 * 16)(*record['input'])
        ground = (C.c_int32 * 6)(*record['ground'])
        predicted = (C.c_int64 * 128)()
        bits = dll.review_step(ram, inputs, ground, predicted)
        unsupported |= bits
        if bits:
            detail = {'frame': record['frame'], 'scenario': scenario, 'reason': 'Unsupported behavior reached; not counted as validated', 'bits': bits}
            first_unsupported = first_unsupported or detail
            issue = issue or detail
            skipped += 1
            row['unsupported'] += 1
            if a.audit_all:
                continue
            break
        j = 0
        different = False
        changed_fields = set()
        for name, offset, width, count, sign in fields:
            for k in range(count):
                real = int.from_bytes(expected[offset+k*width:offset+(k+1)*width], 'little', signed=bool(sign))
                if real != predicted[j]:
                    different = True
                    changed_fields.add(name)
                    field_mismatches[name] = field_mismatches.get(name, 0) + 1
                    detail = {'frame': record['frame'], 'field': name, 'element': k, 'original': real,
                              'native': predicted[j], 'unsupported': bits, 'scenario': scenario}
                    by_field.setdefault(f'{name}[{k}]', detail)
                    if issue is None:
                        issue = detail
                j += 1
        checked += 1
        mismatched += int(different)
        animation_only += int(changed_fields == {'next_frame'})
        row['compared'] += 1
        row['mismatched' if different else 'matched'] += 1
    if not checked and not issue:
        raise ValueError('No captured frame records; empty input cannot pass validation')
    report = {'mode': 'seeded one-frame PlayerUpdate differential; no host collision replay',
              'trace_sha256': hashlib.sha256(a.trace.read_bytes()).hexdigest(),
              'source_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in src},
              'checked_frames': checked, 'first_divergence': issue,
              'matched_frames': checked-mismatched, 'mismatched_frames': mismatched,
              'excluded_frames': skipped, 'first_unsupported': first_unsupported,
              'field_mismatch_elements': field_mismatches, 'next_frame_only_mismatched_frames': animation_only,
              'scenarios': scenarios, 'audit_all': a.audit_all,
              'first_divergence_per_field': by_field, 'unsupported': unsupported,
              'limits': 'Seeds represented player fields and selected globals only. Unrepresented scratch/global context and capture timing must be audited before attributing a difference to module logic.'}
    print(json.dumps(report, indent=2))
    if a.report:
        a.report.write_text(json.dumps(report, indent=2) + '\n')
    return int(issue is not None)


if __name__ == '__main__':
    sys.exit(main())
