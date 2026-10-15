"""Integer envelopes vs pinned independent SPU counter implementation (test only).

Third-party oracle source/compiled DLL remain ignored. No emulator code enters port.
"""
import ctypes as C,hashlib,json,random,shutil,struct,subprocess,sys,urllib.request
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_sound as ds,dump_tones as dt
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'extract/codex/audio-audit'
DEPS=ROOT/'extract/codex/deps/duckstation-adsr'
COMMIT='697599c47a646a6cfcc4d246018adf4904d55001'
SHA='0c667291785a7881decb0b8118372851eac47db9f03834bdec386b789847c2bf'

def section(s,marker,semicolon=False):
    start=s.index(marker);cur=s.index('{',start);depth=1;end=cur+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    if semicolon:end+=1
    return s[start:end]

def oracle():
    DEPS.mkdir(parents=True,exist_ok=True);path=DEPS/'spu.cpp'
    if not path.exists():path.write_bytes(urllib.request.urlopen(f'https://raw.githubusercontent.com/stenzek/duckstation/{COMMIT}/src/core/spu.cpp',timeout=30).read())
    assert hashlib.sha256(path.read_bytes()).hexdigest()==SHA
    s=path.read_text();code=section(s,'struct VolumeEnvelope',True)
    code+=section(s,'void SPU::VolumeEnvelope::Reset(')
    code+=section(s,'ALWAYS_INLINE_RELEASE bool SPU::VolumeEnvelope::Tick(')
    source=OUT/'adsr_oracle.cpp'
    source.write_text('''// Extracted pinned third-party test oracle only, never port source.
#include <cstdint>
#include <algorithm>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using s16=int16_t;using s32=int32_t;
static s16 Truncate16(s32 v){return static_cast<s16>(v);}
#define ALWAYS_INLINE_RELEASE
namespace SPU {'''+code.split('void SPU::VolumeEnvelope::Reset(')[0]+'''}
'''+code[code.index('void SPU::VolumeEnvelope::Reset('):]+'''
extern "C" __declspec(dllexport) void trace(unsigned a,unsigned b,int keyoff,int n,uint32_t *out){
 SPU::VolumeEnvelope env;int phase=0;int16_t level=0;
 auto configure=[&](){
  switch(phase){
  case0:env.Reset((a>>8)&127,127,false,a&32768,false);break;
  case1:env.Reset(((a>>4)&15)*4,124,true,true,false);break;
  case2:env.Reset((b>>6)&127,127,b&16384,b&32768,false);break;
  case3:env.Reset((b&31)*4,124,true,b&32,false);break;
  default:env.Reset(0,0,false,false,false);break;
  }
 };
 configure();
 for(int i=0;i<n;i++){
  if(i==keyoff&&phase!=4&&phase!=3){phase=3;configure();}
  if(phase!=4){
   if(env.counter_increment)env.Tick(level);
   int target=phase==0?32767:phase==1?std::min(32767,int((a&15)+1)*2048):0;
   if(phase!=2&&((env.decreasing&&level<=target)||(!env.decreasing&&level>=target))){++phase;configure();}
  }
  out[i*2]=uint16_t(level)|(phase<<16);out[i*2+1]=env.counter;
 }
}
'''.replace('case0:','case 0:').replace('case1:','case 1:').replace('case2:','case 2:').replace('case3:','case 3:'))
    dll=OUT/'adsr_oracle.dll';subprocess.run([shutil.which('g++'),'-std=c++17','-O2','-shared',str(source),'-o',str(dll),'-static-libgcc','-static-libstdc++'],check=True)
    lib=C.CDLL(str(dll));lib.trace.argtypes=[C.c_uint,C.c_uint,C.c_int,C.c_int,C.POINTER(C.c_uint32)];return lib

