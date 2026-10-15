"""SDL integration smoke test; synthetic inputs are not an original gameplay capture."""
import json
import subprocess
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from codex.build_native_viewer import build
from codex.pack_area import pack,ROOT,OUT
from codex.pack_actor import pack_actor

def main():
    source=ROOT/'src/player';exe=build(source);scene=pack(1);actor=pack_actor()
    trace=OUT/'original-player-smoke.json';capture=OUT/'original-player-smoke.ppm'
    command=[str(exe),scene['pack'],'--actor',actor['path'],'--ground',str(ROOT/'extract/members/05B.bin'),
             '--collision-exe',str(ROOT/'extract/disc/SCUS_944.23'),'--native-player','--focus-actor',
             '--walk-test',str(trace),'--frames','120','--capture',str(capture)]
    result=subprocess.run(command,cwd=ROOT,text=True,capture_output=True,check=True)
    (OUT/'original-player-smoke.log').write_text(result.stdout+result.stderr)
    frames=json.loads(trace.read_text())['frames'];assert len(frames)==120
    states={f['state'] for f in frames};assert {3,5,8,9}<=states,states
    assert all(f['update_flags']==0 for f in frames)
    assert all(f['body_flags']==0 for f in frames), 'body callback omission reached'
    assert all(f['volume_flags']==0 for f in frames), 'volume query failed closed'
    assert frames[60]['vel'][1]<0,'jump never rises'
    assert any(f['vel'][1]>0 for f in frames[65:90]),'jump never falls'
    assert frames[89]['xyz'][1]==frames[0]['xyz'][1] and frames[89]['vel'][1]==0,'no landing'
    for f in frames:
        name=actor['clips'][f['clip']]['name'];prefix=f"set0_state{f['state']:02x}"
        assert name==prefix or name.startswith(prefix+'_'),(f,name)
    report=dict(source=subprocess.run(['git','-C',str(ROOT),'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip(),frames=len(frames),
                states=sorted(states),update_flags=0,body_flags=0,volume_flags=0,failures=0,
                limitation='Synthetic integration smoke only; no original gameplay replay. Collider-volume and mode-0 ray callbacks wired; original trace/body replay, gameplay sound/effects and animation interpolation pending.')
    (OUT/'original-player-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)

if __name__=='__main__':main()
