"""Compile the standalone C VM and compare every update with the Python reference.

Uses installed GCC and standard-library ctypes; generated DLL/results stay ignored.
No game bytes are embedded into source or generated notes.
"""
import ctypes as ct
import json
import random
import shutil
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from codex.effect_vm import ParticleVM,SEEDS
import dump_effect_scripts as scripts
ROOT=scripts.ROOT
EVENT=ct.CFUNCTYPE(None,ct.c_void_p,ct.c_uint32,ct.c_int32,ct.POINTER(ct.c_uint8))

class RecordingVM(ParticleVM):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs);self.event_trace=[]
    def instruction(self,op,args,next_pc):
        event=3 if op==52 else 1 if op==51 and self.ground<self.get16(22) else 2 if op==51 and self.water<self.get16(22) else 0
        if event:self.event_trace.append((event,args[0] if event==3 else 0,bytes(self.state)))
        return super().instruction(op,args,next_pc)


def library():
    compiler=shutil.which('gcc')
    if not compiler:raise RuntimeError('installed GCC unavailable; no compiler installation attempted')
    dll=ROOT/'extract/codex/effect_vm.dll'
    subprocess.run([compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(ROOT/'src/effects/effect_vm.c'),'-o',str(dll)],check=True)
    lib=ct.CDLL(str(dll))
    lib.ae_particle_vm_size.restype=ct.c_size_t
    lib.ae_particle_init.argtypes=[ct.c_void_p,ct.c_void_p,*([ct.c_uint32]*4),ct.c_uint16,ct.c_uint8,ct.c_int32,ct.c_int32]
    lib.ae_particle_init.restype=ct.c_int
    lib.ae_particle_step.argtypes=[ct.c_void_p];lib.ae_particle_step.restype=ct.c_int
    lib.ae_particle_snapshot.argtypes=[ct.c_void_p,ct.c_void_p]
    lib.ae_particle_stats.argtypes=[ct.c_void_p,ct.c_void_p]
    lib.ae_particle_set_event_hook.argtypes=[ct.c_void_p,EVENT,ct.c_void_p]
    return lib


def native(lib,blob,start,end,origin,seed,kind,ground,water):
    context=ct.create_string_buffer(lib.ae_particle_vm_size())
    buffer=ct.create_string_buffer(blob)
    assert lib.ae_particle_init(context,buffer,len(blob),start,end,origin,seed,kind,ground,water)==0
    return context,buffer


def compare(lib,blob,start,end,origin,seed,kind=2,ground=32767,water=32767):
    context,buffer=native(lib,blob,start,end,origin,seed,kind,ground,water)
    vm=RecordingVM(blob,start,end,origin,seed,kind,ground,water)
    events=[]
    observer=EVENT(lambda user,event,operand,state:events.append((event,operand,bytes(state[:52]))))
    lib.ae_particle_set_event_hook(context,observer,None)
    state=(ct.c_uint8*52)();stats=(ct.c_uint32*9)()
    compared=0
    reasons={None:0,'END':1,'ground':2,'water':3}
    while vm.state[0] and compared<10000:
        vm.step();status=lib.ae_particle_step(context)
        if status<0:raise AssertionError(f'native error {status}')
        lib.ae_particle_snapshot(context,state);lib.ae_particle_stats(context,stats)
        assert bytes(state)==bytes(vm.state),(hex(origin+start),seed,compared,'state',bytes(state).hex(),vm.state.hex())
        expected=[vm.frames,vm.commands,vm.rng_calls,vm.seed,vm.pc,reasons[vm.termination],
                  vm.events['SpawnParticle_107'],vm.events['EffectSpawnFromDef_B1E20'],vm.events['RibbonDraw']]
        assert events==vm.event_trace,'callback payload/state mismatch'
        assert list(stats)==expected,(hex(origin+start),seed,compared,'stats',list(stats),expected)
        assert status==int(bool(vm.state[0]))
        compared+=1
    assert not vm.state[0],'frame budget exhausted'
    # A dead slot is stable and does not increment frame counters.
    assert lib.ae_particle_step(context)==0
    lib.ae_particle_stats(context,stats);assert stats[0]==compared
    return compared,vm.opcodes


