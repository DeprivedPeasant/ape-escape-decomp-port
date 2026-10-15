"""Whole PlayerUpdate differential execution; dependencies are named test doubles.

Production calls existing hand ports directly. This harness substitutes those
callee interfaces equally on both sides, testing the complete enclosing control
flow rather than independently entered ranges. Integrated real-callee frames are
the responsibility of check_game_area.
"""
import ctypes as C
import csv
import random
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools'), str(ROOT/'tools/mips')]
import mips_ram as mr
import game_call_inventory as inventory
import check_player2 as p2

SOURCE = ROOT/'src/player/ae_playerupdate_frame.c'
CALLBACK = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_uint32, C.c_uint32,
                      C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
PLAYER = 0x800EC1E0
LOCALS = mr.DEFAULT_SP - 120


class NativeMemory:
    def __init__(self, ram):self.ram=ram
    def pointer(self,address):
        return self.ram + (mr.RAM_SIZE+address-mr.SCRATCH_BASE if mr.SCRATCH_BASE<=address<mr.SCRATCH_BASE+mr.SCRATCH_SIZE else address-0x80000000)
    def read8(self,address):return C.c_uint8.from_address(self.pointer(address)).value
    def read16(self,address):return C.c_uint16.from_address(self.pointer(address)).value
    def read32(self,address):return C.c_uint32.from_address(self.pointer(address)).value
    def write8(self,address,value):C.c_uint8.from_address(self.pointer(address)).value=value
    def write16(self,address,value):C.c_uint16.from_address(self.pointer(address)).value=value
    def write32(self,address,value):C.c_uint32.from_address(self.pointer(address)).value=value


