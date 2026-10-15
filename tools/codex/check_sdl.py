"""SDL3 window/context/event-loop and audio-stream integration checks.

Uses real extracted music through SDL's dummy driver; does not claim listening
QA or physical-controller testing. No game data or output is committed.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from codex.build_native_viewer import build
from codex.pack_area import OUT,ROOT,pack
from codex.setup_sdl import VERSION


def main():
    executable=build();scene=pack(1)
    command=[str(executable),scene['pack'],'--frames','120','--music-bank',
             str(ROOT/'extract/members/059.bin'),'--collision-exe',str(ROOT/'extract/disc/SCUS_944.23')]
    env=dict(os.environ,SDL_AUDIO_DRIVER='dummy')
    result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,(result.stdout,result.stderr)
    assert 'PLATFORM SDL ' in result.stdout and 'OPENGL ' in result.stdout
    match=re.search(r'AUDIO stereo_float rate=44100 frames=(\d+)',result.stdout)
    assert match and int(match[1])>0,result.stdout
    failures=[]
    for label,driver,bank in [('missing bank','dummy',str(OUT/'missing_music_bank.bin')),
                              ('unavailable audio driver','codex_missing_driver',command[5])]:
        args=list(command);args[5]=bank
        result=subprocess.run(args,env=dict(os.environ,SDL_AUDIO_DRIVER=driver),capture_output=True,text=True,timeout=30)
        assert result.returncode!=0 and 'ERROR:' in result.stderr,(label,result.stdout,result.stderr)
        failures.append(label)
    report=dict(sdl_version=VERSION,audio_frames=int(match[1]),failure_paths=failures,
                physical_controller_tested=False,audible_listening_tested=False,failures=[])
    (OUT/'sdl-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"PASS SDL{VERSION}, {report['audio_frames']} stereo frames, {len(failures)} audio failure paths")


if __name__=='__main__':main()
