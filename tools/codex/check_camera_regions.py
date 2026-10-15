"""Check camera region queries against original MIPS and link every actual area CAM."""
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
from pathlib import Path
from mips_ground_reference import Reference, signed, da

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'extract/codex/native'


class Regions(C.Structure):
    _fields_ = [('records', C.c_void_p), ('count', C.c_uint16),
                ('named_count', C.c_uint16), ('cam_offset', C.c_uint32)]


def expected(blob):
    off = struct.unpack_from('<I', blob, 0x4D8)[0]
    if not off:
        return off, 0, b''
    assert blob[off:off+3] == b'CAM'
    count = struct.unpack_from('<H', blob, off+4)[0]
    records = bytearray(blob[off+16:off+16+count*48])
    names = 0
    if count:
        table = off+struct.unpack_from('<H', blob, off+14)[0]
        names = struct.unpack_from('<H', blob, table)[0]
        for i in range(count):
            if not names:
                continue
            hits = [j for j in range(names)
                    if records[i*48+16:i*48+48] == blob[table+8+j*40+8:table+8+j*40+40]]
            assert hits, (off, i)
            j = hits[0]
            struct.pack_into('<HH', records, i*48, j,
                             struct.unpack_from('<H', blob, table+8+j*40)[0])
    return off, names, bytes(records)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f'camera_regions_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-shared', str(ROOT/'src/camera/native_camera_regions.c'),
                    '-o', str(dll), '-static-libgcc'], check=True)
    lib = C.CDLL(str(dll))
    lib.ae_camera_regions_load.argtypes = [C.POINTER(Regions), C.c_char_p, C.c_size_t]
    lib.ae_camera_regions_free.argtypes = [C.POINTER(Regions)]
    lib.ae_camera_regions_error.restype = C.c_char_p
    lib.ae_camera_region_test.argtypes = [C.c_char_p, C.c_size_t, C.c_uint16, C.POINTER(C.c_int16)]
    lib.ae_camera_region_test.restype = C.c_int32
    ref = Reference((ROOT/'extract/members/05B.bin').read_bytes())
    rng = random.Random(94423)
    queries = hits = 0

    def compare(records, point):
        nonlocal queries, hits
        count = len(records)//48
        ref.write(0x800E68D0, 2, count)
        ref.write(0x800E691C, 4, 0x8013A000)
        ref.ram[0x13A000:0x13A000+len(records)] = records
        for i, v in enumerate(point):
            ref.write(0x8013F000+i*2, 2, v)
        original = signed(ref.call(0x8004BD98, (0x8013F000,))[2])
        result = lib.ae_camera_region_test(records, len(records), count, (C.c_int16*3)(*point))
        assert result == original, (queries, result, original, records.hex(), point)
        queries += 1
        hits += result >= 0

    for case in range(6000):
        count = rng.randrange(17)
        records = bytearray(rng.randbytes(count*48))
        point = [rng.randrange(-32768, 32768) for _ in range(3)]
        for i in range(count):
            flags = rng.choice([0, 0x6000, 0x8100, 0x8300, 0x8000, 0x2000, 0x4000, rng.randrange(65536)])
            centers = point if case%3 else [rng.randrange(-32768, 32768) for _ in range(3)]
            widths = [rng.choice([-32768, -3, -1, 0, 1, 2, 3, 2048, 32767]) for _ in range(3)]
            struct.pack_into('<HHhhhhhh', records, i*48, i, flags, *centers, *widths)
        if count and case%4 == 0:
            center, width = struct.unpack_from('<hh', records, 4)[0], struct.unpack_from('<h', records, 10)[0]
            half = abs(width)//2 * (-1 if width < 0 else 1)
            point[0] = signed(center+rng.choice([-half, -half+1, half-1, half]), 16)
        compare(bytes(records), point)

    present = nonempty = total = 0
    r = Regions()
    for area, member in sorted(da.area_members().items()):
        blob = (ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        off, names, records = expected(blob)
        input_buffer = C.create_string_buffer(blob)
        assert lib.ae_camera_regions_load(C.byref(r), input_buffer, len(blob)), lib.ae_camera_regions_error()
        assert (r.cam_offset, r.named_count, r.count) == (off, names, len(records)//48)
        assert C.string_at(r.records, len(records)) == records
        assert input_buffer.raw[:-1] == blob
        present += bool(off); nonempty += bool(records); total += r.count
        for i in range(r.count):
            center = struct.unpack_from('<hhh', records, i*48+4)
            compare(records, center)
            for axis in range(3):
                width = struct.unpack_from('<h', records, i*48+10+axis*2)[0]
                half = abs(width)//2 * (-1 if width < 0 else 1)
                for sign in (-1, 1):
                    point = list(center); point[axis] = signed(center[axis]+sign*half, 16)
                    compare(records, point)
        if area == 1:
            assert off and r.count == 0
    assert (present, nonempty, total) == (85, 78, 246)

    # Small valid fixture exercises transactional bounds/name failures and first-name wins.
    fixture = bytearray(0x4DC+16+48+8+80)
    off = 0x4DC
    struct.pack_into('<I', fixture, 0x4D8, off)
    fixture[off:off+3] = b'CAM'
    struct.pack_into('<H', fixture, off+4, 1)
    struct.pack_into('<H', fixture, off+14, 64)
    fixture[off+32:off+64] = b'N'*32
    struct.pack_into('<H', fixture, off+64, 2)
    for j in range(2):
        struct.pack_into('<H', fixture, off+72+j*40, j+0x2000)
        fixture[off+80+j*40:off+112+j*40] = b'N'*32
    assert lib.ae_camera_regions_load(C.byref(r), bytes(fixture), len(fixture))
    assert C.string_at(r.records, 4) == struct.pack('<HH', 0, 0x2000)
    before = bytes(r), C.string_at(r.records, 48)
    failures = 0
    malformed = [bytes(fixture[:n]) for n in (0, 0x4DB, off+15, off+63, off+71, len(fixture)-1)]
    for field, value, fmt in ((0x4D8, 0xFFFFFFFF, '<I'), (off+4, 65535, '<H'), (off+14, 65535, '<H'), (off+64, 65535, '<H')):
        b = bytearray(fixture); struct.pack_into(fmt, b, field, value); malformed.append(bytes(b))
    b = bytearray(fixture); b[off] = 0; malformed.append(bytes(b))
    b = bytearray(fixture); b[off+32] = ord('X'); malformed.append(bytes(b))
    for b in malformed+[None]:
        assert not lib.ae_camera_regions_load(C.byref(r), b, len(b) if b else 0)
        assert (bytes(r), C.string_at(r.records, 48)) == before
        assert lib.ae_camera_regions_error()
        failures += 1
    p = (C.c_int16*3)(0, 0, 0)
    assert lib.ae_camera_region_test(None, 0, 0, p) == -1
    for args in ((None, 48, 1, p), (bytes(48), 47, 1, p), (None, 0, 0, None)):
        assert lib.ae_camera_region_test(*args) == -2
        failures += 1
    lib.ae_camera_regions_free(C.byref(r)); assert not r.records and not r.count
    report = dict(original_mips_queries=queries, hits=hits, areas=101, cam_sections=present,
                  nonempty_sections=nonempty, records=total, failure_checks=failures, failures=0)
    (OUT/'camera-regions-validation.json').write_text(json.dumps(report, indent=2)+'\n')
    (ROOT/'notes/port/validation/camera_regions_validation.md').write_text(
        '# Camera region validation\n\nGenerated by `check_camera_regions.py`.\n\n'+
        '\n'.join(f'- {k}: {v}' for k, v in report.items())+'\n\n'+
        'Original extracted instructions execute in the test-only MIPS interpreter. '
        'Name linking is independently decoded from all actual area blobs. No console capture claim. '
        'Region lifecycle, scripts and slot selection are still separate.\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