def build(source=SOURCE, label='whole'):
    out = ROOT/'extract/checks'
    out.mkdir(parents=True, exist_ok=True)
    protos = inventory.declarations()
    with (ROOT/'tools/verified.csv').open(newline='') as stream:
        addresses = {row['c_function']: int(row['address'], 16) for row in csv.DictReader(stream)}
    called = sorted(set(re.findall(r'\b(ae_\w+)\(', source.read_text())) -
                    {'ae_PlayerUpdate', 'ae_ram_fail', 'ae_ram_failed', 'ae_player_vec_length3'})
    called = [name for name in called if not name.startswith(('ae_rd','ae_wr','ae_sdk'))]
    code = ['#include "../../src/player/ae_playerupdate_frame.h"',
            'typedef uint32_t (*TestCall)(AeRam *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);',
            'static TestCall test_call;',
            'static uint32_t invoke(AeRam *r,uint32_t address,uint32_t n,uint32_t a,uint32_t b,uint32_t c,uint32_t d) { return test_call(r,address,n,a,b,c,d); }']
    specs = {}
    for name in called:
        _, ret, params = protos[name]
        variables = [re.search(r'(\w+)$', p)[1] for p in params]
        ram = next(v for p,v in zip(params,variables) if 'AeRam' in p)
        args = [v for p,v in zip(params,variables) if '*' not in p]
        address = addresses[name]
        specs[address] = (name, len(args))
        code.append(f'{ret} {name}('+', '.join(params)+') {')
        code.extend(f'    (void){v};' for v in variables)
        words = [f'(uint32_t){v}' for v in args] + ['0']*(4-len(args))
        call = f'invoke({ram},0x{address:08X}u,{len(args)},'+','.join(words)+')'
        if name=='ae_PlayerSetMode':
            mode=args[0]
            hook_arg=next(v for p,v in zip(params,variables) if 'AePlayerActHooks' in p)
            call=f'invoke({ram},0x{address:08X}u,{mode} == 1 ? 2 : 1,(uint32_t){mode},{hook_arg} ? {hook_arg}->incoming_a2 : 0,0,0)'
        code.append(('    '+call+';' if ret == 'void' else '    return ('+ret+')'+call+';')+'\n}')
    boundaries = [('XaPlay',0x80011F6C,1,'int32_t'),('CdMgrQueueCommand',0x80012360,4,'int32_t'),
                  ('CdIntToPos',0x80025D20,2,'int32_t'),('CdPosToInt',0x80025E24,1,'int32_t'),
                  ('memset',0x800A3740,3,'int32_t'),('PlayerAnimateAndDraw',0x80063628,0,'void'),
                  ('FUN_80142808',0x80142808,0,'void'),('PlayerCheckGadgetSelect',0x80067974,0,'int32_t')]
    for name, address, n, ret in boundaries:
        specs[address] = (name,n)
        args = [f'uint32_t a{i}' for i in range(n)]
        words = [f'a{i}' for i in range(n)]+['0']*(4-n)
        code.append(f'static {ret} boundary_{name}(void *user,AeRam *r'+(' ,'+','.join(args) if args else '')+') {')
        code.append('    (void)user;')
        call = f'invoke(r,0x{address:08X}u,{n},'+','.join(words)+')'
        code.append(('    '+call+';' if ret == 'void' else '    return '+call+';')+'\n}')
    code.extend([
        'static void indirect(void *u,AeRam *r,uint32_t address) { (void)u; invoke(r,address,0,0,0,0,0); }',
        f'static uint32_t local_frame(void *u,AeRam *r) {{ (void)u;(void)r;return 0x{LOCALS:08X}u; }}',
        'static uint32_t voice_fallback(void *u,AeRam *r) { (void)u;return invoke(r,0xFFFFFF00u,0,0,0,0,0); }',
        'static uint32_t actor_camera_table(void *u,AeRam *r) { (void)u;return invoke(r,0xFFFFFF04u,0,0,0,0,0); }',
        'static void rumble(void *u,AeRam *r,int32_t a,int32_t b,int32_t c) { (void)u;invoke(r,0x80014168u,3,(uint32_t)a,(uint32_t)b,(uint32_t)c,0); }',
        'void test_whole_playerupdate(AeRam *r,TestCall callback) {',
        '    test_call=callback;',
        '    AePlayer2Hooks player2={0}; player2.PadRumble=rumble;',
        '    AePlayerUpdateFrameHooks h={0}; h.player2=&player2;',
        '    h.PlayerModeCallback=indirect; h.PlayerAction=indirect;',
        '    h.PlayerUpdateLocalFrame=local_frame; h.PlayerVoiceFallback=voice_fallback;',
        '    h.PlayerActorCameraTable=actor_camera_table;',
    ])
    code.extend(f'    h.{name}=boundary_{name};' for name,_,_,_ in boundaries)
    code.extend(['    ae_PlayerUpdate(r,&h);','}'])
    specs[0x80014168] = ('PadRumble',3)
    shim = out/f'playerupdate_whole_abi_{label}.c'
    shim.write_text('\n'.join(code))
    dll = shim.with_suffix('.dll')
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    '-static-libgcc',str(source),str(shim),str(ROOT/'src/core/ae_ram.c'),
                    str(ROOT/'src/core/ae_sdk.c'),str(ROOT/'src/core/ae_sdk_matrix.c'),
                    str(ROOT/'src/player/native_player.c'),'-I',str(ROOT/'src/player'),'-o',str(dll)],check=True)
    return dll,specs


