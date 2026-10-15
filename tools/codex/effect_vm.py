"""Offline reference semantics for EffectScriptUpdate (one isolated particle).

Uses a 0x34-byte state image and real script bytes. Implements PSX wrapping,
loop counters, waits, random operands, position/velocity/UV/colour updates.
External drawing/spawn calls are recorded as events, not emulated. No game loop.
uv run --offline python tools/codex/effect_vm.py
"""
import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_effect_scripts as scripts

ROOT = scripts.ROOT
SEEDS = [0,1,2,255,256,0x3039,0x7FFF,0x8000,0xFFFF,0xDEAD,0xBEEF,0x1234,0x4321,0x5555,0xAAAA,0xD432]


def signed(v,bits):
    v &= (1<<bits)-1
    return v-(1<<bits) if v & (1<<(bits-1)) else v


class ParticleVM:
    def __init__(self,blob,start,end,origin,seed=0,kind=2,ground=32767,water=32767):
        self.blob,self.start,self.end,self.origin = blob,start,end,origin
        self.pc,self.seed = start,seed & 0xFFFF
        self.state = bytearray(0x34)
        self.state[0],self.state[3] = kind & 255,255
        self.put32(4,origin+start)
        self.put32(0x30,0x2C000000)
        self.put16(0x2A,0x7C)
        self.ground,self.water = ground,water
        self.frames,self.commands,self.rng_calls = 0,0,0
        self.events = Counter()
        self.opcodes = Counter()
        self.termination = None

    def get16(self,offset):
        return struct.unpack_from('<h',self.state,offset)[0]

    def put16(self,offset,value):
        struct.pack_into('<H',self.state,offset,value & 0xFFFF)

    def get32(self,offset):
        return struct.unpack_from('<I',self.state,offset)[0]

    def put32(self,offset,value):
        struct.pack_into('<I',self.state,offset,value & 0xFFFFFFFF)

    def rand8(self):
        self.seed = (self.seed*5+1) & 0xFFFF
        self.rng_calls += 1
        return self.seed >> 8

    def random_offset(self,spread,bias):
        return (self.rand8()*spread >> 8)-bias

    def random_base(self,base,spread):
        return base+(self.rand8()*spread >> 8)

    def set_vector(self,offset,values,add=False):
        for i,value in enumerate(values):
            self.put16(offset+i*2,value+(self.get16(offset+i*2) if add else 0))

    def terminate(self,reason):
        self.state[0] = 0
        self.termination = reason

    def instruction(self,opcode,args,next_pc):
        """Execute one instruction. Returns True to yield this frame."""
        self.pc = next_pc
        s = self.state
        if opcode in (1,2):
            depth = signed(s[3],8)+1
            if not 0 <= depth < 2:
                raise ValueError('LoopDepth error')
            s[3] = depth
            self.put32(8+depth*4,self.origin+next_pc)
            count = args[0] if opcode == 1 else self.random_base(*args)
            s[0x10+depth] = count & 255
        elif opcode == 3:
            depth = signed(s[3],8)
            if not 0 <= depth < 2:
                raise ValueError('END_LOOP without active loop')
            offset = 0x10+depth
            if s[offset]:
                s[offset] -= 1
                if s[offset]:
                    self.pc = (self.get32(8+depth*4)-self.origin) & 0xFFFFFFFF
                    return False
            s[3] = (depth-1) & 255
        elif opcode == 4:
            self.terminate('END')
            return True
        elif opcode == 8:
            self.set_vector(0x14,args,True)
        elif opcode == 9:
            self.set_vector(0x14,[self.random_offset(*args[i:i+2]) for i in (0,2,4)],True)
        elif opcode in (10,11,13,14):
            s[1] = int(opcode in (10,11))
            timer = args[0] if opcode in (10,13) else self.random_base(*args)
            self.put16(0x22,timer-1)
            return True
        elif opcode == 12:
            s[1] = 0
            self.put16(0x22,0)
            return True
        elif opcode == 15:
            self.set_vector(0x1C,args)
        elif opcode in (16,17,18):
            self.put16(0x1C+(opcode-16)*2,args[0])
        elif opcode == 19:
            self.set_vector(0x1C,[self.random_offset(*args[i:i+2]) for i in (0,2,4)])
        elif opcode in (20,21,22):
            self.put16(0x1C+(opcode-20)*2,self.random_offset(*args))
        elif opcode == 23:
            self.set_vector(0x1C,args,True)
        elif opcode == 24:
            self.set_vector(0x1C,[self.get16(0x1C+i*2)*args[i] >> 12 for i in range(3)])
        elif opcode in (25,26,27):
            s[2] = opcode-25
        elif opcode == 30:
            base,delta = [v & 0xFFFF for v in args]
            for offset,value in zip((0x24,0x28,0x2C,0x2E),(base,base+(delta & 0xFF00),base+(delta & 255),base+delta)):
                self.put16(offset,value)
        elif opcode in (31,32):
            for offset in (0x24,0x28,0x2C,0x2E):
                index = offset+(opcode-31)
                s[index] = (s[index]+args[0]) & 255
        elif opcode == 33:
            s[0x12],s[0x13] = args[0] & 255,args[1] & 255
        elif opcode == 34:
            s[0x12] = self.random_base(*args[:2]) & 255
            s[0x13] = self.random_base(*args[2:]) & 255
        elif opcode == 35:
            s[0x12] = s[0x13] = self.random_base(*args) & 255
        elif opcode == 36:
            s[0x12] = (s[0x12]*args[0] >> 12) & 255
            s[0x13] = (s[0x13]*args[1] >> 12) & 255
        elif opcode == 37:
            self.put32(0x30,(self.get32(0x30) & 0xFF000000) | args[0] | args[1]<<8 | args[2]<<16)
        elif opcode == 38:
            # Preserve the decompiler's uint32 OR behavior, including signed base.
            value = (self.get32(0x30) & 0xFFFF00FF) | ((args[0]+((self.rand8()*args[1] >> 8) & 0xFFFF))*0x100)
            self.put32(0x30,value)
        elif opcode in (39,40):
            s[0x1A] = (args[0] if opcode == 39 else self.random_base(*args)) & 255
        elif opcode == 41:
            s[0x33] &= 0xFD
        elif opcode in (42,43,44,45):
            self.put16(0x2A,0x1C+(opcode-42)*0x20)
            s[0x33] |= 2
        elif opcode == 46:
            s[0x33] &= 0xFE
        elif opcode == 47:
            s[0x33] |= 1
        elif opcode in (48,49):
            row = args[0] if opcode == 48 else self.random_base(*args)
            self.put16(0x26,((row+0x1F0)*0x40)|0x2C)
        elif opcode == 51:
            if self.ground < self.get16(0x16):
                self.events['SpawnParticle_107'] += 1
                self.terminate('ground')
                return True
            if self.water < self.get16(0x16):
                self.events['EffectSpawnFromDef_B1E20'] += 1
                self.terminate('water')
                return True
        elif opcode == 52:
            self.events['RibbonDraw'] += 1
        else:
            raise ValueError(f'unimplemented opcode {opcode}')
        return False

    def step(self):
        if not self.state[0]:
            return
        self.frames += 1
        count = 0
        while self.state[0]:
            timer = self.get16(0x22)
            if timer > 0:
                self.put16(0x22,timer-1)
                break
            if not self.start <= self.pc < self.end:
                raise ValueError(f'pc outside stream: +0x{self.pc:X}')
            opcode = struct.unpack_from('<H',self.blob,self.pc)[0]
            if opcode not in scripts.OPS:
                raise ValueError(f'illegal opcode {opcode}')
            operands = scripts.OPS[opcode][1]
            next_pc = self.pc+2*(operands+1)
            if next_pc > self.end:
                raise ValueError('operands outside stream')
            args = struct.unpack_from('<'+'h'*operands,self.blob,self.pc+2) if operands else ()
            self.commands += 1
            self.opcodes[opcode] += 1
            count += 1
            if count > 100000:
                raise ValueError('non-yielding instruction budget exceeded')
            if self.instruction(opcode,args,next_pc):
                break
        self.put32(4,self.origin+self.pc)
        if self.state[0]:
            if self.state[1] == 0:
                self.set_vector(0x14,[self.get16(0x1C+i*2) for i in range(3)],True)
            fade = signed(self.state[0x1A],8)
            if fade > 0:
                for i in range(0x30,0x33):
                    self.state[i] = max(0,min(255,self.state[i]-fade))

    def run(self,max_frames=10000):
        while self.state[0] and self.frames < max_frames:
            self.step()
        if self.state[0]:
            raise ValueError(f'still active after {max_frames} frames')
        return {'frames':self.frames,'commands':self.commands,'rng_calls':self.rng_calls,
                'termination':self.termination,'events':dict(self.events),'final_seed':self.seed,
                'position':[self.get16(0x14+i*2) for i in range(3)],
                'velocity':[self.get16(0x1C+i*2) for i in range(3)],
                'rgb':list(self.state[0x30:0x33])}