def main():
    OUT.mkdir(parents=True,exist_ok=True);ref=oracle()
    source=OUT/'adsr_native_probe.c'
    source.write_text('''#include "../../../src/audio/sp_player.c"
__declspec(dllexport) void trace(unsigned a,unsigned b,int keyoff,int n,uint32_t *out){
 SpAdsr e;sp_adsr_start(&e,(uint16_t)a,(uint16_t)b);
 for(int i=0;i<n;i++){if(i==keyoff)sp_adsr_release(&e);sp_adsr_step(&e);out[i*2]=(unsigned)e.level|((unsigned)e.phase<<16);out[i*2+1]=e.counter;}
 }
__declspec(dllexport) void wrapped(unsigned a,unsigned b,int keyoff,int n,uint32_t *out){
 Env e;env_start_counter(&e,(uint16_t)a,(uint16_t)b);
 for(int i=0;i<n;i++){
  if(i==keyoff)env_key_off(&e);
  if(e.phase==ENV_RELEASE||e.phase==ENV_DONE)env_step_release(&e);else env_step_on(&e);
  unsigned phase=e.phase==ENV_ATTACK_A?0:e.phase==ENV_DECAY?1:e.phase==ENV_SUSTAIN?2:e.phase==ENV_RELEASE?3:4;
  out[i*2]=(unsigned)e.level|(phase<<16);out[i*2+1]=e.counter.counter;
 }
}
''')
    dll=OUT/'adsr_native_probe.dll' ;subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',str(source),'-o',str(dll),'-lm','-static-libgcc'],check=True)
    native=C.CDLL(str(dll));native.trace.argtypes=ref.trace.argtypes;native.wrapped.argtypes=ref.trace.argtypes
    pairs=set();banks=layers=0
    for file in sorted(ds.MEMBERS.glob('*.bin')):
        blob=file.read_bytes();chunks=ds.parse(blob)
        if not chunks or not chunks['ssq']:continue
        banks+=1
        for offset,size in chunks['shd']:
            for program in dt.parse_programs(blob[offset:offset+size]).values():
                for layer in program['layers']:
                    if layer!=dt.EMPTY:pairs.add(struct.unpack_from('<HH',layer,6));layers+=1
    music_pairs=len(pairs);effect_layers=effect_chunks=0
    for file in sorted(ds.MEMBERS.glob('*.bin')):
        blob=file.read_bytes();chunks=ds.parse(blob)
        headers=[blob[o:o+n] for o,n in chunks['shd']] if chunks else ([blob] if blob[12:16]==b'SShd' else [])
        for header in headers:
            if struct.unpack_from("<I",header,0x20)[0]==0xFFFFFFFF:continue
            programs=dt.sfx_programs(header)
            if programs:effect_chunks+=1
            for program in programs.values():
                for layer in program['layers']:
                    if layer!=dt.EMPTY:pairs.add(struct.unpack_from('<HH',layer,6));effect_layers+=1
    cases=[(a,b,5000,16384) for a,b in sorted(pairs)]
    rng=random.Random(94423)
    cases += [(rng.randrange(65536),rng.randrange(65536),rng.choice([-1,0,17,4096]),16384) for _ in range(500)]
    # Long counters, all-ones freeze and exponential threshold boundaries.
    cases += [(0,(rate<<6)|((mode&1)<<14)|((mode>>1)<<15),-1,131072) for rate in [40,41,43,44,47,48,106,118,124,126,127] for mode in range(4)]
    total=0
    for a,b,keyoff,n in cases:
        expected=np.empty(n*2,dtype=np.uint32);actual=np.empty_like(expected)
        args=[a,b,keyoff,n]
        ref.trace(*args,expected.ctypes.data_as(C.POINTER(C.c_uint32)));native.trace(*args,actual.ctypes.data_as(C.POINTER(C.c_uint32)))
        bad=np.flatnonzero(actual!=expected)
        assert not len(bad),(hex(a),hex(b),keyoff,int(bad[0]//2),int(actual[bad[0]]),int(expected[bad[0]]))
        native.wrapped(*args,actual.ctypes.data_as(C.POINTER(C.c_uint32)))
        bad=np.flatnonzero(actual!=expected)
        assert not len(bad),('wrapped',hex(a),hex(b),keyoff,int(bad[0]//2))
        total+=n
    report=dict(oracle='Pinned DuckStation VolumeEnvelope (test-only extraction)',commit=COMMIT,source_sha256=SHA,
                music_banks=banks,music_layers=layers,unique_adsr_pairs=len(pairs),music_unique_adsr_pairs=music_pairs,effect_layers=effect_layers,effect_chunks=effect_chunks,cases=len(cases),sample_steps=total,integrated_wrapper_steps=total,failures=0,
                scope='Positive ADSR register semantics, per-sample level/phase/counter. No hardware capture, negative ENVX writes, volume sweeps or KON latency validation.')
    documentation=f"""# Native audio envelope counter validation

Generated by `python tools/codex/check_adsr_counters.py`; edit the generator and rerun.

The native viewer enables `SP_COUNTER_ADSR` for music and `sp_sfx_counter_adsr(s,1)` for effects. Standalone renderers keep the previous envelope model by default, preserving existing listening exports. Attack, decay, sustain and release use integer levels, stepped counters and phase resets; exponential increase slows at 0x6000. Slow rates have a minimum increment except the all-ones freeze settings. Q15 envelope gain divides by 32768.

Reference: [PSX-SPX SPU register documentation](https://psx-spx.consoledev.net/ps1/spu/soundprocessingunitspu/). Independent test oracle: [DuckStation SPU implementation, pinned {COMMIT}](https://github.com/stenzek/duckstation/blob/{COMMIT}/src/core/spu.cpp). Its source and compiled test harness stay under ignored extract/, and are not shipped with the port. The port implementation is written from register rules. The threshold uses >=0x6000 as the oracle does; documentation prose uses >. This is emulator corroboration, not a hardware boundary measurement.

Real-data coverage: {banks} music banks/{layers} music layers plus {effect_chunks} effect chunks/{effect_layers} effect layers, {len(pairs)} distinct ADSR pairs in total ({music_pairs} from music). {len(cases)} cases compare {total:,} sample steps in both the counter core and integrated Env wrapper, including key-off, phase mapping, long counters and freeze rates. Zero mismatches in level, phase or counter.

`sp_render_wav` accepts an optional final `--native` argument after loops and max seconds to render the viewer clock/counter mode. The current Time Station listening candidate is ignored `extract/native_audio/136_native_counters.wav` (186.3 seconds); older approved exports remain unchanged. It still uses the earlier reverb and has not received fresh listening approval.

Separate real-bank shared-pool stress verifies callback chunk independence and the 24-voice/eight-effect budgets. Legacy 174-effect reference regression remains unchanged. Integer reverb, negative ENVX writes, volume sweeps, KON latency, voice free timing and complete end-to-end hardware fidelity remain unvalidated. The allocator's earlier low-envelope threshold and one-shot lifetime logic are still present; exact counters do not make those policies hardware-exact.
"""
    (ROOT/'notes/port/validation/audio_counters.md').write_text(documentation)
    (OUT/'adsr-counter-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
