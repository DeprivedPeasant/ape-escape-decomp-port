"""Prepare area hint indicator placements and existing validated actor exports.

AEIND1 is a local presentation format. Registration follows the area group handler
order and Gimmick_ProxTrigger_Init; this does not run trigger/game-progress logic.
"""
import argparse
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_objects as objects
from codex.area_tables import ROOT, area_members
from codex.feature_sources import inventory
from codex.pack_actor import pack_actor

OUT = ROOT/'extract/codex/native'


def registrations(area, blob, raw=False):
    """raw=True also returns each source Gimmick instance (24 bytes) and referenced
    collider (56 bytes) as hex for the runtime pack; default rows are unchanged."""
    groups, instances = objects.parse(blob)
    handlers = objects.handlers(area)
    ah = struct.unpack_from('<I', blob, 0x4D0)[0]
    co = ah+struct.unpack_from('<I', blob, ah+16)[0]*4
    boxes = co+struct.unpack_from('<I', blob, co+8)[0]
    count = struct.unpack_from('<H', blob, boxes)[0]
    assert boxes+2+count*56 <= len(blob)
    result = []
    for group, (n, _, first, _) in enumerate(groups):
        if group >= len(handlers) or handlers[group][0] != 0x80091060:
            continue
        assert n >= 0 and first >= 0 and first+n <= len(instances)
        for index in range(first, first+n):
            obj = instances[index]
            assert obj[0] == 0x22 and 0 <= obj[2] < count
            position = struct.unpack_from('<hhh', blob, boxes+2+obj[2]*56)
            result.append(dict(object=index, collider=obj[2], kind=obj[5], angle=obj[6],
                               position=list(position), hint_sector=obj[9],
                               subtype=obj[4], radius=obj[7]))
            if raw:
                # parse() unpacks the 24-byte record "<HHh9h" losslessly; repack it.
                trigger = struct.pack('<HHh9h', *obj)
                collider = blob[boxes+2+obj[2]*56:boxes+2+obj[2]*56+56]
                assert len(trigger) == 24 and len(collider) == 56
                assert struct.unpack_from('<H', trigger, 4)[0] == obj[2]
                assert collider[:6] == struct.pack('<hhh', *position)
                result[-1].update(raw_trigger=trigger.hex(), raw_collider=collider.hex())
    assert len(result) <= 4, (area, len(result))
    return result


def pack(area=1, runtime=False):
    member = area_members()[area]
    blob = (ROOT/f'extract/members/{member:03X}.bin').read_bytes()
    rows = registrations(area, blob, raw=runtime)
    _, sources = inventory()
    assets = {r['kind']: r for r in sources if r['area'] == area and r['kind'] in (0x10, 0x11)}
    magic = b'AEIND2\0\0' if runtime else b'AEIND1\0\0'
    payload = bytearray(struct.pack('<8sII', magic, area, len(rows)))
    prepared = {}
    for row in rows:
        assert row['kind'] in (0, 1, 2), row
        feature = 0x10 if row['kind'] < 2 else 0x11
        source = assets[feature]
        if not source.get('export_supported', source['scanner_supported']):
            raise ValueError(f'area {area:02X}: one-joint indicator asset unsupported by exporter')
        if feature not in prepared:
            path = ROOT/f'extract/gltf/{member:03X}_{source["model"]:06X}/actor.gltf'
            prepared[feature] = pack_actor(path)
        row['actor_pack'] = prepared[feature]['path']
        row['model'] = source['model']
        # Position8 padding is zero in the prepared presentation, unlike the original's
        # copied uninitialized stack halfword; register overwrites it with timer0 anyway.
        name = Path(row['actor_pack']).name.encode('ascii')
        assert len(name) < 64
        payload.extend(struct.pack('<hhhhhHII64s', row['kind'], row['angle'],
                                   *row['position'], 0, row['object'], row['hint_sector'], name))
        if runtime:
            trigger, collider = bytes.fromhex(row['raw_trigger']), bytes.fromhex(row['raw_collider'])
            assert len(trigger) == 24 and len(collider) == 56
            payload.extend(trigger + collider)
    assert len(payload) == 16 + len(rows)*(164 if runtime else 84)
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT/(f'indicators_runtime_{area:02X}.aei' if runtime else f'indicators_{area:02X}.aei')
    path.write_bytes(payload)
    result = dict(area=area, member=member, count=len(rows), records=rows, pack=str(path), runtime=runtime)
    path.with_suffix('.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--area', type=lambda s: int(s, 0), default=1)
    parser.add_argument('--audit-all', action='store_true')
    parser.add_argument('--runtime', action='store_true', help='write AEIND2 runtime pack')
    args = parser.parse_args()
    if args.audit_all:
        all_rows = []
        _, sources = inventory()
        supported = {(r['area'], r['kind']): r.get('export_supported', r['scanner_supported']) for r in sources}
        for area, member in sorted(area_members().items()):
            rows = registrations(area, (ROOT/f'extract/members/{member:03X}.bin').read_bytes())
            for row in rows:
                feature = 0x10 if row['kind'] < 2 else 0x11
                assert row['kind'] in (0, 1, 2) and (area, feature) in supported
                row.update(area=area, member=member, supported=supported[area, feature])
            all_rows.extend(rows)
        report = dict(areas=101, areas_with_indicators=len({r['area'] for r in all_rows}),
                      indicators=len(all_rows), unsupported_assets=sum(not r['supported'] for r in all_rows),
                      records=all_rows, failures=0)
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT/'indicator-registration-audit.json').write_text(json.dumps(report, indent=2)+'\n')
        print({k: v for k, v in report.items() if k != 'records'})
    else:
        print(json.dumps(pack(args.area, args.runtime), indent=2))


if __name__ == '__main__':
    main()