def synthetic_checks():
    def run(words,frames=None,seed=0):
        blob = struct.pack('<'+'H'*len(words),*(v & 0xFFFF for v in words))
        vm = ParticleVM(blob,0,len(blob),0,seed)
        if frames is None:
            vm.run()
        else:
            for _ in range(frames):
                vm.step()
        return vm
    # A moving wait of 3 advances position three times; END runs next frame.
    moving = run([15,1,2,3,13,3,4])
    assert moving.frames == 4 and [moving.get16(0x14+i*2) for i in range(3)] == [3,6,9]
    frozen = run([15,1,2,3,10,3,4])
    assert frozen.frames == 4 and frozen.get16(0x14) == 0
    repeated = run([1,3,8,1,0,0,3,4])
    assert repeated.frames == 1 and repeated.get16(0x14) == 3
    nested = run([1,2,1,3,8,1,0,0,3,3,4])
    assert nested.get16(0x14) == 6 and signed(nested.state[3],8) == -1
    zero = run([1,0,8,7,0,0,3,4])
    assert zero.get16(0x14) == 7  # zero loops execute body once, not skip it
    wrapped = run([8,32767,0,0,8,1,0,0,4])
    assert wrapped.get16(0x14) == -32768
    fade = run([37,20,1,255,39,3,13,2,4],frames=2)
    assert list(fade.state[0x30:0x33]) == [14,0,249]
    seed = run([16,0,20,256,0,4],seed=65535)
    assert seed.seed == 65532 and seed.get16(0x1C) == 255
    return 8