class Harness:
    def __init__(self, source=SOURCE, label='whole', gte=None):
        dll,self.specs=build(source,label)
        self.lib=C.CDLL(str(dll))
        self.lib.ae_ram_create.restype=C.c_void_p
        self.lib.ae_ram_failed.argtypes=[C.c_void_p]
        self.lib.ae_ram_error.argtypes=[C.c_void_p];self.lib.ae_ram_error.restype=C.c_char_p
        self.lib.ae_ram_clear_error.argtypes=[C.c_void_p]
        self.lib.ae_ram_destroy.argtypes=[C.c_void_p]
        self.lib.test_whole_playerupdate.argtypes=[C.c_void_p,CALLBACK]
        self.ram=self.lib.ae_ram_create()
        self.cpu=mr.MipsRam.from_extract()
        if gte is None:self.cpu.attach_gte_oracle()
        else:self.cpu.gte_backend,self.cpu.gte_state=gte
        self.cpu.coverage=set()
        self.callback=CALLBACK(self.c_call)
        self.calls=[];self.count=0

    def dependency(self, cpu, address, args):
        self.trace.append((address,*args))
        if address==0x800433B0: return cpu.read32(PLAYER+0x12C)
        if address==0x800797F8: return cpu.read16(PLAYER+0x138)
        if address==0x80064208:
            cpu.write16(PLAYER+0x5E,args[0])
            set_index=0 if args[0]<128 else cpu.read8(PLAYER+0xF2)+1
            cpu.write32(0x1F800048,cpu.read32(0x800F4DE0+set_index*16+4))
            return 0
        if address==0x8007B374: cpu.write16(0x800F44A0,args[0]);return 0
        if address==0x80072170: return 0x80150000
        if address==0x80072220: return 0x80150040 if self.allocate else 0
        if address==0x8008C09C: return 0x80120000 if args[0]==0xA000 else 0x80130000
        if address==0x80025E24: return 1234
        if address==0x80025D20: cpu.write32(args[1],args[0]);return args[1]
        if address==0x800A3740:
            for i in range(args[2]):cpu.write8(args[0]+i,args[1]&255)
            return args[0]
        if address==0x8005DCE4:return self.exit_active
        if address==0x800657F0:return self.enter_water
        if address==0x800652E4:return self.on_ground
        if address==0x80067974:return self.select
        if address in (0x8006699C,0x80066ADC):return self.acceleration
        return 0

    def c_call(self, ram, address, n, a,b,c,d):
        if address==0xFFFFFF00:return self.fallback
        if address==0xFFFFFF04:return self.actor_table
        cpu=NativeMemory(ram)
        result=self.dependency(cpu,address,(a,b,c,d)[:n])
        return result & 0xFFFFFFFF

    def m_call(self, address, n):
        def call(cpu,regs):
            args=tuple(regs[4:4+n])
            if address==0x8007B374 and regs[4]==1:
                self.actor_table=regs[6]
                args=(1,regs[6])
            result=self.dependency(cpu,address,args)
            return result
        return call

    def run(self,image,scratch,controls):
        self.allocate,self.exit_active,self.enter_water,self.on_ground,self.select=controls[:5]
        self.acceleration=controls[5] if len(controls)>5 else self.count%2
        self.actor_table=0
        self.cpu.ram[:]=image;self.cpu.scratch[:]=scratch
        C.memmove(self.ram,image,mr.RAM_SIZE);C.memmove(self.ram+mr.RAM_SIZE,scratch,mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        hooks={addr:self.m_call(addr,n) for addr,(_,n) in self.specs.items()}
        for addr in (0x80120000,0x80120004):hooks[addr]=self.m_call(addr,0)
        for i in range(40):
            addr=self.cpu.read32(0x800B03AC+i*4)
            hooks.setdefault(addr,self.m_call(addr,0))
        self.trace=[]
        self.cpu.call(0x80060780,hooks=hooks)
        expected_trace=self.trace
        self.fallback=next((row[2] for row in expected_trace if row[0]==0x8006896C),0)
        self.trace=[]
        self.lib.test_whole_playerupdate(self.ram,self.callback)
        assert not self.lib.ae_ram_failed(self.ram),self.lib.ae_ram_error(self.ram)
        assert self.trace==expected_trace,(self.count,self.trace,expected_trace)
        actual=C.string_at(self.ram,mr.RAM_SIZE)
        stack=mr.STACK_WINDOW[0]-mr.RAM_BASE if hasattr(mr,'RAM_BASE') else mr.STACK_WINDOW[0]-0x80000000
        if actual[:stack]!=self.cpu.ram[:stack]:
            index=next(i for i in range(stack) if actual[i]!=self.cpu.ram[i])
            raise AssertionError(f'case {self.count}: RAM {index+0x80000000:08X}: {actual[index]:02X} != {self.cpu.ram[index]:02X}')
        actual=C.string_at(self.ram+mr.RAM_SIZE,mr.SCRATCH_SIZE)
        if actual!=bytes(self.cpu.scratch):
            index=next(i for i in range(mr.SCRATCH_SIZE) if actual[i]!=self.cpu.scratch[i])
            raise AssertionError(f'case {self.count}: scratch {index+mr.SCRATCH_BASE:08X}: {actual[index]:02X} != {self.cpu.scratch[index]:02X}')
        self.count+=1

    def close(self):self.lib.ae_ram_destroy(self.ram)


def scenarios(seed=60780,scale=100):
    rng=random.Random(seed)
    for member in ('05B.bin','064.bin','066.bin','05E.bin'):
        base=mr.MipsRam.from_extract(ROOT/'extract/members'/member)
        p2.player_state_tables(base)
        base.write32(0x800B4DC8,0x80120000);base.write32(0x800B4DC0,0x80120004)
        for index in range(300*scale//100):
            cpu=mr.MipsRam.from_extract();cpu.ram[:]=base.ram
            cpu.scratch[:]=rng.randbytes(mr.SCRATCH_SIZE)
            for offset in range(0,0x150,4):cpu.write32(PLAYER+offset,0)
            mode=index%17 if index<34 else rng.choice([0,0,0,5,6,7,10,11])
            cpu.write16(0x800F44A0,mode)
            cpu.write16(0x800F44A2,rng.choice([0,0,0,11]))
            state=rng.choice([0,1,6,8,9,34,36,43,44,45,47,54,70,71,72,73,74,75,76,77,129,132])
            gadget=rng.choice([0,1,3,4,6,7,9])
            if gadget==9 and state>=128:state=0
            cpu.write16(PLAYER+0x5E,state);cpu.write8(PLAYER+0xF2,gadget)
            cpu.write16(PLAYER+0xE8,rng.choice([-1,0,1,5]))
            for axis in range(3):
                cpu.write32(PLAYER+0x24+axis*4,rng.randint(-2000,2000)&0xFFFFFFFF)
                cpu.write16(PLAYER+0x130+axis*2,cpu.read16(PLAYER+0x24+axis*4))
                cpu.write32(PLAYER+0x30+axis*4,rng.randint(-150,150)&0xFFFFFFFF)
                cpu.write32(PLAYER+0x114+axis*4,rng.randint(-150,150)&0xFFFFFFFF)
                cpu.write32(PLAYER+0x108+axis*4,rng.randint(-5000,5000)&0xFFFFFFFF)
            cpu.write32(PLAYER+0x12C,rng.randint(-2000,2000)&0xFFFFFFFF)
            cpu.write32(PLAYER+0x7C,rng.randint(-1000,1000)&0xFFFFFFFF)
            cpu.write16(PLAYER+0x104,rng.randint(-1500,1500)&0xFFFF)
            cpu.write16(PLAYER+0x106,rng.randint(-1500,1500)&0xFFFF)
            cpu.write32(PLAYER+0xE0,rng.choice([0,2,4,8,16,32,64,0x4000000]))
            cpu.write32(PLAYER+0xE4,rng.choice([0,16]))
            cpu.write32(0x800B4DC4,0x80150000)
            cpu.write8(0x8015008E,rng.randrange(3));cpu.write8(0x8015008F,rng.randrange(4))
            cpu.write32(0x800F5120,rng.randrange(16))
            if index%11==0:cpu.write32(PLAYER+0x50,0x80000000)
            for addr in (0x800E6902,0x800E6908,0x800E6916):cpu.write8(addr,rng.choice([0,0,1,2]))
            cpu.write32(0x800E5478,rng.randrange(4096));cpu.write32(0x800E5480,rng.randrange(4096))
            for addr in (0x800E5498,0x800E549A):cpu.write16(addr,rng.choice([0,2,4,8]))
            for addr in (0x800E54A0,0x800E54A2):cpu.write16(addr,rng.randrange(257))
            for addr in (0x800E54B4,0x800E54BC,PLAYER+0x60,PLAYER+0x68,PLAYER+0x74,PLAYER+0x80,PLAYER+0xD8):
                for lane in range(4):cpu.write16(addr+lane*2,rng.randint(-4096,4096)&0xFFFF)
            for addr in (0x800E54CC,0x800E54D0,0x800E54DC):cpu.write16(addr,rng.randint(-4096,4096)&0xFFFF)
            cpu.write16(0x800E54CE,rng.randint(-4096,4096)&0xFFFF)
            cpu.write16(PLAYER+0x138,rng.choice([0,1,2,0x1000,0x1001,0x1002,0x1003]))
            cpu.write16(0x1F8003F8,rng.randrange(16))
            cpu.write16(0x800F4DC8,rng.choice([-1,0,1,149,150,299,300,449,450,585,590,599,600])&0xFFFF)
            cpu.write16(0x800F4DCA,rng.randrange(2))
            cpu.write16(0x800F51E4,rng.choice([0,1,10,11,28,30]))
            cpu.write32(0x800EC350,index)
            cpu.write32(0x800EC034,256);cpu.write32(0x800EC02C,256)
            cpu.write32(0x800EC014,24)
            cpu.write16(PLAYER+0xEE,rng.randrange(2))
            cpu.write32(PLAYER+0x70,rng.choice([0,48,160,192,224]))
            cpu.write16(PLAYER+0x102,rng.randrange(2))
            cpu.write16(PLAYER+0x8A,rng.randrange(4096));cpu.write16(PLAYER+0x8E,rng.choice([2048,4096]))
            cpu.write16(PLAYER+0x8C,rng.choice([0,1,5,65534,65535]))
            table=cpu.read32(0x800F4DE4) if state<128 else cpu.read32(0x800F4DE0+(gadget+1)*16+4)
            cpu.write8(table+(state&127)*8+4,rng.randrange(5)*16+rng.randrange(11))
            cpu.write8(table+(state&127)*8+5,index%17)
            cpu.write8(table+(state&127)*8+6,index%40)
            # Seed every original local, including the unwritten normal/padding.
            for offset in range(0,88,4):cpu.write32(LOCALS+offset,rng.getrandbits(32))
            yield bytes(cpu.ram),bytes(cpu.scratch),(index%2,index%2,index%7==0,index%2,index%5==0)


def focused_scenarios():
    for member in ('05B.bin','064.bin','066.bin','05E.bin'):
        base=mr.MipsRam.from_extract(ROOT/'extract/members'/member)
        p2.player_state_tables(base)
        base.write16(0x800F44A0,0);base.write16(0x800F44A2,0)
        base.write16(PLAYER+0xE8,5);base.write8(PLAYER+0xF2,9)
        base.write32(PLAYER+0xE0,0);base.write32(PLAYER+0xE4,0)
        base.write32(PLAYER+0x50,0)
        base.write32(0x800B4DC4,0x80150000)
        base.write32(0x800B4DC8,0x80120000);base.write32(0x800B4DC0,0x80120004)
        base.write16(PLAYER+0x60,4096);base.write16(PLAYER+0x68+2,4096)
        base.write32(PLAYER+0x12C,1000)
        base.write32(PLAYER+0x28,0)
        base.write16(0x800E5498,2);base.write16(0x800E54A0,128)
        base.write32(0x800E5478,0);base.write32(0x800E5480,0)
        base.write32(0x800EC034,256);base.write32(0x800EC02C,256)
        base.write16(0x800E54B4,0);base.write16(0x800E54B8,0)
        for addr in (0x800E6902,0x800E6908,0x800E6916):base.write8(addr,0)
        table=base.read32(0x800F4DE4)
        def emit(writes,controls=(1,1,0,1,0)):
            cpu=mr.MipsRam.from_extract();cpu.ram[:]=base.ram;cpu.scratch[:]=base.scratch
            for size,address,value in writes:getattr(cpu,f'write{size}')(address,value&((1<<size)-1))
            return bytes(cpu.ram),bytes(cpu.scratch),controls
        # Cross all facing, movement and tilt modes in the original enclosing flow.
        for move in range(17):
            for turn in range(11):
                yield emit([(16,PLAYER+0x5E,0),(8,table+4,(move%5)*16+turn),
                            (8,table+5,move),(8,table+6,0),(16,PLAYER+0x8A,4095),
                            (32,PLAYER+0x7C,0),(16,0x800F5126,1)])
        # Idle stream starts, repeated variant, ready, loading, reset and movement.
        for timer in (0,899,900,901,65535):
            for ready,loading in ((0,0),(1,0),(0,1)):
                yield emit([(16,0x800E5498,0),(16,0x800E549A,0),(16,PLAYER+0x5E,0),
                    (16,PLAYER+0x148,timer),(8,PLAYER+0x14A,loading),(8,PLAYER+0x14B,ready),
                    (16,PLAYER+0x130,0),(16,PLAYER+0x132,0),(16,PLAYER+0x134,0),
                    (32,0x800F4620,0),(32,0x800EC350,0),(32,0x800B4D70,0x80130000),
                    (32,0x80130080,0x80120000)])
        for axis in range(3):
            yield emit([(16,0x800E5498,0),(16,0x800E549A,0),(8,PLAYER+0x14B,1),
                (32,0x800B4D70,0x80130000),(32,0x80130080,0x80120000),
                (16,PLAYER+0x130+axis*2,1)])
        # Ground hazards, acceleration state, air recovery and water particles.
        for state in (0,43,47,70,71,72,77):
            for surface in range(16):
                yield emit([(16,PLAYER+0x5E,state),(32,PLAYER+0x28,1000),(16,0x1F8003F8,surface),
                    (16,PLAYER+0xEE,surface%2),(16,PLAYER+0x104,0),(16,PLAYER+0x106,0),
                    (32,PLAYER+0x30,30),(32,PLAYER+0x38,30),(16,PLAYER+0x5C,4),
                    (32,0x800EC350,5),(16,0x800E54CE,0),(8,PLAYER+0xF2,4)])
        for state,gadget in ((129,1),(132,6),(70,9)):
            for value in (-600000,-524288,-262145,-262144,-4097,-1,0,4097,262145,524288,600000):
                yield emit([(16,0x800F44A0,5),(16,PLAYER+0x5E,state),(8,PLAYER+0xF2,gadget),
                    (32,PLAYER+0x34,-8),(32,PLAYER+0x120,value),(32,PLAYER+0x124,value),
                    (32,PLAYER+0x128,value),(16,0x800E54CE,value),(16,PLAYER+0xFC,65535),
                    (16,PLAYER+0xFE,2),(16,0x800F4DCE,2),(32,0x800F516C,0x80150000),
                    (32,0x800F5170,0x80150000),(32,0x8015002C,1)])
        for timer,held,gesture in ((0,0,0),(1,0,0),(2,0,0),(10,1,0),(10,0,1)):
            yield emit([(16,0x800F44A0,10),(16,PLAYER+0x148,timer),(32,0x800E5478,held),
                        (32,0x800E5498,gesture)])
        for flags in (4,8):
            for fall in (0,1,2):
                yield emit([(32,PLAYER+0xE0,16),(32,0x800F5120,flags),(8,0x8015008E,fall),
                            (8,0x8015008F,0)])
        yield emit([(32,PLAYER+0x50,0x80000000),(16,0x800F4476,87)])
        # Exact branch thresholds and retained byte padding.
        for acceleration in (0,1):
            for frame in (0,32):
                yield emit([(16,PLAYER+0x5E,0),(32,PLAYER+0x28,1000),(16,0x1F8003F8,10),
                    (8,PLAYER+0xEE,1),(8,PLAYER+0xEF,171),(32,PLAYER+0x70,48),
                    (32,0x800EC350,frame)],(1,1,0,1,0,acceleration))
        yield emit([(32,PLAYER+0x28,1000),(16,0x1F8003F8,4),(32,PLAYER+0xE0,0x4000000),
                    (16,PLAYER+0x104,0),(16,PLAYER+0x106,-1)])
        for boost in (0,1):
            yield emit([(16,PLAYER+0x5E,70),(8,table+70*8+5,12),(32,0x800E5480,10),
                        (16,0x800F51E4,boost)])
        for stick in (8,9):
            yield emit([(16,PLAYER+0x5E,72),(32,PLAYER+0x28,1000),(16,0x1F8003F8,0),
                (16,PLAYER+0x5C,4),(32,0x800EC350,stick)],(1,1,0,1,0))
        for blend in (0,4096):
            net_table=base.read32(0x800F4DE0+2*16+4)
            yield emit([(16,0x800F44A0,5),(16,PLAYER+0x5E,129),(8,PLAYER+0xF2,1),
                        (16,0x800E54CE,0),(16,PLAYER+0x8A,blend),(16,PLAYER+0x88,base.read16(net_table+18))])
        yield emit([(16,0x800F44A0,5),(8,PLAYER+0xF2,0),(32,0x800F516C,0x80150000),
                    (32,0x800F5170,0x80150000),(32,0x8015002C,1)])
        yield emit([(32,PLAYER+0x34,-1),(32,PLAYER+0x118,7)])


def main():
    h=Harness()
    try:
        import itertools
        for scenario in itertools.chain(scenarios(),focused_scenarios()):
            try:h.run(*scenario)
            except Exception:
                print('failed case',h.count,'trace',[(hex(row[0]),*row[1:]) for row in h.trace])
                print('player differences',[(hex(PLAYER+i),int.from_bytes(C.string_at(h.ram+PLAYER-0x80000000+i,4),'little'),h.cpu.read32(PLAYER+i)) for i in range(0,336,4) if C.string_at(h.ram+PLAYER-0x80000000+i,4)!=bytes(h.cpu.ram[PLAYER-0x80000000+i:PLAYER-0x80000000+i+4])])
                import os
                os.environ['MIPS_BACKEND']='py'
                try:h.run(*scenario)
                except Exception:print('original last machine',getattr(h.cpu,'_last_machine',None))
                raise
        covered={pc for pc in h.cpu.coverage if 0x80060780<=pc<0x80063628}
        gaps=[pc for pc in range(0x80060780,0x80063628,4) if pc not in covered]
        print(f'PlayerUpdate: {h.count} whole-function cases; {len(covered)}/2986 instructions; gaps='+','.join(f'{pc:08X}' for pc in gaps))
    finally:h.close()


MUTATIONS = [
    ('idle threshold','timer <= 900','timer <= 901'),
    ('idle allocation','0xA000u','0x9000u'),
    ('idle variant stride','variant * 20u','variant * 16u'),
    ('idle mode','h->act, 10);','h->act, 9);'),
    ('stage exit kind','area == 87 ? 33 : 16','area == 87 ? 16 : 33'),
    ('contact tag','>> 28) == 8','>> 28) == 7'),
    ('attachment ground mask','PU_PAD_HELD) & 10u','PU_PAD_HELD) & 8u'),
    ('restore vertical threshold',') >= 8 && velocity_y',') >= 9 && velocity_y'),
    ('restore negative rounding','PU_VECTOR_Z) / 4','PU_VECTOR_Z) >> 2'),
    ('wall drop','(uint32_t)-110','(uint32_t)-109'),
    ('water recovery','air += 15','air += 14'),
    ('air damage','if (air < 1)','if (air < 0)'),
    ('turn angle','mode == 2 ? 341 : 113','mode == 2 ? 113 : 341'),
    ('swim pitch step','pitch - 32u','pitch - 31u'),
    ('move drag','4000u : 3800u','3999u : 3800u'),
    ('move scale','speed >> 10','speed >> 9'),
    ('animation fraction','fraction & 4095u','fraction & 2047u'),
    ('dive velocity','(uint32_t)-40960','(uint32_t)-40959'),
    ('swim boost timer','PU_SWIM_BOOST, 30','PU_SWIM_BOOST, 29'),
    ('slope correction','* 56));','* 55));'),
    ('hazard state','h->act, 77);','h->act, 76);'),
    ('hazard damage','ae_PlayerDamage(r, 1);','ae_PlayerDamage(r, 2);'),
    ('tilt average','slope /= mode == 1 ? 4 : 2','slope /= mode == 1 ? 2 : 4'),
    ('net body kind','h->body, 8, body','h->body, 6, body'),
    ('flyer body owner','h->body, 6, body, PU_PLAYER','h->body, 6, body, 0'),
    ('horizontal clamp','524288 : 262144','524288 : 262143'),
    ('fall timer','== 524288 ?','== 524287 ?'),
    ('renderer attachment clear','PU_PLAYER + PU_ATTACH_FLAGS, 0);','PU_PLAYER + PU_ATTACH_FLAGS, 1);'),
    ('fraction reconstruction','(uint32_t)velocity << 12','(uint32_t)velocity << 11'),
    ('blend cleanup','blend >= 65534','blend >= 65535'),
    ('actor camera table','actor.incoming_a2 = table;','actor.incoming_a2 = table + 1u;'),
]


def mutate(cases,gte):
    source=SOURCE.read_text()
    for index,(label,before,after) in enumerate(MUTATIONS):
        assert before in source,label
        path=ROOT/'extract/checks'/f'playerupdate_whole_mutant_{index}.c'
        path.write_text(source.replace(before,after,1))
        h=Harness(path,f'mutant_{index}',gte)
        try:
            for scenario in cases:h.run(*scenario)
        except AssertionError:print('caught whole:',label)
        else:raise AssertionError('whole mutation survived: '+label)
        finally:h.close()
    print(f'{len(MUTATIONS)} PlayerUpdate mutations caught')


if __name__=='__main__':main()
