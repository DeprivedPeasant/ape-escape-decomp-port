"""Validate native scene loading and GPU rendering using the real extracted areas.

Runs offline. All packs/executables/frame captures remain under ignored extract/.
Produces notes/65-native-area-viewer.md from actual run results.
"""
import argparse
import json
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_areas as da
import dump_tims as dt
from codex.build_native_viewer import build
from codex.pack_area import OUT, ROOT, pack


def run(executable, *args, success=True):
    result = subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True, timeout=60)
    if (result.returncode == 0) != success:
        raise AssertionError(f'Unexpected native status {result.returncode}: {result.stdout}\n{result.stderr}')
    return result


def corruptions(executable, path):
    original = path.read_bytes()
    nv, nb, nt = struct.unpack_from('<3I', original, 20)
    vertex = 64+nb*16
    texture = vertex+nv*32
    tests = [
        ('bad signature', 0, b'INVALID!'), ('wrong version', 8, struct.pack('<I', 2)),
        ('zero vertices', 20, struct.pack('<I', 0)), ('huge vertex count', 20, struct.pack('<I', 0xFFFFFFFF)),
        ('huge batch count', 24, struct.pack('<I', 0xFFFFFFFF)), ('huge texture count', 28, struct.pack('<I', 0xFFFFFFFF)),
        ('nonfinite bounds', 40, struct.pack('<f', float('nan'))), ('reversed bounds', 40, struct.pack('<f', 1e6)),
        ('batch gap', 64, struct.pack('<I', 3)), ('batch overrun', 68, struct.pack('<I', nv+3)),
        ('bad texture reference', 72, struct.pack('<I', nt)), ('unknown batch kind', 76, struct.pack('<I', 3)),
        ('nonfinite vertex', vertex, struct.pack('<f', float('inf'))),
        ('invalid color', vertex+20, struct.pack('<f', -1)),
        ('outside bounds', vertex, struct.pack('<f', 1e5)),
        ('invalid texture dimensions', texture, struct.pack('<I', 0)),
        ('wrong texture byte length', texture+8, struct.pack('<I', 4)),
    ]
    fixture = OUT/'invalid_scene.aev'
    outcomes = []
    for label, offset, replacement in tests:
        blob = bytearray(original)
        blob[offset:offset+len(replacement)] = replacement
        fixture.write_bytes(blob)
        result = run(executable, fixture, '--check', success=False)
        if result.returncode != 1 or 'ERROR:' not in result.stderr:
            raise AssertionError('invalid scene did not fail cleanly')
        outcomes.append(dict(case=label, error=result.stderr.strip()))
    for label, blob in [('truncated header', original[:32]), ('truncated textures', original[:-1]),
                        ('trailing bytes', original+b'\0')]:
        fixture.write_bytes(blob)
        result = run(executable, fixture, '--check', success=False)
        if result.returncode != 1 or 'ERROR:' not in result.stderr:
            raise AssertionError('invalid scene did not fail cleanly')
        outcomes.append(dict(case=label, error=result.stderr.strip()))
    return outcomes


