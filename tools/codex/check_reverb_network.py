"""Integer 22.05 kHz reverb network vs pinned external test-only implementation."""
import ctypes as C,json,shutil,struct,subprocess
import numpy as np
from check_adsr_counters import ROOT,OUT,DEPS,COMMIT,SHA,section,oracle

def main():
    OUT.mkdir(parents=True,exist_ok=True);oracle() # pinned source verification
    source=(DEPS/'spu.cpp').read_text()
    registers=source[source.index('      u16 FB_SRC_A;'):source.index('      s16 IN_COEF[2];')+len('      s16 IN_COEF[2];')]
    helpers=(section(source,'  static constexpr auto iiasm =')+';'+section(source,'  static constexpr auto neg =')+';').replace('[[unlikely]]','')
    start=source.index('    for (size_t channel = 0; channel < 2; channel++)',source.index('downsampled[channel] = Clamp16'))
    end=source.index('    s_state.reverb_current_address =',start)
    body=source[start:end]
    body=body.replace('s_state.reverb_upsample_buffer[channel][(s_state.reverb_resample_buffer_position >> 1) | 0x20] =', 'output[channel] =')
    body=body.replace('s_state.reverb_upsample_buffer[channel][s_state.reverb_resample_buffer_position >> 1] =','')
    prefix='''#include <cstdint>
#include <cstring>
#include <algorithm>
using u16=uint16_t;using u32=uint32_t;using s16=int16_t;using s32=int32_t;
static int Clamp16(int v){return std::clamp(v,-32768,32767);}
static s16 Truncate16(int v){return static_cast<s16>(v);}
struct Registers {union {struct {REGISTERS};u16 rev[32];};};
struct State {Registers reverb_registers;struct {bool reverb_master_enable=true;} SPUCNT;};
static State s_state;static s16 *memory;static u32 base,cursor;
static s16 ReverbRead(u32 address,s32 adjust=0){u32 v=cursor+(((address<<2)+adjust)&262143u);if(v&262144u)v+=base;return memory[v&262143u];}
static void ReverbWrite(u32 address,s16 data){u32 v=cursor+((address<<2)&262143u);if(v&262144u)v+=base;memory[v&262143u]=data;}
extern "C" __declspec(dllexport) void trace(const uint16_t *reg,uint32_t start,const int16_t *inputs,int n,int16_t *outputs,int16_t *ram){
 std::memcpy(s_state.reverb_registers.rev,reg,64);memory=ram;base=cursor=start;
 for(int i=0;i<n;i++){int downsampled[2]={inputs[i*2],inputs[i*2+1]};int output[2];
'''.replace('REGISTERS',registers)
    suffix='''
 outputs[i*2]=output[0];outputs[i*2+1]=output[1];cursor=(cursor+1)&262143u;if(!cursor)cursor=base;
 }
}
'''
    cpp=OUT/'reverb_oracle.cpp';cpp.write_text(prefix+helpers+body+suffix)
    dll=OUT/'reverb_oracle.dll';subprocess.run([shutil.which('g++'),'-std=c++17','-O2','-shared',str(cpp),'-o',str(dll),'-static-libgcc','-static-libstdc++'],check=True)
    native_source=OUT/'reverb_native_probe.c';native_source.write_text('''#include <string.h>
#include "../../../src/audio/sp_reverb.h"
__declspec(dllexport) void trace(const uint16_t *reg,uint32_t start,const int16_t *inputs,int n,int16_t *outputs,int16_t *ram){
 SpReverb r;memcpy(r.reg,reg,64);r.base=r.cursor=start;r.ram=ram;
 for(int i=0;i<n;i++){int out[2];sp_reverb_step(&r,inputs[i*2],inputs[i*2+1],out);outputs[i*2]=(int16_t)out[0];outputs[i*2+1]=(int16_t)out[1];}
}
''')
    ndll=OUT/'reverb_native_probe.dll';subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',str(native_source),'-o',str(ndll),'-static-libgcc'],check=True)
    libs=[C.CDLL(str(dll)),C.CDLL(str(ndll))]
    for lib in libs:lib.trace.argtypes=[C.POINTER(C.c_uint16),C.c_uint32,C.POINTER(C.c_int16),C.c_int,C.POINTER(C.c_int16),C.POINTER(C.c_int16)]
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();rng=np.random.default_rng(94423);cases=steps=0
    for preset in range(9):
        raw=struct.unpack_from('<33H',exe,0x800B41D0-0x80010000+0x800+preset*66)
        for signal in ('impulse','noise','limits'):
            reg=np.array(raw[1:],dtype=np.uint16);n=131072;inputs=np.zeros((n,2),dtype=np.int16)
            if signal=='impulse':inputs[0]=[32767,-32768]
            elif signal=='noise':inputs[:]=rng.integers(-32768,32768,(n,2),dtype=np.int16)
            else:
                inputs[:]=np.tile(np.array([[-32768,32767],[32767,-32768],[0,0]],dtype=np.int16),(n//3+1,1))[:n]
                reg[[2,8,9]]=32768 # signed coefficient corner cases
            results=[];memories=[]
            initial=np.zeros(262144,dtype=np.int16)
            if signal=='limits':initial[:]=rng.integers(-32768,32768,262144,dtype=np.int16)
            for lib in libs:
                output=np.empty_like(inputs);ram=initial.copy()
                lib.trace(reg.ctypes.data_as(C.POINTER(C.c_uint16)),raw[0]*4,inputs.ctypes.data_as(C.POINTER(C.c_int16)),n,output.ctypes.data_as(C.POINTER(C.c_int16)),ram.ctypes.data_as(C.POINTER(C.c_int16)))
                results.append(output);memories.append(ram)
            bad=np.argwhere(results[0]!=results[1]);assert not len(bad),(preset+1,signal,bad[0],results[0][tuple(bad[0])],results[1][tuple(bad[0])])
            assert np.array_equal(*memories),(preset+1,signal,'RAM differs')
            cases+=1;steps+=n
    report=dict(presets=9,cases=cases,network_steps=steps,output_samples=steps*2,final_ram_words_checked=cases*262144,failures=0,oracle_commit=COMMIT,source_sha256=SHA,
                scope='22.05 kHz integer network outputs and final RAM, original presets plus signed coefficient corners. FIR resampling, shared reverb input, disabled-master mode and hardware capture unvalidated. Not yet enabled in viewer.')
    (OUT/'reverb-network-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
    documentation=f'''# Integer reverb network validation

Generated by `python tools/codex/check_reverb_network.py`; edit the generator and rerun.

`sp_reverb.h` implements the 22.05 kHz network: signed coefficients, intermediate shifts, saturating reflection/all-pass writes, the -32768 coefficient corner cases and SPU work-area address wrapping. It remains separate from the viewer until the 44.1 kHz conversion filters and mixer integration have independent validation.

The test loads all nine reverb presets directly from the original EXE table at 0x800B41D0. Impulse and full-range noise use original registers; stress cases replace IIR/APF coefficients with -32768 and start with random RAM. {cases} cases compare {steps:,} network steps ({steps*2:,} channel output samples) and {cases*262144:,} final RAM words with zero differences. 131072 steps per case exercise work-area wrapping.

Independent oracle: [pinned DuckStation SPU implementation](https://github.com/stenzek/duckstation/blob/{COMMIT}/src/core/spu.cpp). Extracted third-party code is used only in ignored test artifacts. The native core is independently written from arithmetic/register semantics. This corroborates network arithmetic against an emulator, not measured console output. Hardware reference: [PSX-SPX SPU documentation](https://psx-spx.consoledev.net/ps1/spu/soundprocessingunitspu/).

Remaining work: the 39-tap input/output FIR filters, one shared reverb input for music and effects, output-volume arithmetic, disabled-master behavior, hardware RAM interrupts and end-to-end Time Station listening/reference comparison. Existing demo reverb is still the earlier floating-point network and simple resampling. These tests do not claim full hardware audio fidelity.
'''
    (ROOT/'notes/port/validation/audio_reverb.md').write_text(documentation)

if __name__=='__main__':main()
