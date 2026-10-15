"""Build the SDL3/OpenGL area viewer with installed tools, fully offline.

uv run --offline python tools/codex/build_native_viewer.py [--area 0x01] [--capture] [--run]
"""
import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path
from src_paths import src

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from codex.pack_area import OUT, ROOT, pack
from codex.setup_sdl import locate, VERSION


# Camera leaf / collision / follow / host / render modules, linked only into player builds.
# native_camera_math is always linked (ALWAYS_MODULES) for the world indicators.
CAMERA_MODULES = ['native_camera', 'native_camera_collision',
                  'native_camera_follow', 'native_camera_host', 'native_camera_render',
                  'native_camera_bank', 'native_camera_regions', 'native_camera_select',
                  'native_camera_blend', 'native_camera_focus', 'native_camera_script',
                  'native_camera_motion', 'native_camera_molten', 'native_camera_lifecycle', 'native_camera_stage']
# Indicator state / world presentation and the original SDK math they need, in every build.
# native_sky is the integer sky-prop vertex transform used by AEVIEW3 packs (--sky-props).
ALWAYS_MODULES = ['native_camera_math', 'native_indicators',
                  'native_indicator_runtime', 'native_indicator_trigger', 'native_indicator_world', 'native_sky', 'native_input']


def build(player_source_dir=None):
    compiler = shutil.which('gcc')
    if not compiler:
        raise RuntimeError('installed GCC unavailable; no download or installation attempted')
    OUT.mkdir(parents=True, exist_ok=True)
    sdk=locate()
    # A running Windows demo locks its executable. Build a distinct revision so
    # the user's open window need not be interrupted to prepare an update.
    sources = ['native_viewer.c','native_actor.c','native_actor.h','native_ground.c',
               'native_ground.h','native_collision.c','native_collision.h',
               'native_audio_output.c','native_audio_output.h','spu_gauss.h','native_ui.c','native_ui.h']
    sources += [f'{name}.{ext}' for name in ALWAYS_MODULES for ext in ('c', 'h')]
    player_sources = []
    if player_source_dir is not None:
        player_source_dir = Path(player_source_dir).resolve()
        sources += ['native_player_host.c', 'native_player_host.h']
        sources += [f'{name}.{ext}' for name in CAMERA_MODULES for ext in ('c', 'h')]
        player_sources = [player_source_dir / name for name in ['native_player.c','native_player.h','native_player_body.c','native_player_body.h','native_player_volume.c','native_player_volume.h','native_player_pad.c','native_player_pad.h','native_gadget_slots.c','native_gadget_slots.h']]
        player_sources += [ROOT/'src/core'/name for name in ['ae_ram.c','ae_ram.h','ae_pad.c','ae_pad.h','ae_vecnorm.h']]
    game_sources = [ROOT / line for line in (ROOT/'tools/codex/viewer_game_sources.txt').read_text().splitlines()] if player_source_dir is not None else []
    game_sources += [ROOT/'src/app/viewer_game.c'] if player_source_dir is not None else []
    headers=b''.join(p.read_bytes() for pattern in ('*.h','*.inc')
                     for p in sorted((ROOT/'src').rglob(pattern)))
    digest = hashlib.sha256(VERSION.encode()+headers+b''.join(p.read_bytes() for p in player_sources + game_sources)+Path(__file__).read_bytes()+b''.join(Path(src(n)).read_bytes() for n in sources)
                            +(ROOT/'src/audio/sp_player.c').read_bytes()+(ROOT/'src/audio/sp_player.h').read_bytes()
                            +(ROOT/'src/audio/sp_voice_pool.h').read_bytes()
                            +(ROOT/'src/audio/sp_adsr.h').read_bytes()
                            +(ROOT/'src/audio/sp_reverb.h').read_bytes()
                            +(ROOT/'src/audio/reverb_fir.h').read_bytes()
                            +(ROOT/'src/audio/reverb_pipeline.h').read_bytes()
                            +(ROOT/'src/text/tl_text.c').read_bytes()+(ROOT/'src/text/tl_text.h').read_bytes()).hexdigest()[:16]
    executable = OUT / f'native_viewer_{digest}.exe'
    command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
               str(ROOT/'src/app/native_viewer.c'), str(ROOT/'src/render/native_actor.c'),
               str(ROOT/'src/collision/native_ground.c'),str(ROOT/'src/collision/native_collision.c'),
               str(ROOT/'src/app/native_audio_output.c'),str(ROOT/'src/audio/sp_player.c'),
               str(ROOT/'src/ui/native_ui.c'),str(ROOT/'src/text/tl_text.c'),
               *[str(src(f'{name}.c')) for name in ALWAYS_MODULES], '-o', str(executable),
               '-I',str(sdk/'include'),'-L',str(sdk/'lib'),'-lSDL3','-lopengl32','-lm','-static-libgcc']
    if player_source_dir is not None:
        command += ['-DAE_NATIVE_PLAYER',
                    str(ROOT/'src/app/native_player_host.c'),str(player_source_dir/'native_player.c'),
                    str(player_source_dir/'native_player_body.c'),str(player_source_dir/'native_player_volume.c'),
                    str(player_source_dir/'native_player_pad.c'),str(player_source_dir/'native_gadget_slots.c'),
                    str(ROOT/'src/core/ae_ram.c'),str(ROOT/'src/core/ae_pad.c')]
        command += [str(src(f'{name}.c')) for name in CAMERA_MODULES]
    linked = {str(p.resolve()) for p in player_sources if p.suffix == '.c'}
    command += [str(p) for p in game_sources if str(p.resolve()) not in linked]
    if not executable.exists():
        # Cache individual translation units while iterating on the viewer host.
        cache=OUT/'viewer_obj';cache.mkdir(exist_ok=True)
        units=[arg for arg in command if arg.endswith('.c')]
        flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I',str(sdk/'include')]
        if player_source_dir is not None:flags+=['-DAE_NATIVE_PLAYER']
        objects=[]
        for unit in units:
            key=hashlib.sha256(Path(unit).read_bytes()+headers+repr(flags).encode()).hexdigest()[:20]
            obj=cache/(Path(unit).stem+'_'+key+'.o');objects.append(str(obj))
            if not obj.exists():subprocess.run([compiler,*flags,'-c',unit,'-o',str(obj)],check=True)
        subprocess.run([compiler,*objects,'-o',str(executable),'-L',str(sdk/'lib'),'-lSDL3','-lopengl32','-lm','-static-libgcc'],check=True)
    runtime=OUT/'SDL3.dll'
    if not runtime.exists() or hashlib.sha256(runtime.read_bytes()).digest()!=hashlib.sha256((sdk/'bin/SDL3.dll').read_bytes()).digest():
        shutil.copy2(sdk/'bin/SDL3.dll',runtime)
    (OUT/'native_viewer_latest.txt').write_text(executable.name+'\n',encoding='ascii')
    print(f'Built {executable}', flush=True)
    return executable


