"""Combined reverb vs complete pinned external ProcessReverb (ignored oracle)."""
import ctypes as C,json,shutil,struct,subprocess
import numpy as np
from check_adsr_counters import ROOT,OUT,DEPS,COMMIT,SHA,section,oracle

def build_reference():
    oracle();s=(DEPS/'spu.cpp').read_text()
    regs=s[s.index('      u16 FB_SRC_A;'):s.index('      s16 IN_COEF[2];')+len('      s16 IN_COEF[2];')]
    code='\n'.join(section(s,m) for m in ('u32 SPU::ReverbMemoryAddress(', 's16 SPU::ReverbRead(', 'void SPU::ReverbWrite(', 'void SPU::ProcessReverb('))
    code=code.replace('SPU::','').replace('[[unlikely]]','')
    prefix='''// Third-party test oracle only; never enters production source.
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <array>
using u32=uint32_t;using u16=uint16_t;using s16=int16_t;using s32=int32_t;using u8=uint8_t;
#define VECTOR_ALIGNMENT 16
constexpr u32 RAM_SIZE=524288;
static s32 Clamp16(s32 v){return std::clamp(v,-32768,32767);}
static s16 Truncate16(s32 v){return static_cast<s16>(v);}
static s32 ApplyVolume(s32 v,s16 gain){return (v*gain)>>15;}
/* Scalar test adapter for upstream vector operations; algorithm is extracted unchanged. */
struct GSVector4i {
 s32 x[4]{};
 template<bool aligned> static GSVector4i load(const void *p){GSVector4i v;std::memcpy(v.x,p,16);return v;}
 template<bool aligned> static GSVector4i loadl(const void *p){GSVector4i v;std::memcpy(v.x,p,8);return v;}
 GSVector4i sll32(int n)const{GSVector4i v;for(int i=0;i<4;i++)v.x[i]=u32(x[i])<<n;return v;}
 GSVector4i sra32(int n)const{GSVector4i v;for(int i=0;i<4;i++)v.x[i]=x[i]>>n;return v;}
 GSVector4i mul32l(GSVector4i b)const{GSVector4i v;for(int i=0;i<4;i++)v.x[i]=x[i]*b.x[i];return v;}
 GSVector4i add32(GSVector4i b)const{GSVector4i v;for(int i=0;i<4;i++)v.x[i]=x[i]+b.x[i];return v;}
 GSVector4i s16to32()const{GSVector4i v;s16 a[8];std::memcpy(a,x,16);for(int i=0;i<4;i++)v.x[i]=a[i];return v;}
 GSVector4i uph64()const{GSVector4i v;v.x[0]=v.x[2]=x[2];v.x[1]=v.x[3]=x[3];return v;}
 s32 addv_s32()const{return x[0]+x[1]+x[2]+x[3];}
};
struct Registers {s16 vLOUT,vROUT;union{struct{REGISTERS};u16 rev[32];};};
struct State {
 Registers reverb_registers{};
 struct{bool reverb_master_enable=true;} SPUCNT;
 u32 reverb_base_address=0,reverb_current_address=0,reverb_resample_buffer_position=0;
 s16 reverb_downsample_buffer[2][128]{},reverb_upsample_buffer[2][64]{};
 s32 last_reverb_input[2]{},last_reverb_output[2]{};
};
static State s_state;static u8 s_ram[RAM_SIZE];
static s16 ReverbRead(u32 address,s32 offset=0);
'''.replace('REGISTERS',regs)
    suffix='''
extern "C" __declspec(dllexport) void init(const u16 *reg,s16 left,s16 right){
 s_state=State{};std::memset(s_ram,0,sizeof(s_ram));std::memcpy(s_state.reverb_registers.rev,reg+1,64);
 s_state.reverb_registers.vLOUT=left;s_state.reverb_registers.vROUT=right;
 s_state.reverb_base_address=s_state.reverb_current_address=u32(reg[0])*4;
}
extern "C" __declspec(dllexport) void trace(const s16 *input,int n,s32 *output){
 for(int i=0;i<n;i++)ProcessReverb(input[i*2],input[i*2+1],output+i*2,output+i*2+1);
}
extern "C" __declspec(dllexport) void ram(s16 *out){std::memcpy(out,s_ram,sizeof(s_ram));}
'''
    cpp=OUT/'pipeline_oracle.cpp';cpp.write_text(prefix+code+suffix)
    dll=OUT/'pipeline_oracle.dll';subprocess.run([shutil.which('g++'),'-std=c++17','-O2','-shared',str(cpp),'-o',str(dll),'-static-libgcc','-static-libstdc++'],check=True)
    return C.CDLL(str(dll))

