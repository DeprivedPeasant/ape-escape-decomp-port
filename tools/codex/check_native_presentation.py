"""Real table/bank coverage and SDL font/message presentation smoke."""
import ctypes as C,json,os,re,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from codex.stage_catalog import catalog,ROOT
from codex.pack_ui import pack_ui
from codex.pack_area import pack,OUT
from codex.build_native_viewer import build
from native_audio.check_player import build as audio_build
import render_sfx as rx

def main():
    rows=catalog();lib=audio_build('presentation_audio');exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    banks=sorted({r['sound_bank'] for r in rows if r['sound_bank'] is not None});checked=valid=0
    samples=(C.c_float*2048)();effect_rows=rx.sfx_table(exe)
    for member in banks:
        mixer=lib.sp_sfx_create(exe,len(exe));assert mixer
        try:
            try:h,vb=rx.bank(2,f'{member:03X}')
            except SystemExit:continue
            assert lib.sp_sfx_bank(mixer,2,h,len(h),vb,len(vb))==0
            for i,(slot,prog,note) in enumerate(effect_rows):
                if slot!=2:continue
                checked+=1
                if lib.sp_sfx_play_id(mixer,i)>=0:valid+=1
            lib.sp_sfx_render(mixer,samples,1024)
            assert all(abs(v)<1e5 for v in samples)
        finally:lib.sp_sfx_destroy(mixer)
    binary=build();ui=pack_ui(1);scene=pack(1)
    env=dict(os.environ,SDL_AUDIO_DRIVER='dummy')
    args=[str(binary),scene['pack'],'--ui-pack',ui['pack'],'--music-bank',str(ROOT/'extract/members/059.bin'),
          '--hint','0','--effect','0','--ui-test','--frames','30','--capture',str(OUT/'presentation.ppm')]
    run=subprocess.run(args,env=env,capture_output=True,text=True,timeout=30,check=True)
    assert 'UI RESULT state=3 hint=0' in run.stdout,run.stdout
    match=re.search(r'AUDIO stereo_float rate=44100 frames=(\d+)',run.stdout);assert match and int(match[1])>0
    report=dict(areas=len(rows),named_areas=sum(bool(r['name']) for r in rows),music_areas=sum(r['sound_bank'] is not None for r in rows),
                unique_music_banks=len(banks),stage_effect_ids_checked=checked,valid_stage_effects=valid,
                hint_sectors=ui['hint_sectors'],nonempty_hints=ui['nonempty_hints'],texture_pages=ui['pages'],
                hint_prompt_state=3,audio_frames=int(match[1]),failures=0,
                scope='Original tables and real banks; synthetic SDL hint-page/common-effect smoke. Shared24voice mixer enabled; automatic gameplay sound/indicator triggers and hardware voice lifetime remain pending; integer/shared reverb is enabled.')
    (OUT/'presentation-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
