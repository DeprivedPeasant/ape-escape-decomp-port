"""Real TimeStation/common-effect shared24voice stress and stream chunk checks."""
import ctypes as C,json,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from native_audio.check_player import build
import render_sfx as rx
ROOT=Path(__file__).resolve().parents[2]

def main():
    lib=build('shared_audio');lib.sp_mix_create.argtypes=[C.c_void_p,C.c_void_p];lib.sp_mix_create.restype=C.c_void_p
    lib.sp_mix_render.argtypes=[C.c_void_p,C.POINTER(C.c_float),C.c_int]
    lib.sp_mix_destroy.argtypes=[C.c_void_p]
    lib.sp_mix_stats.argtypes=[C.c_void_p]+[C.POINTER(C.c_int)]*4
    lib.sp_sfx_frame_timing.argtypes=[C.c_void_p,C.c_int]
    lib.sp_sfx_counter_adsr.argtypes=[C.c_void_p,C.c_int]
    bank=(ROOT/'extract/members/136.bin').read_bytes();exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    h,vb=rx.bank(0,None);out=[];maxima=[]
    for block in (735,512):
        p=lib.sp_create(bank,len(bank),exe,len(exe),1,56,4,10);s=lib.sp_sfx_create(exe,len(exe))
        assert p and s and lib.sp_sfx_bank(s,0,h,len(h),vb,len(vb))==0
        lib.sp_sfx_frame_timing(s,1);lib.sp_sfx_counter_adsr(s,1);m=lib.sp_mix_create(p,s);assert m
        data=[];buffer=(C.c_float*(block*2))();left=44100*4;rendered=0
        warm=44100*45
        while warm:
            n=min(warm,block);lib.sp_mix_render(m,buffer,n);warm-=n
        # A dense simultaneous burst exercises matching-id reuse, priority and music eviction.
        for effect in range(64):lib.sp_sfx_play_id(s,effect)
        try:
            while left:
                n=min(left,block);lib.sp_mix_render(m,buffer,n)
                values=np.frombuffer(buffer,dtype=np.float32,count=n*2).copy();assert np.isfinite(values).all()
                data.append(values);rendered+=n;left-=n
                stats=[C.c_int() for _ in range(4)];lib.sp_mix_stats(m,*[C.byref(v) for v in stats])
                assert 0<=stats[0].value<=24 and 0<=stats[1].value<=8 and stats[2].value<=24,[v.value for v in stats]
            maxima.append([v.value for v in stats])
        finally:lib.sp_mix_destroy(m);lib.sp_destroy(p);lib.sp_sfx_destroy(s)
        out.append(np.concatenate(data))
    assert np.array_equal(*out),'Shared mix depends on audio callback chunk size'
    report=dict(frames=rendered,warmup_frames=44100*45,simultaneous_effect_requests=64,chunk_sizes=[735,512],active= max(v[0] for v in maxima),
                effects=max(v[1] for v in maxima),peak=max(v[2] for v in maxima),dropped=max(v[3] for v in maxima),failures=0,
                scope='Real-bank stress, budget and deterministic stream rendering. Original allocator selection separately checked; envelope/voice lifetime, SFX track ordering and source mix are not hardware-exact.')
    (ROOT/'extract/codex/audio-audit/shared-audio-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