def validate_layout():
    """Independently check the proposed Ghidra layout covers each slot byte once."""
    path = ROOT/'tools/codex/particle_state.txt'
    fields, coverage = [], [0]*0x34
    sizes = {'u8':1,'s8':1,'u16':2,'s16':2,'u32':4,'s32':4}
    for raw in path.read_text().splitlines():
        line = raw.split(';',1)[0].strip()
        if not line or line.startswith('#') or not line.startswith('0x'):
            continue
        offset,kind,name = line.split()
        count = 1
        if '[' in kind:
            kind,count_text = kind.split('[',1)
            count = int(count_text[:-1])
        size = (4 if kind.endswith('*') else sizes[kind])*count
        offset = int(offset,16)
        assert 0 <= offset and offset+size <= len(coverage), name
        for index in range(offset,offset+size):
            coverage[index] += 1
        fields.append((offset,size,kind+('['+str(count)+']' if count > 1 else ''),name))
    assert all(n == 1 for n in coverage), 'layout holes or overlapping fields'
    assert 0x800B9900+100*len(coverage) == 0x800BAD50
    return fields


def propose_layout():
    path = ROOT/'notes/codex-inbox.md'
    existing = path.read_text(encoding='utf-8')
    title = 'ParticleState instance layout'
    if title in existing:
        return  # Preserve any Claude status update, and avoid duplicate proposals.
    definition = (ROOT/'tools/codex/particle_state.txt').read_text()
    text = ('\n## 2026-10-06 13:53 AEDT — '+title+'  [status: open]\n'
            'Evidence: notes/54-effect-vm.md, tools/codex/effect_vm.py; private typed export '
            'notes/decomp/codex/effects-typed.c. Slot stride 0x34 / 100 slots from ParticleAlloc '
            'and EffectScriptUpdate; setters and draw packet accesses establish fields. '
            'All 52 bytes covered once, pool ends at allocator cursor 0x800BAD50. '
            'unknown_1b remains unresolved; no speculative name for its purpose.\n'
            'structs / protos (same format as tools/structs.txt):\n```\n'+definition+'```\n'
            'Suggested prototype changes are limited to the return of ParticleAlloc:\n'
            '```\n80069548 ParticleState* ParticleAlloc(void);\n```\n'
            'Keep SpawnOneParticle parameters unchanged pending resolution of its unaligned SVECTOR copy.\n')
    path.write_text(existing+text,encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--max-frames',type=int,default=10000)
    parser.add_argument('--propose',action='store_true',help='append layout proposal to Codex inbox')
    options = parser.parse_args()
    synthetic = synthetic_checks()
    fields = validate_layout()
    inventory = json.loads((ROOT/'extract/codex/effect-scripts.json').read_text())
    profiles = [('no_collision',32767,32767),('ground',-32768,32767),('water',32767,-32768)]
    results,failures = [],[]
    executed = Counter()
    for script in inventory['scripts']:
        source = script['source']
        blob = (ROOT/'extract/disc/SCUS_944.23').read_bytes() if source == 'EXE' else (ROOT/f'extract/members/{source}.bin').read_bytes()
        origin = 0x80010000-0x800 if source == 'EXE' else 0x8013D000
        start,end = script['address']-origin,script['end']-origin
        summary = {'source':source,'address':script['address'],'types':script['types'],'runs':[]}
        for seed in SEEDS:
            for profile,ground,water in profiles:
                vm = ParticleVM(blob,start,end,origin,seed,script['types'][0],ground,water)
                try:
                    result = vm.run(options.max_frames)
                    if profile == 'no_collision' and result['termination'] != 'END':
                        raise ValueError('unexpected collision termination')
                    summary['runs'].append({'seed':seed,'profile':profile,**result})
                    executed.update(vm.opcodes)
                except (ValueError,struct.error) as error:
                    failures.append(f'{source}::{script["address"]:08X} seed={seed} {profile}: {error}')
        results.append(summary)
    all_runs = [r for s in results for r in s['runs']]
    output = ROOT/'extract/codex/effect-vm.json'
    result = {'synthetic_checks':synthetic,'scripts':results,'runs':len(all_runs),
              'opcodes':dict(executed),'failures':failures}
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    note = ['# Particle script reference execution (generated)','',
            'Generated by `uv run --offline python tools/codex/effect_vm.py` after dump_effect_scripts.py.', '',
            'This is an offline reference interpreter for one isolated particle, transcribed from EffectScriptUpdate. '
            'It reads actual script bytes at the pc and operates on a 0x34-byte instance image, including the two loop slots and their pointer/count fields. '
            'It does not replay PS1 gameplay, rendering, shared multi-particle random sequencing, or external spawn callbacks. '
            'Callbacks are recorded as events. Correct completion is a semantic sanity check, not proven instruction-for-instruction equivalence.', '',
            'All arithmetic writes wrap to 8/16/32 bits as in the PSX slots. Random seed updates wrap unsigned 16 bits; operands use signed halfwords where required. '
            'WAIT subtracts one from the count and yields immediately; later updates decrement positive signed timers before interpreting again. '
            'Moving waits integrate velocity each frame; fixed-position waits suppress integration. END suppresses the final integration/fade step. '
            'A LOOP count of zero executes its body once before popping (it does not skip the body). RGB fade acts only when the stored signed byte is positive.', '',
            'Profiles: ground/water=32767 prevents conditional kills; ground=-32768 forces the ground path when opcode 51 executes; '
            'water=-32768 with ground=32767 forces the water path. Initial position, velocity and dimensions are zero, '
            'with command=0x2C, TPAGE=0x7C, loop depth=-1, as an isolated reset state. '
            'SpawnOneParticle does not explicitly initialize every velocity/UV field, so runtime reused slots may have additional state not represented here.', '',
            f'Validation: {len(results)} scripts × {len(SEEDS)} deterministic seeds × {len(profiles)} profiles = {len(all_runs)} completed runs; '
            f'failures={len(failures)}; {sum(executed.values())} executed commands / {len(executed)} observed opcodes.',
            f'Hand-derived semantic fixtures passed: {synthetic} (moving/frozen waits, repeats/nesting/zero count, s16 wrap, RGB clamp, LCG wrap).', '',
            '| Source / script | Runs | Frames min..max | Commands min..max | End reasons |', '|---|---|---|---|---|']
    for s in results:
        runs = s['runs']
        if runs:
            reasons = dict(Counter(r['termination'] for r in runs))
            note.append(f'| {s["source"]}::{s["address"]:08X} | {len(runs)} | {min(r["frames"] for r in runs)}..{max(r["frames"] for r in runs)} | '
                        f'{min(r["commands"] for r in runs)}..{max(r["commands"] for r in runs)} | {reasons} |')
    note += ['', '## Instance layout','',
             'Proposed definition: tools/codex/particle_state.txt. Independent layout validation covers '
             'all 52 bytes exactly once, with no overlap or holes. The 100-slot pool starts at '
             '0x800B9900 and ends at 0x800BAD50, exactly the allocator cursor address. '
             'Field evidence comes from EffectScriptUpdate, SpawnOneParticle, ParticleAlloc and ParticlesDraw. '
             'The byte at +0x1B has no established independent meaning.', '',
             '| Offset | Size | Type | Field |','|---|---|---|---|']
    note += [f'| 0x{off:02X} | {size} | `{kind}` | `{name}` |' for off,size,kind,name in fields]
    note += ['', '## Failures','']+(failures or ['None.'])
    (ROOT/'notes/54-effect-vm.md').write_text('\n'.join(note)+'\n',encoding='utf-8')
    print(f'scripts={len(results)} runs={len(all_runs)} commands={sum(executed.values())} failures={len(failures)} synthetic={synthetic}')
    if failures:
        print('\n'.join(failures[:20]))
        raise SystemExit(1)
    if options.propose:
        propose_layout()


if __name__ == '__main__':
    main()
