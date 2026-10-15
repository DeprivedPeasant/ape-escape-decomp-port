"""Real actor/area world-indicator loading, timing, failures and SDL presentation."""
import ctypes as C
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from src_paths import src
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from codex.pack_indicators import pack as pack_indicators, registrations, ROOT, OUT, area_members
from codex.build_native_viewer import build
from codex.pack_actor import pack_actor
from codex.pack_area import pack
from codex.pack_ui import pack_ui
from codex.check_indicator_activation import Activation


def main():
    dll = OUT/f'indicator_world_{os.getpid()}.dll'
    sources = ['native_indicator_world', 'native_indicators', 'native_camera_math', 'native_actor', 'native_indicator_runtime', 'native_indicator_trigger', 'native_collision', 'native_ground']
    subprocess.run([shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-shared', *[str(src(s+'.c')) for s in sources],
                    '-o', str(dll), '-lopengl32', '-lm', '-static-libgcc'], check=True)
    lib = C.CDLL(str(dll)); exe_bytes = (ROOT/'extract/disc/SCUS_944.23').read_bytes()
    lib.ae_indicator_world_load.argtypes = [C.POINTER(C.c_void_p), C.c_char_p, C.c_char_p, C.c_size_t, C.c_uint32]
    lib.ae_indicator_world_free.argtypes = [C.c_void_p]
    lib.ae_indicator_world_tick.argtypes = [C.c_void_p]
    lib.ae_indicator_world_error.restype = C.c_char_p
    for name in ('count', 'ticks', 'frame', 'state', 'kind'):
        getattr(lib, 'ae_indicator_world_'+name).argtypes = [C.c_void_p]+([] if name in ('count', 'ticks') else [C.c_uint32])
    lib.ae_indicator_world_clip_frame.argtypes = [C.c_void_p, C.c_uint32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    lib.ae_indicator_world_placement.argtypes = [C.c_void_p, C.c_uint32, C.POINTER(C.c_float), C.POINTER(C.c_float)]
    lib.ae_indicator_world_activate.argtypes = [C.c_void_p, C.c_uint32, C.c_int, C.POINTER(Activation)]
    lib.ae_indicator_world_hint_sector.argtypes = [C.c_void_p, C.c_uint32]
    loaded = unsupported = slots = checks = 0
    activation_checks = 0
    for area, member in sorted(area_members().items()):
        try:
            prepared = pack_indicators(area)
        except ValueError as error:
            assert 'one-joint indicator asset unsupported' in str(error), str(error)
            unsupported += 1
            continue
        w = C.c_void_p()
        assert lib.ae_indicator_world_load(C.byref(w), prepared['pack'].encode(), exe_bytes, len(exe_bytes), area), lib.ae_indicator_world_error()
        try:
            assert lib.ae_indicator_world_count(w) == prepared['count']
            for i, row in enumerate(prepared['records']):
                position = (C.c_float*3)(); yaw = C.c_float()
                assert lib.ae_indicator_world_placement(w, i, position, C.byref(yaw))
                assert list(position) == [row['position'][0]/256, -row['position'][1]/256, row['position'][2]/256]
            for tick in range(180):
                for i, row in enumerate(prepared['records']):
                    expected = tick%(58 if row['kind'] == 2 else 29)
                    assert lib.ae_indicator_world_frame(w, i) == expected and lib.ae_indicator_world_state(w, i) == 0
                    clip = C.c_uint32(); frame = C.c_uint32()
                    assert lib.ae_indicator_world_clip_frame(w, i, C.byref(clip), C.byref(frame))
                    assert (clip.value, frame.value) == (0, expected)
                    checks += 1
                lib.ae_indicator_world_tick(w)
            assert lib.ae_indicator_world_ticks(w) == 180
            for i, row in enumerate(prepared['records']):
                events = Activation()
                assert lib.ae_indicator_world_hint_sector(w, i) == row['hint_sector']
                assert lib.ae_indicator_world_activate(w, i, 0, C.byref(events))
                assert events.sound == 0xF
                assert (events.hint, events.pickups) == ((0, 3) if row['kind'] == 2 else (1, 0))
                assert lib.ae_indicator_world_frame(w, i) == (58 if row['kind'] == 2 else 29)
                assert lib.ae_indicator_world_activate(w, i, 0, C.byref(events))
                assert not events.hint and not events.pickups and not events.sound
            for tick in range(120):
                for i, row in enumerate(prepared['records']):
                    loop = 58 if row['kind'] == 2 else 29
                    duration = 30 if row['kind'] == 2 else 60
                    expected = loop+tick if tick < duration else (tick-duration)%loop
                    assert lib.ae_indicator_world_frame(w, i) == expected
                    clip = C.c_uint32(); frame = C.c_uint32()
                    assert lib.ae_indicator_world_clip_frame(w, i, C.byref(clip), C.byref(frame))
                    assert (clip.value, frame.value) == ((1, tick) if tick < duration else (0, expected))
                    activation_checks += 1
                lib.ae_indicator_world_tick(w)
            loaded += 1; slots += prepared['count']
        finally:
            lib.ae_indicator_world_free(w)
    prepared = pack_indicators(1); original = Path(prepared['pack']).read_bytes()
    malformed = [original[:n] for n in (0, 15, 16, len(original)-1)]+[original+b'X']
    for offset, fmt, value in ((0, '<I', 0), (8, '<I', 2), (12, '<I', 5), (16, '<h', -1)):
        b = bytearray(original); struct.pack_into(fmt, b, offset, value); malformed.append(bytes(b))
    for name in (b'../actor.aea', b'\\actor.aea', b'C:actor.aea', b'absent.aea', b'X'*64):
        b = bytearray(original); b[36:100] = name.ljust(64, b'\0'); malformed.append(bytes(b))
    bad = OUT/'indicators_bad.aei'; failures = 0
    for data in malformed:
        bad.write_bytes(data); w = C.c_void_p()
        assert not lib.ae_indicator_world_load(C.byref(w), str(bad).encode(), exe_bytes, len(exe_bytes), 1)
        assert not w and lib.ae_indicator_world_error()
        failures += 1
    # An otherwise valid actor with a wrong required clip length must fail too.
    actor_path = Path(prepared['records'][0]['actor_pack']); actor = bytearray(actor_path.read_bytes())
    marker = actor.index(b'indicator10_loop\0'); struct.pack_into('<I', actor, marker+64, 28)
    (OUT/'indicator_bad_clip.aea').write_bytes(actor)
    b = bytearray(original); b[36:100] = b'indicator_bad_clip.aea'.ljust(64, b'\0'); bad.write_bytes(b)
    w = C.c_void_p()
    assert not lib.ae_indicator_world_load(C.byref(w), str(bad).encode(), exe_bytes, len(exe_bytes), 1) and not w
    failures += 1

    # SDL+GL native-camera runs use a fixed 30 Hz walking test, not wall time.
    executable = build(ROOT/'src/player'); scene = pack(1); spike = pack_actor(); ui = pack_ui(1)
    args = [str(executable), scene['pack'], '--actor', spike['path'], '--ground', str(ROOT/'extract/members/05B.bin'),
            '--collision-exe', str(ROOT/'extract/disc/SCUS_944.23'), '--player-member', str(ROOT/'extract/members/008.bin'),
            '--ui-pack', ui['pack'], '--native-camera', '--indicators-pack', prepared['pack']]
    traces = []
    for repeat in range(2):
        trace = OUT/f'indicator_walk_{repeat}.json'; image = OUT/f'indicator_walk_{repeat}.ppm'
        r = subprocess.run([*args, '--frames', '90', '--walk-test', str(trace), '--capture', str(image)],
                           cwd=ROOT, env=dict(os.environ, SDL_AUDIO_DRIVER='dummy'), capture_output=True, text=True, timeout=40)
        assert r.returncode == 0, r.stdout+r.stderr
        data = json.loads(trace.read_text()); assert len(data['frames']) == 90
        for tick, row in enumerate(data['frames'], 1):
            assert row['indicator_frames'] == [tick%29]*3 and row['camera_status'] == 0
        traces.append(trace.read_bytes())
    assert traces[0] == traces[1]
    preview_trace = OUT/'indicator_hint_preview.json'
    r = subprocess.run([*args, '--indicator-hint', '0', '--frames', '90',
                        '--walk-test', str(preview_trace), '--capture', str(OUT/'indicator_hint_preview.ppm')],
                       cwd=ROOT, env=dict(os.environ, SDL_AUDIO_DRIVER='dummy'), capture_output=True, text=True, timeout=40)
    assert r.returncode == 0 and 'manual hint preview index=0 sector=14' in r.stdout, r.stdout+r.stderr
    for tick, row in enumerate(json.loads(preview_trace.read_text())['frames'], 1):
        expected = 29+tick if tick < 60 else (tick-60)%29
        assert row['indicator_frames'] == [expected, tick%29, tick%29]
    # The pack index and busy UI cannot silently activate a different marker.
    for extra in (['--indicator-hint', '99'], ['--hint', '14', '--indicator-hint', '0']):
        r = subprocess.run([*args, *extra, '--frames', '1'], cwd=ROOT,
                           capture_output=True, text=True, timeout=30)
        assert r.returncode != 0 and 'indicator hint preview unavailable' in r.stderr, r.stdout+r.stderr
    default = build()
    r = subprocess.run([str(default), scene['pack'], '--indicators-pack', prepared['pack'], '--ui-test',
                        '--frames', '30', '--capture', str(OUT/'indicator_inspection.ppm')], cwd=ROOT,
                       capture_output=True, text=True, timeout=30)
    assert r.returncode == 0, r.stdout+r.stderr
    report = dict(areas_loaded=loaded, areas_with_unsupported_assets=unsupported, indicators_loaded=slots,
                  idle_frame_checks=checks, failure_checks=failures, deterministic_demo_frames=180,
                  activation_clip_frame_checks=activation_checks, manual_hint_demo_frames=90,
                  rejected_preview_requests=2,
                  strict_default_and_player_builds=True, failures=0)
    (OUT/'indicator-world-validation.json').write_text(json.dumps(report, indent=2)+'\n')
    (ROOT/'notes/port/validation/indicator_world_validation.md').write_text(
        '# World indicator integration validation\n\nGenerated by `check_indicator_world.py`.\n\n'+
        '\n'.join(f'- {k}: {v}' for k, v in report.items())+'\n\n'+
        'Actual prepared actor assets and area placements load; original idle ranges step for180ticks. '
        'Repeated SDL walking traces retain camera status0 and exact indicator frame cycles. '
        'Activation clips and manual hint preview use real hint sectors and the validated local activation routine. '
        'This checks native presentation, not an original gameplay capture. Automatic activation, '
        'player pause/stage transitions/progress/timeattack, shadows and kind1 drawflag remain separate.\n')
    print(report)


if __name__ == '__main__':
    main()