def blob(words):return struct.pack('<'+'H'*len(words),*(w&65535 for w in words))


def fixture(op,rng=None):
    if op in (1,2):return [op,2,*([3] if op==2 else []),8,1,2,3,3]
    if op==3:return [1,2,8,1,2,3,3]
    if op==4:return [4]
    if op in (10,13):return [op,3]
    if op in (11,14):return [op,3,4]
    count=scripts.OPS[op][1]
    # Includes negative/full-width values for packing and sign extension.
    values=[-32768,-1,32767,256,-4096,4096]
    if op in (9,19):values=[-32768,32767,32767,-32768,-1,256]
    if rng is not None:values=[rng.randint(-32768,32767) for _ in range(count)]
    return [op,*values[:count]]


def errors(lib):
    cases=[([0,4],-2),([15,1],-3),([3,4],-4),([1,1,1,1,1,1,4],-4),([8,1,2,3],-1),
           ([1,255,1,255,8,1,2,3,3,3,4],-5)]
    for words,expected in cases:
        b=blob(words);context,buffer=native(lib,b,0,len(b),0,0,2,32767,32767)
        assert lib.ae_particle_step(context)==expected,(words,expected)
    context=ct.create_string_buffer(lib.ae_particle_vm_size());buffer=ct.create_string_buffer(b'xx')
    for start,end,size in ((0,0,2),(0,3,2),(2,1,2)):
        assert lib.ae_particle_init(context,buffer,size,start,end,0,0,2,0,0)==-6
    return len(cases)+3