def pack_monkey_actor(member):
    """Pack the area's LOD0 monkey model (monkey section header word 2) from the glTF export; None if unavailable."""
    import struct
    from codex.pack_actor import pack_actor
    import export_gltf as gl
    blob = (ROOT/f'extract/members/{member:03X}.bin').read_bytes()
    section = struct.unpack_from('<I', blob, 0x4E4)[0]
    if not section or section + 44 > len(blob) or struct.unpack_from('<I', blob, section)[0] != 0x11:
        return None
    lod0 = section + struct.unpack_from('<i', blob, section + 8)[0]
    gltf = gl.OUT/f'{member:03X}_{lod0:06X}/actor.gltf'
    if not gltf.exists():
        print(f'monkeys: {gltf} missing (run tools/export_gltf.py); continuing without monkeys', file=sys.stderr)
        return None
    return pack_actor(gltf)['path']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', action='store_true', default=True, help='run integrated game host; implies native player')
    parser.add_argument('--legacy-demo', dest='game', action='store_false', help='run the earlier viewer prototypes')
    parser.add_argument('--area', default='0x01', type=lambda s: int(s, 0))
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--spike', action='store_true', help='include animated Spike at original area entry')
    parser.add_argument('--player', action='store_true', help='include Spike and enable collision walking demo')
    parser.add_argument('--native-player', action='store_true', help='use original movement/body prototype; prefer local Claude worktree when available')
    parser.add_argument('--native-camera', action='store_true', help='opt-in original follow camera prototype; implies --native-player')
    parser.add_argument('--stage-camera', action='store_true', help='opt-in stage camera prototype (local regions/scripts/blend; events, aim/fixed, flight incomplete); implies --native-camera')
    parser.add_argument('--player-source-dir', type=Path, help='local immutable cloud module snapshot')
    parser.add_argument('--world-indicators', action='store_true', help='opt-in: draw the area hint indicators (presentation only)')
    parser.add_argument('--keyboard-only', action='store_true', help='explicit controller-free launch: viewer skips SDL gamepad init/enumeration (default unchanged)')
    parser.add_argument('--directinput', action='store_true', help='allow the SDL DirectInput joystick backend (off by default: it can hang startup on some PCs; DualShock 4 uses HIDAPI, Xbox pads XInput)')
    parser.add_argument('--prop-materials', action='store_true', help='opt-in: pack and render source prop semi-transparency/blend flags and untextured prop materials (host OpenGL approximation; see native_prop_rendering.md)')
    parser.add_argument('--sky-props', action='store_true', help='opt-in: pack and draw the original special type 4/5 sky props as a background pass (AEVIEW3; host depth/order approximation, not full GTE parity; see native_sky_rendering.md); implies --prop-materials')
    parser.add_argument('--no-monkeys', action='store_true', help='native player demos include the RAM monkeys in game mode; this turns them off')
    parser.add_argument('--mute', action='store_true')
    parser.add_argument('--hint', type=int, help='open a hint sector at startup')
    parser.add_argument('--indicator-hint', type=int, help='manually preview a world marker hint and its activation animation')
    parser.add_argument('--indicator-proximity', action='store_true', help='opt-in: event/animation prototype (not the complete gimmick protocol); runtime indicator pack stepped from the native player tick; implies --world-indicators and --native-player')
    args = parser.parse_args()
    if args.game: args.native_player = True
    if args.sky_props:
        args.prop_materials = True
    if args.indicator_proximity:
        args.world_indicators = True
        args.native_player = True  # must precede the player source selection below
    if args.stage_camera:
        args.native_camera = True
    if args.native_camera:
        args.native_player = True  # must precede the player source selection below
    player_source = None
    if args.native_player:
        player_source = args.player_source_dir or ROOT/'src/player'
    executable = build(player_source)
    scene = pack(args.area, prop_materials=args.prop_materials, sky_props=args.sky_props)
    actor_options = []
    from codex.pack_ui import pack_ui
    ui=pack_ui(args.area)
    presentation=['--ui-pack',ui['pack'],'--collision-exe',str(ROOT/'extract/disc/SCUS_944.23')]
    if ui['sound_bank'] is not None and not args.mute:
        presentation+=['--music-bank',str(ROOT/f'extract/members/{ui["sound_bank"]:03X}.bin')]
    if args.game:presentation.append('--game')
    if args.keyboard_only:presentation.append('--keyboard-only')
    if args.directinput:presentation.append('--directinput')
    if args.hint is not None:presentation+=['--hint',str(args.hint)]
    if args.world_indicators:
        from codex.pack_indicators import pack as pack_indicators
        presentation+=['--indicators-pack',pack_indicators(args.area,runtime=args.indicator_proximity)['pack']]
        if args.indicator_proximity:presentation.append('--indicator-proximity')
    if args.indicator_hint is not None:
        if not args.world_indicators:parser.error('--indicator-hint requires --world-indicators')
        presentation+=['--indicator-hint',str(args.indicator_hint)]
    if args.spike or args.player or args.native_player:
        from codex.pack_actor import pack_actor
        actor = pack_actor()
        actor_options = ['--actor', actor['path'], '--ground', str(ROOT/f'extract/members/{scene["member"]:03X}.bin'),
                         '--collision-exe',str(ROOT/'extract/disc/SCUS_944.23'),'--focus-actor']
        if args.player or args.native_player:
            actor_options.append('--player')
        if args.native_player:
            actor_options += ['--native-player', '--player-member', str(ROOT/'extract/members/008.bin')]
            if args.native_camera:
                actor_options.append('--native-camera')
            if args.stage_camera:
                actor_options.append('--stage-camera')
            if not args.no_monkeys:
                monkey_actor = pack_monkey_actor(scene['member'])
                if monkey_actor:
                    actor_options += ['--monkey-actor', monkey_actor]
    subprocess.run([str(executable), scene['pack'], '--check', *actor_options,*presentation], check=True,cwd=ROOT)
    if args.capture:
        subprocess.run([str(executable), scene['pack'], '--capture', str(OUT/f'area_{args.area:02X}.ppm'), *actor_options,*presentation], check=True,cwd=ROOT)
    if args.run:
        subprocess.Popen([str(executable), scene['pack'], *actor_options,*presentation], cwd=ROOT)
        print('Started native area viewer. Close the window or press Escape to exit.')


if __name__ == '__main__':
    main()
