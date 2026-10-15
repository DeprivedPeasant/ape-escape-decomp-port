"""Original-instruction regression for signed grid versus unsigned collider ids."""
import ctypes as C
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.check_native_collision import library, OUT, collision_records
from codex.check_collision_floor import full_reference, setup
from codex.check_collision_movement import compare, Move


def main():
    lib = library(); setup(lib)
    lib.ae_collision_resolve.argtypes = [C.c_void_p, C.POINTER(C.c_int32), C.POINTER(C.c_int32),
                                       C.c_int32, C.c_int32, C.POINTER(Move)]
    source = (da.MEMBERS / '05B.bin').read_bytes()
    ah = struct.unpack_from('<I', source, 0x4D0)[0]
    start = ah + struct.unpack_from('<I', source, ah + 16)[0] * 4
    shape = start + struct.unpack_from('<I', source, start + 4)[0]
    boxes = start + struct.unpack_from('<I', source, start + 8)[0]
    records, _ = collision_records(source)
    rows = []
    for identifier in (0x7FFF, 0x8000, 0x8001, 0xFFFF):
        blob = bytearray(source)
        for i in range(len(records)):
            struct.pack_into('<H', blob, shape + 2 + i * 20 + 14, identifier)
        # Remove collider records from this owned fixture to isolate the grid id.
        struct.pack_into('<H', blob, boxes, 0)
        path = OUT / f'grid_contact_{identifier:04x}.bin'; path.write_bytes(blob)
        native = lib.ae_collision_load(str(path).encode()); assert native
        ref, _, w, d, _ = full_reference(blob)
        comparisons = contacts = 0
        try:
            for record in records:
                b = struct.unpack('<10h', record)
                x, y, z = b[0] * 512 - w * 256 + 256, -b[1] * 512, b[2] * 512 - d * 256 + 256
                for dx, dz in ((0, 0), (b[3], 0), (0, b[5])):
                    pos = (x + dx, y - 100, z + dz)
                    contacts += compare(lib, native, ref, pos, (13, 20, -13), 64, 256)
                    comparisons += 1
                if comparisons >= 192 and contacts:
                    break
            assert contacts, 'Fixture did not exercise a grid contact'
        finally:
            lib.ae_collision_free(native)
        signed_id = identifier if identifier < 0x8000 else identifier - 0x10000
        expected = (0xA0000000 | (signed_id & 0xFFFFFFFF)) & 0xFFFFFFFF
        rows.append({'cell_id': identifier, 'expected_contact': f'{expected:08X}',
                     'comparisons': comparisons, 'contacts': contacts})
        print(rows[-1], flush=True)
    result = {'grid_cases': rows, 'failures': 0,
              'oracle': 'Original CollisionBuildProbePoints and CollisionResolveProbes instructions',
              'fixture': 'Owned copy of Fossil Field collision data; grid ids replaced, collider count zero'}
    (OUT / 'signed-grid-contact-validation.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