def main():
    lib=library();inventory=json.loads((ROOT/'extract/codex/effect-scripts.json').read_text())
    cache={};runs=frames=0;opcodes=Counter();failures=[]
    for script in inventory['scripts']:
        source=script['source']
        if source not in cache:
            cache[source]=(ROOT/'extract/disc/SCUS_944.23').read_bytes() if source=='EXE' else (ROOT/f'extract/members/{source}.bin').read_bytes()
        origin=0x80010000-0x800 if source=='EXE' else 0x8013D000
        for seed in SEEDS:
            for ground,water in ((32767,32767),(-32768,32767),(32767,-32768)):
                try:
                    count,ops=compare(lib,cache[source],script['address']-origin,script['end']-origin,
                                      origin,seed,script['types'][0],ground,water)
                    frames+=count;runs+=1;opcodes.update(ops)
                except (AssertionError,ValueError) as error:failures.append(str(error))
        if runs%480==0:print(f'native real runs={runs}, frames={frames}, failures={len(failures)}',flush=True)
    real_opcodes=set(opcodes);synthetic_runs=synthetic_frames=0
    # Every legal opcode, including ten never observed in the extracted pools.
    for op in scripts.OPS:
        for seed in (0,65535,0x1234):
            b=blob(fixture(op)+[4]);count,ops=compare(lib,b,0,len(b),0x81000000,seed)
            synthetic_runs+=1;synthetic_frames+=count;opcodes.update(ops)
    rng=random.Random(94423)
    for _ in range(1000):
        words=[]
        for __ in range(20):
            op=rng.choice(list(scripts.OPS))
            if op==4:continue
            words.extend(fixture(op,rng))
        words.append(4);b=blob(words)
        count,ops=compare(lib,b,0,len(b),0xFFFFFFF0,rng.randrange(65536))
        synthetic_runs+=1;synthetic_frames+=count;opcodes.update(ops)
    assert set(opcodes)==set(scripts.OPS),'legal opcode coverage missing'
    rejected=errors(lib)
    result={'real_scripts':len(inventory['scripts']),'real_runs':runs,'real_frame_snapshots':frames,
            'synthetic_runs':synthetic_runs,'synthetic_frame_snapshots':synthetic_frames,
            'legal_opcodes_checked':len(opcodes),'real_opcodes':len(real_opcodes),
            'diagnostic_cases':rejected,'context_bytes':lib.ae_particle_vm_size(),'failures':failures}
    (ROOT/'extract/codex/native-effect-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    note=['# Native particle bytecode VM (generated)','',
          'Generated by tools/codex/check_native_effects.py, using the installed GCC compiler and Python ctypes. '
          'The C11 sources are original implementations of the documented behavior, not copied decompiled code. '
          'No dependencies are installed; DLL/results remain ignored under extract/codex.','',
          'Run: `uv run --offline python tools/codex/check_native_effects.py`. '
          'Standalone build: `gcc -std=c11 -O2 -Wall -Wextra -Werror -shared '
          'src/effects/effect_vm.c -o extract/codex/effect_vm.dll`. The caller can also compile '
          'the C source directly into its native executable and include effect_vm.h.','',
          'src/effects/effect_vm.c and effect_vm.h expose initialization and one update on a single isolated particle. '
          'The state retains the exact 52-byte little-endian image and PSX 32-bit script/loop addresses; '
          'host pointers stay outside it. Reads are bounded, host alignment is irrelevant, and signed shifts '
          'use defined arithmetic instead of implementation-dependent C shifts. Writes explicitly wrap.','',
          'This runs natively on Windows and has no renderer, allocator, GTE, audio or game-loop dependency. '
          'Its caller supplies a borrowed script buffer, PSX address origin, RNG seed and ground/water thresholds. '
          'It counts ground/water/ribbon events and offers an optional callback with opcode operand and a borrowed state snapshot at the event point. The hook must not mutate/re-enter the context. It does not reproduce the external draw/spawn bodies. '
          'The header describes lifetime, status/error codes and the frame snapshot API.','',
          f'Differential validation: {runs} real runs over {len(inventory["scripts"])} extracted scripts, '
          f'16 seeds and three collision profiles; {frames} frame snapshots. '
          'Every frame compares all 52 bytes, RNG seed/call count, PC, frame/command counts, termination reason '
          'and callback counts against the Python reference. Callback operands and all event-point state bytes are also compared. Dead-slot updates are checked for stability.','',
          f'Additional synthetic validation: {synthetic_runs} runs / {synthetic_frames} frames. '
          f'All {len(opcodes)} legal opcodes execute, including the {len(opcodes)-len(real_opcodes)} absent from real pools. '
          'Deterministic random instruction sequences exercise full signed halfwords, RGB/UV byte wrapping, '
          'size scaling, loops, waits and overflow of PSX address origin. The overflow fixture exposed a missing uint32 mask in the Python loop jump, now corrected; real script results were unaffected. '
          f'{rejected} diagnostic cases cover illegal/truncated streams, bad loop depth, missing terminator, '
          'non-yielding command budget and invalid initial ranges. Compiler warnings are errors.','',
          'Limits: differential agreement establishes consistency with the checked reference, not independent '
          'instruction-for-instruction PSX equivalence. The C VM starts from the same isolated reset state; '
          'shared pool RNG order, partially reused slots, draw-before-update ordering and external callbacks '
          'need integration at the native-port game-loop boundary. Negative error returns are diagnostic stops, '
          'not a claim about the original game handling corrupted data.','',
          f'Failures: {len(failures)}. Context size on this Windows build: {lib.ae_particle_vm_size()} bytes '
          '(host ABI metadata; the game state image itself remains 52 bytes).']
    (ROOT/'notes/64-native-effect-vm.md').write_text('\n'.join(note)+'\n')
    print(result)
    if failures:raise SystemExit(1)

if __name__=='__main__':main()