def main():
    OUT.mkdir(parents=True,exist_ok=True);ref=build_reference()
    source=OUT/'pipeline_native.c';source.write_text('''#include "../../../src/audio/reverb_pipeline.h"
static AeReverbPipeline p;
__declspec(dllexport) void init(const uint16_t *reg,int16_t left,int16_t right){ae_reverb_pipeline_init(&p,reg,left,right);}
__declspec(dllexport) void trace(const int16_t *input,int n,int32_t *output){for(int i=0;i<n;i++)ae_reverb_pipeline_step(&p,input+i*2,output+i*2);}
__declspec(dllexport) void ram(int16_t *out){memcpy(out,p.ram,sizeof(p.ram));}
''')
    dll=OUT/'pipeline_native.dll';subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',str(source),'-o',str(dll),'-static-libgcc'],check=True)
    native=C.CDLL(str(dll));ptr=C.POINTER(C.c_int16)
    for lib in (ref,native):
        lib.init.argtypes=[C.POINTER(C.c_uint16),C.c_int16,C.c_int16];lib.trace.argtypes=[ptr,C.c_int,C.POINTER(C.c_int32)];lib.ram.argtypes=[ptr]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();rng=np.random.default_rng(94423);cases=frames=0
    for preset in range(9):
        reg=np.array(struct.unpack_from('<33H',exe,0x800B41D0-0x80010000+0x800+preset*66),dtype=np.uint16)
        for mode in ('impulse','noise','limits'):
            n=65537;input=np.zeros((n,2),dtype=np.int16);gains=(2560,2560)
            if mode=='impulse':input[0]=[32767,-32768]
            elif mode=='noise':input[:]=rng.integers(-32768,32768,(n,2),dtype=np.int16);gains=(32767,-32768)
            else:input[::2]=32767;input[1::2]=-32768;gains=(-32768,0)
            expected=np.empty((n,2),dtype=np.int32);ref.init(reg.ctypes.data_as(C.POINTER(C.c_uint16)),*gains)
            ref.trace(input.ctypes.data_as(ptr),n,expected.ctypes.data_as(C.POINTER(C.c_int32)))
            expected_ram=np.empty(262144,dtype=np.int16);ref.ram(expected_ram.ctypes.data_as(ptr))
            for block in (735,512):
                native.init(reg.ctypes.data_as(C.POINTER(C.c_uint16)),*gains);actual=np.empty_like(expected)
                for offset in range(0,n,block):native.trace(input[offset:].ctypes.data_as(ptr),min(block,n-offset),actual[offset:].ctypes.data_as(C.POINTER(C.c_int32)))
                bad=np.argwhere(actual!=expected);assert not len(bad),(preset+1,mode,block,bad[0],actual[tuple(bad[0])],expected[tuple(bad[0])])
                actual_ram=np.empty_like(expected_ram);native.ram(actual_ram.ctypes.data_as(ptr));assert np.array_equal(actual_ram,expected_ram),(preset+1,mode,'ram')
                cases+=1;frames+=n
    report=dict(presets=9,cases=cases,stereo_frames=frames,failures=0,chunk_sizes=[735,512],oracle_commit=COMMIT,source_sha256=SHA,
                scope='Full extracted ProcessReverb with scalar vector adapter, zero initial RAM, original presets, impulse/noise/limits and signed output volumes. No hardware capture, disabled master, IRQ or whole-song mix fidelity claim.')
    (OUT/'reverb-pipeline-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
    (ROOT/'notes/port/validation/reverb_pipeline_validation.md').write_text(f'''# Combined reverb validation

Generated by tools/codex/check_reverb_pipeline.py. Sonnet5.5 implemented the composition wrapper; Codex reviewed and independently tested it.

All9originalEXEpresets, {cases} cases and {frames:,} stereo frames match the complete pinned DuckStation ProcessReverb output and final RAM with zero failures. The external function is extracted into ignored test artifacts; a scalar test adapter supplies its vector operations. No external algorithm code is included in the native production headers. Input impulse/noise/limits, signed gains and735/512callbackchunk sizes are covered. Base addresses, RAM, FIR filters and output gain are checked together.

Oracle [pinned SPU source](https://github.com/stenzek/duckstation/blob/{COMMIT}/src/core/spu.cpp). Emulator corroboration is not console measurement: master-disable/IRQ/RAMsharing/whole-song source mix fidelity remain unvalidated. Components are ready for separate mixer integration validation.
''')

if __name__=='__main__':main()
