"""Instrument actual mixer sends/dry output; compare mixed return to external SPU core."""
import ctypes as C,json,shutil,subprocess,sys,struct
from pathlib import Path
import numpy as np
from check_reverb_pipeline import build_reference
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import render_sfx as rx
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/audio-audit'

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'src/audio/sp_player.c').read_text()
    source='static double *audit_music,*audit_effect;static float *audit_dry;static int audit_frame;\n'+source
    marker='        if(m->music->integer_reverb){'
    assert source.count(marker)==1
    source=source.replace(marker,'''        if(audit_dry){for(int ch=0;ch<2;ch++){
            audit_dry[audit_frame*2+ch]=out[i*2+ch];
            audit_music[audit_frame*2+ch]=m->music->last_wet[ch];
            audit_effect[audit_frame*2+ch]=m->effects->last_wet[ch];
        }audit_frame++;}
'''+marker)
    source+='''
__declspec(dllexport) void audit(float *dry,double *music,double *effect){audit_dry=dry;audit_music=music;audit_effect=effect;audit_frame=0;}
'''
    c=OUT/'shared_reverb_probe.c';c.write_text(source)
    dll=OUT/'shared_reverb_probe.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-Wl,--export-all-symbols',str(c),'-I',str(ROOT/'src/audio'),'-o',str(dll),'-lm','-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));ptr=C.POINTER(C.c_float);dptr=C.POINTER(C.c_double)
    lib.sp_create.restype=C.c_void_p;lib.sp_create.argtypes=[C.c_char_p,C.c_size_t,C.c_char_p,C.c_size_t,C.c_int,C.c_int,C.c_int,C.c_int]
    lib.sp_sfx_create.restype=C.c_void_p;lib.sp_sfx_create.argtypes=[C.c_char_p,C.c_size_t]
    lib.sp_sfx_bank.argtypes=[C.c_void_p,C.c_int,C.c_char_p,C.c_size_t,C.c_char_p,C.c_size_t]
    lib.sp_sfx_play_id.argtypes=[C.c_void_p,C.c_int]
    lib.sp_mix_create.restype=C.c_void_p;lib.sp_mix_create.argtypes=[C.c_void_p,C.c_void_p]
    lib.sp_mix_render.argtypes=[C.c_void_p,ptr,C.c_int]
    lib.audit.argtypes=[ptr,dptr,dptr]
    for name in ('sp_destroy','sp_sfx_destroy','sp_mix_destroy'):getattr(lib,name).argtypes=[C.c_void_p]
    for name in ('sp_sfx_frame_timing','sp_sfx_counter_adsr'):getattr(lib,name).argtypes=[C.c_void_p,C.c_int]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();bank=(ROOT/'extract/members/136.bin').read_bytes();h,vb=rx.bank(0,None)
    ref=build_reference();iptr=C.POINTER(C.c_int16);optr=C.POINTER(C.c_int32)
    ref.init.argtypes=[C.POINTER(C.c_uint16),C.c_int16,C.c_int16];ref.trace.argtypes=[iptr,C.c_int,optr]
    reg=np.array(struct.unpack_from('<33H',exe,0x800B41D0-0x80010000+0x800+3*66),dtype=np.uint16)
    n=44100*8;results=[];wet_counts=[]
    for flags in (56,58):
        p=lib.sp_create(bank,len(bank),exe,len(exe),1,flags,4,10);s=lib.sp_sfx_create(exe,len(exe));assert p and s
        assert lib.sp_sfx_bank(s,0,h,len(h),vb,len(vb))==0
        lib.sp_sfx_frame_timing(s,1);lib.sp_sfx_counter_adsr(s,1);m=lib.sp_mix_create(p,s);assert m
        actual=np.empty((n,2),np.float32);dry=np.empty_like(actual);music=np.empty((n,2));effects=np.empty_like(music)
        lib.audit(dry.ctypes.data_as(ptr),music.ctypes.data_as(dptr),effects.ctypes.data_as(dptr))
        try:
            for off in range(0,n,735):
                if off%44100==0:
                    for effect in range(64):lib.sp_sfx_play_id(s,effect)
                lib.sp_mix_render(m,actual[off:].ctypes.data_as(ptr),min(735,n-off))
        finally:lib.sp_mix_destroy(m);lib.sp_destroy(p);lib.sp_sfx_destroy(s)
        if flags==56:
            wet=np.clip(np.floor((music+effects)*32768),-32768,32767).astype(np.int16)
            returns=np.empty((n,2),np.int32);ref.init(reg.ctypes.data_as(C.POINTER(C.c_uint16)),2560,2560)
            ref.trace(wet.ctypes.data_as(iptr),n,returns.ctypes.data_as(optr))
            expected=(dry.astype(np.float64)+returns/32768.0).astype(np.float32)
            assert np.array_equal(actual,expected),'Shared routing/return differs from reference'
            assert np.any(effects!=0) and np.any(music!=0) and np.any(actual!=dry)
            wet_counts=[int(np.count_nonzero(music)),int(np.count_nonzero(effects))]
        else:assert np.array_equal(actual,dry),'SP_NO_REVERB failed'
        results.append(flags)
    short=exe[:0x800B41D0-0x80010000+0x800+3*66]
    assert not lib.sp_create(bank,len(bank),short,len(short),1,56,4,10)
    report=dict(stereo_frames=n*2,flags=results,wet_nonzero_samples=wet_counts,failures=0,
      scope='Actual music/common-effect wet sends and dry output, shared return vs pinned full external ProcessReverb; no-reverb bypass and truncated preset failure. Source oscillators/routing flag and voice lifetimes are separate fidelity questions.')
    (OUT/'shared-reverb-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
