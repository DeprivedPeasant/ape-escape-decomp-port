"""Reject malformed/truncated particle bytecode; verify each real decode boundary."""
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_effect_scripts as effects


def main():
    cases = {
        'unknown': [5,4], 'truncated_operand': [37,1,2],
        'missing_end': [41], 'unmatched_loop': [3,4],
        'loop_depth': [1,2,1,2,1,2,4],
    }
    rejected = 0
    for label,words in cases.items():
        try:
            effects.disassemble(struct.pack('<'+'H'*len(words),*words),0)
        except ValueError:
            rejected += 1
        else:
            raise AssertionError(f'accepted {label}')
    r = json.loads((effects.ROOT/'extract/codex/effect-scripts.json').read_text())
    truncated = 0
    for script in r['scripts']:
        source = script['source']
        if source == 'EXE':
            blob = (effects.ROOT/'extract/disc/SCUS_944.23').read_bytes()
            origin = 0x80010000-0x800
        else:
            blob = (effects.ROOT/f'extract/members/{source}.bin').read_bytes()
            origin = 0x8013D000
        records,end = effects.disassemble(blob,script['address']-origin,script['end']-origin)
        assert len(records) == len(script['commands']) and end+origin == script['end']
        try:
            effects.disassemble(blob,script['address']-origin,script['end']-origin-2)
        except ValueError:
            truncated += 1
        else:
            raise AssertionError('accepted real stream without its terminator')
    result = {'synthetic_corruptions_rejected':rejected,'real_stream_boundaries_verified':len(r['scripts']),
              'real_streams_missing_end_rejected':truncated}
    (effects.ROOT/'extract/codex/effect-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(result)


if __name__ == '__main__':
    main()
