"""Read back every emitted actor glTF and check actual binary/PNG resource coverage.

uv run --offline python tools/codex/check_exports.py
Run export_gltf.py all first. Outputs coverage JSON in extract/ and generated notes.
"""
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import export_gltf as gl
import export_model as em


def resource(folder,uri):
    path = (folder/uri).resolve()
    if not path.is_relative_to(folder.resolve()) or ':' in uri or '\\' in uri:
        raise ValueError(f'non-local or escaping URI {uri}')
    if not path.is_file():
        raise ValueError(f'missing resource {uri}')
    return path


def png_check(blob):
    if blob[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('bad PNG signature')
    offset,idat,header,ended = 8,bytearray(),None,False
    while offset < len(blob):
        if offset+12 > len(blob):
            raise ValueError('truncated PNG chunk')
        length = struct.unpack_from('>I',blob,offset)[0]
        kind = blob[offset+4:offset+8]
        end = offset+12+length
        if end > len(blob):
            raise ValueError('PNG chunk exceeds file')
        data = blob[offset+8:end-4]
        crc = struct.unpack_from('>I',blob,end-4)[0]
        if zlib.crc32(kind+data)&0xFFFFFFFF != crc:
            raise ValueError('PNG CRC mismatch')
        if kind == b'IHDR':
            if header is not None or length != 13:
                raise ValueError('bad PNG IHDR')
            header = struct.unpack('>IIBBBBB',data)
        elif kind == b'IDAT':
            idat.extend(data)
        elif kind == b'IEND':
            if length or end != len(blob):
                raise ValueError('bad PNG IEND or trailing bytes')
            ended = True
        else:
            raise ValueError(f'unexpected exporter PNG chunk {kind}')
        offset = end
    if not ended or header != (256,256,8,6,0,0,0):
        raise ValueError(f'PNG format mismatch {header}')
    decoded = zlib.decompress(idat)
    stride = 256*4+1
    if len(decoded) != 256*stride or any(decoded[i*stride] for i in range(256)):
        raise ValueError('PNG decompressed rows/filter mismatch')
    return len(decoded)


def main():
    sample = next((gl.OUT/'008_0000BC/tex').glob('*.png')).read_bytes()
    damaged = bytearray(sample)
    damaged[29] ^= 1  # IHDR CRC
    rejected = 0
    for blob in (bytes(damaged),sample[:-1],sample+b'\x00'):
        try:
            png_check(blob)
        except (ValueError,zlib.error):
            rejected += 1
        else:
            raise AssertionError('corrupt PNG accepted')
    try:
        resource(gl.OUT/'008_0000BC','../actor.bin')
    except ValueError:
        rejected += 1
    else:
        raise AssertionError('escaping resource URI accepted')
    metadata = gl.clip_metadata()
    rows,failures,resources = [],[],set()
    binary_bytes,png_bytes,png_count,animation_count,pose_count = 0,0,0,0,0
    for member,offset,m in em.scan_members():
        folder = gl.OUT/f'{member}_{offset:06X}'
        try:
            g = json.loads((folder/'actor.gltf').read_text())
            path = resource(folder,g['buffers'][0]['uri'])
            binary = path.read_bytes()
            arrays = gl.validate(g,binary)
            expected = metadata.get((member,offset),[])
            assert len(g['skins'][0]['joints']) == len(m['bones']), 'joint count differs'
            assert len(g['animations']) == len(expected), 'clip count differs'
            poses = 0
            for source,clip in zip(expected,g['animations']):
                times = arrays[clip['samplers'][0]['input']]
                count = gl.clip_frame_count(source)
                assert len(times) == count, 'pose sample count differs'
                duration = source[5] if len(source) == 6 and isinstance(source[5],(int,float)) else (source[3]-source[2])*source[4]
                assert abs(times[-1][0]-duration/g['extras']['fps']) < 2e-5, 'clip duration differs'
                poses += count
            for img in g.get('images',[]):
                path = resource(folder,img['uri'])
                if path not in resources:
                    blob = path.read_bytes()
                    png_check(blob)
                    png_bytes += len(blob)
                    png_count += 1
                    resources.add(path)
            rows.append({'member':member,'offset':offset,'joints':len(m['bones']),
                         'clips':len(expected),'poses':poses,'binary_bytes':len(binary),
                         'images':len(g.get('images',[]))})
            binary_bytes += len(binary)
            animation_count += len(expected)
            pose_count += poses
        except (ValueError,AssertionError,OSError,KeyError,IndexError,zlib.error) as error:
            failures.append(f'{member}+0x{offset:X}: {error}')
        if (len(rows)+len(failures))%100 == 0:
            print(f'checked exports={len(rows)} failures={len(failures)}',flush=True)
    result = {'models':len(rows),'clips':animation_count,'pose_samples':pose_count,'png_resources':png_count,
              'binary_bytes':binary_bytes,'png_bytes':png_bytes,'rows':rows,'failures':failures,
              'resource_corruptions_rejected':rejected}
    (gl.OUT/'export-coverage.json').write_text(json.dumps(result,indent=2)+'\n')
    note = ['# Complete on-disk actor export coverage (generated)','',
            'Generated by `uv run --offline python tools/codex/check_exports.py` after `tools/export_gltf.py all`.', '',
            'This reads every scanner actor export from disk, verifies its actual binary with the structural '
            'validator, compares joint/clip/sample counts and durations against loader metadata, and resolves '
            'all buffer/image URIs inside the individual export directory. Each PNG is checked for signature, '
            'chunk bounds, CRCs, dimensions/RGBA format, decompressed length, row filters and final IEND.', '',
            f'- Actor files passed: {len(rows)}',f'- Animation clips: {animation_count}',
            f'- Pose samples: {pose_count}',f'- PNG resources checked: {png_count}',
            f'- Binary payload: {binary_bytes:,} bytes',f'- PNG payload: {png_bytes:,} bytes',
            f'- Failures: {len(failures)}','',
            f'Rejected {rejected} corruption fixtures: PNG CRC, truncation, trailing bytes and a resource URI escaping its folder.', '',
            'All glTF/bin/PNG outputs remain under extract/gltf (ignored; not committed). '
            'This proves export structure/resource coverage, not that every auxiliary model has correct '
            'runtime textures or resolved animation metadata. Auxiliary members use global VRAM unless '
            'an area load chain or validated compact UI load list identifies their specific image. PSX material effects remain approximations. '
            f'{sum(r["clips"] == 0 for r in rows)} scanner actor instances still have no automatically associated clip; notes/56-actor-sources.md '
            'documents them. The exporter supplies their skeleton and bind mesh.', '',
            '## Failures','']+(failures or ['None.'])
    (gl.ROOT/'notes/57-export-coverage.md').write_text('\n'.join(note)+'\n',encoding='utf-8')
    print({k:v for k,v in result.items() if k != 'rows'})
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