def ppm(path):
    with path.open('rb') as file:
        if file.readline() != b'P6\n':
            raise ValueError('capture is not PPM')
        width, height = map(int, file.readline().split())
        if file.readline() != b'255\n':
            raise ValueError('capture depth differs')
        pixels = file.read()
    if len(pixels) != width*height*3:
        raise ValueError('capture size differs')
    rgba = bytearray(width*height*4)
    rgba[0::4], rgba[1::4], rgba[2::4], rgba[3::4] = pixels[0::3], pixels[1::3], pixels[2::3], b'\xff'*(width*height)
    dt.png(path.with_suffix('.png'), width, height, bytes(rgba))
    return pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--quick', action='store_true', help='only pack representative areas')
    args = parser.parse_args()
    started = time.perf_counter()
    executable = build()
    rows, failures, captures = [], [], []
    areas = [1, 2, 7, 0x1D] if args.quick else sorted(da.area_members())
    for area in areas:
        try:
            report = pack(area)
            result = run(executable, report['pack'], '--check')
            report['native_load'] = result.stdout.strip()
            rows.append(report)
        except (ValueError, AssertionError, OSError, subprocess.TimeoutExpired) as error:
            failures.append(dict(area=area, error=str(error)))
        if len(rows)%10 == 0:
            print(f'Native load checks: {len(rows)}, failures: {len(failures)}', flush=True)
    errors = corruptions(executable, OUT/'area_01.aev')
    images = {}
    configurations = [(1, 'default', []), (1, 'rotated', ['--yaw', '125', '--pitch', '50']),
                      (1, 'untextured', ['--untextured']), (1, 'colliders', ['--colliders']),
                      (2, 'default', []), (7, 'default', []), (0x1D, 'default', [])]
    for area, name, options in configurations:
        path = OUT/f'area_{area:02X}_{name}.ppm'
        try:
            run(executable, OUT/f'area_{area:02X}.aev', '--capture', path, *options)
            images[(area, name)] = ppm(path)
            report = json.loads(Path(str(path)+'.json').read_text())
            assert report['area'] == area and report['covered_pixels'] > 100 and report['luma_variance'] > 1
            captures.append(dict(name=name, **report))
            print(f"GPU capture {area:02X}/{name}: {report['covered_pixels']} pixels, {report['renderer']}", flush=True)
        except (ValueError, AssertionError, OSError, subprocess.TimeoutExpired) as error:
            failures.append(dict(area=area, capture=name, error=str(error)))
    differences = {}
    if (1, 'default') in images:
        for name in ('rotated', 'untextured', 'colliders'):
            if (1, name) not in images:
                continue
            a, b = images[(1, 'default')], images[(1, name)]
            changed = sum(any(a[i+k] != b[i+k] for k in range(3)) for i in range(0, len(a), 3))
            differences[name] = changed
            if changed < (10 if name == 'colliders' else 1000):
                failures.append(dict(capture=name, error=f'only {changed} changed pixels'))
    try:
        run(executable, OUT/'area_01.aev', '--frames', '120')
        smoke = True
    except (AssertionError, subprocess.TimeoutExpired) as error:
        failures.append(dict(error=str(error))); smoke = False
    result = dict(areas=rows, loader_corruptions=errors, captures=captures, changed_pixels=differences,
                  multiframe_smoke=smoke, failures=failures, seconds=time.perf_counter()-started)
    (OUT/'validation.json').write_text(json.dumps(result, indent=2)+'\n')
    lines = ['# Native Windows area renderer (generated)', '',
             'Generated by `uv run --offline python tools/codex/check_native_viewer.py`.', '',
             '## Milestone and scope', '',
             'A standalone native Windows C11 executable loads locally prepared area assets, uploads '
             'RGBA textures to OpenGL and renders terrain and placed props in an SDL3 window. '
             'No emulator, BIOS, original game executable or Python interpreter is needed at runtime. '
             'Preparation reads the extracted game data through the existing Python OBJ/VRAM tools. '
             'Animated Spike and original static floor/wall responses are integrated in the separate '
             'walking harness (notes/66-69). Enemies, gimmicks and the original player/game loop remain incomplete.', '',
             '## Run', '',
             'From the repository root, using installed GCC and existing extracted data:', '',
             '```powershell',
             'uv run --offline python tools/codex/setup_sdl.py --download',
             'uv run --offline python tools/codex/build_native_viewer.py --area 0x01 --run',
             'uv run --offline python tools/codex/build_native_viewer.py --area 0x01 --capture',
             'uv run --offline python tools/codex/pack_area.py --all',
             'uv run --offline python tools/codex/check_native_viewer.py',
             '```', '',
             'Executable, area packs and captures live in `extract/codex/native/` (gitignored). '
             'The executable can be launched directly with a pack path. SDL3.dll is copied beside it; '
             'the OpenGL driver remains a system dependency. Assets are not embedded into it.', '',
             'Controls: WASD move the camera target, Q/E lower/raise it, Shift moves faster; '
             'hold right mouse button to orbit, mouse wheel zooms, arrows rotate. C toggles collider '
             'wireframes, T toggles textures, Home resets the camera, Escape exits. '
             'The initial view fits visible terrain, falling back to prop bounds in terrainless areas.', '',
             'Animated Spike and terrain walking are available through the build runner\'s '
             '`--spike` / `--player` options (notes/66-68). The checks here isolate the area renderer.', '',
             '## Validation on real data', '',
             f'- Prepared and accepted by native loader: {len(rows)} / {len(areas)} areas.',
             f'- Native framebuffer captures: {len(captures)} (actual `glReadPixels`, depth coverage and luminance variance).',
             f'- Invalid pack fixtures rejected cleanly: {len(errors)}.',
             f'- Hidden 120-frame window/event-loop smoke passed: {smoke}.',
             f'- Failures: {len(failures)}.', '',
             '| Area / view | Terrain triangles | Prop triangles | Textures | Covered pixels | Renderer |',
             '|---|---:|---:|---:|---:|---|']
    for capture in captures:
        lines.append(f"| 0x{capture['area']:02X} / {capture['name']} | {capture['terrain_triangles']} | "
                     f"{capture['prop_triangles']} | {capture['textures']} | {capture['covered_pixels']} | {capture['renderer']} |")
    lines += ['', 'Changed pixels versus the default area 01 view: '+', '.join(f'{k}: {v:,}' for k,v in differences.items())+'.', '',
              'The loader checks file signature/version, bounded counts and allocations, contiguous draw '
              'batch coverage, texture references, kind values, finite vertex/bound values, RGB ranges, '
              'vertex containment, triangle totals, texture dimensions/byte lengths, truncation and trailing bytes. '
              'Corruption fixtures exercise these conditions independently of the packer.', '',
              '## Asset adapter and format', '',
              '`pack_area.py` imports `export_area_obj.py`, `dump_areas.py` and the existing PNG structural '
              'checker. It never edits them. It reads OBJ/MTL/PNG output, expands triangle vertices into '
              'xyz/uv/rgb records, flips texture rows to match OBJ UV origin, doubles textured RGB modulation '
              '(PSX 128 is neutral), and excludes cells whose original bit0 is clear or areas whose header '
              'disables terrain. Collider triangles form a separate optional draw class.', '',
              'AEV1 is a new local prepared format, not a recovered game format. All words are little endian: '
              '64-byte header (`8s8I6f`: magic AEVIEW1 + NUL, version, area, member, vertex/batch/texture counts, '
              'terrain/prop triangle totals, visible min/max xyz); 16-byte batches (`first,count,texture,kind`); '
              '32-byte vertices (`xyz,uv,rgb` floats); each texture has `width,height,byteCount` u32 followed '
              'by bottom-origin RGBA8. Texture 0xFFFFFFFF means untextured. Kinds 0/1/2 are terrain/prop/collider. '
              'This explicit adapter leaves future direct game-data loading independent of rendering.', '',
              '## Limits and next integration', '',
              'The renderer uses OpenGL 1.1 client arrays with nearest textures, alpha testing and depth '
              'testing. It does not reproduce the PSX ordering table, fixed-point/GTE lighting, fog, '
              'semi-transparent blend modes, animated VRAM, zone culling, terrain subdivision or far-block LOD. '
              'Terrain geometry inherits the exporter\'s center fan approximation and prop rotation assumptions. '
              'Collider boxes are visualization aids; no collision behavior is implied.', '',
              'SDL3 handles windows, OpenGL contexts, events, keyboard/mouse input, gamepad hotplug, '
              'dual-stick development controls and timing. Optional `--music-bank extract/members/059.bin` '
              'with `--collision-exe extract/disc/SCUS_944.23` streams Claude’s portable music engine '
              'through SDL3 (44.1 kHz stereo float). It does not yet route game sound events. '
              'Left stick moves, right stick orbits, Start pauses animation; original gadget controls remain work ahead.', '',
              'The pinned official SDL3 MinGW SDK is 3.4.18; setup_sdl.py verifies its published SHA-256 '
              'and keeps it under ignored extract/codex/deps/. Once present, builds and checks run offline. '
              'A CMake build is available in the top-level CMakeLists.txt; point SDL3_DIR at the SDK’s '
              'lib/cmake/SDL3 directory. Only Windows has been validated.', '',
              'Full machine-readable results: `extract/codex/native/validation.json`.']
    sdl_report=OUT/'sdl-validation.json'
    if sdl_report.exists():
        sdl=json.loads(sdl_report.read_text())
        lines += ['', '## SDL integration checks', '',
                  f"SDL {sdl['sdl_version']}: {sdl['audio_frames']:,} stereo music frames rendered "
                  'through a real SDL audio stream with the dummy driver; missing bank and unavailable '
                  'audio driver fail cleanly. Reproduce with '
                  '`uv run --offline python tools/codex/check_sdl.py`. Physical controllers and audible '
                  'speaker output have not been tested. CMake/MinGW configure and strict Release build '
                  'also succeeded on this host.', '']
    if failures:
        lines += ['', '## Failures', '', *[f'- {item}' for item in failures]]
    terrain_report=OUT/'terrain-material-validation.json'
    if terrain_report.exists():
        terrain=json.loads(terrain_report.read_text())
        lines += ['', '## Terrain texture orientation correction', '',
                  'The native adapter now binds terrain UVs and corner colors to the same world corners '
                  'as original DrawTerrain (0x80043A8C). Original packet geometry copies at '
                  '0x80044A30-0x80044B98 and attribute setup at 0x80044CE0-0x80044E3C were executed '
                  'directly by the test-only instruction oracle, stopping before GTE/draw calls.', '',
                  'For exporter corners X-/Z-, X+/Z-, X-/Z+, X+/Z+, use original UV slots 0,2,1,3 '
                  'and cell color bytes 0,6,1,7. Cell bit2 rotates packet geometry and packet attributes '
                  'together; it does not rotate their world-corner association. The prior native pack '
                  'inherited swapped X+/Z- and X-/Z+ corner UVs and an extra bit2 rotation from the OBJ '
                  'exporter, producing incorrect terrain texture orientation. pack_area.py corrects these '
                  'attributes without modifying the shared exporter or collision geometry.', '',
                  'The terrain atlas is correct: all texels match original VRAM page 0x18, with tile '
                  'i using CLUT (0xF0+i)*0x40|0x37 and source region (i%4,i//4)*64. No atlas fix was needed.', '',
                  f"Original instruction configurations checked: {terrain['original_configurations']} "
                  '(16 tiles ×8 orientations ×2 bit2 states). '
                  f"Real corner comparisons: {terrain['corner_checks']:,}; "
                  f"atlas texel comparisons: {terrain['atlas_texels']:,}; "
                  f"serialized native vertex comparisons: {terrain.get('packed_vertices',0):,}; "
                  f"visible bit2 cells: {terrain['mirrored_cells']:,}. Zero failures. "
                  'Run `uv run --offline python tools/codex/check_terrain_materials.py` to reproduce. '
                  'This establishes terrain corner assignment; prop/actor animation, texture filtering, '
                  'PSX perspective distortion and subdivision parity are separate questions.', '']
    (ROOT/'notes/65-native-area-viewer.md').write_text('\n'.join(lines)+'\n')
    print(f'Validated {len(rows)} areas, {len(captures)} captures, {len(errors)} corruptions; failures={len(failures)}', flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
