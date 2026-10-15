"""Sound request differential checker; real EXE + area data, RAM/scratch and ordered hardware hooks.
Run: .venv/Scripts/python.exe tools/mips/check_sound.py [--coverage] [--mutate] [--trapv]
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mips'))
sys.path.insert(0,str(ROOT/'tools'))
import mips_ram as mr
import exclusion_registry as reg
import dump_areas
import dump_sound
import dump_monkeys

# name: entry, bytes, has hooks, arguments, return ('v' means no defined return).
FUNCS={
    'SfxIsQueued':(0x800885A8,84,False,'s','s'),
    'SoundDelayedQueueAdd':(0x800885FC,112,False,'ssu','s'),
    'SoundLoopAllocSlot':(0x800881D0,60,False,'','u'),
    'SoundLoopFree':(0x8008820C,116,False,'ss','s'),
    'SeqSetTickFlag':(0x800A1378,32,False,'s','s'),
    'SpuVolumeClamp':(0x800A119C,96,False,'ss','s'),
    'SfxLookupTone':(0x8009E82C,276,False,'uuu','u'),
    'SfxVoiceAlloc':(0x8009E940,364,False,'uuu','s'),
    'SfxVoiceStart':(0x800A3158,96,True,'uuu','s'),
    'SfxVoiceStartEx':(0x800A31B8,228,True,'uuusss','s'),
    'SfxVoiceStop':(0x800A329C,400,True,'s','v'),
    'SoundEmitterCalcPan':(0x80087C8C,596,False,'usu','s'),
    'PlaySfxAt':(0x80087A20,276,True,'su','s'),
    'SoundLoopStartEx':(0x800882C4,368,True,'sss','s'),
    'SoundLoopStopAndFree':(0x800880D0,80,True,'ss','s'),
}
EXTRA_COVERAGE={'SquareRoot0':(0x800303C0,132), 'VectorNormalSS':(0x800301E0,48),
                'VectorNormal core':(0x80030210,192)}
Idle=C.CFUNCTYPE(C.c_int32,C.c_void_p,C.c_void_p)
Wait=C.CFUNCTYPE(None,C.c_void_p,C.c_void_p,C.c_uint32)
Adsr=C.CFUNCTYPE(None,C.c_void_p,C.c_void_p,C.c_uint32,C.c_uint16,C.c_uint16)
Off=C.CFUNCTYPE(None,C.c_void_p,C.c_void_p,C.c_uint32,C.c_uint16)
Print=C.CFUNCTYPE(None,C.c_void_p,C.c_void_p,C.c_uint32,C.c_uint32,C.c_int32,C.c_int32,C.c_int32)
class Hooks(C.Structure):
    _fields_=[('user',C.c_void_p),('wait_idle',Idle),('wait_loop',Wait),
              ('set_voice_adsr',Adsr),('voice_key_off',Off),('printf_error',Print)]

def build(source=None,trapv=False):
    out=ROOT/'extract/mips'; out.mkdir(parents=True,exist_ok=True)
    dll=out/f'sound_{os.getpid()}_{time.time_ns()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O1' if trapv else '-O2',
                    *(['-ftrapv'] if trapv else []),'-Wall','-Wextra','-Werror','-shared','-static-libgcc',
                    '-I'+str(ROOT/'src/sound'),str(ROOT/'src/core/ae_ram.c'),str(ROOT/'src/core/ae_sdk.c'),
                    str(source or ROOT/'src/sound/ae_sound.c'),'-o',str(dll)],check=True)
    return dll

class Harness:
    oracle=None
    def __init__(self,source=None,trapv=False):
        member=dump_areas.area_members()[1]
        self.m=mr.MipsRam.from_extract(ROOT/f'extract/members/{member:03X}.bin')
        self.base=self.m.snapshot(); self.m.coverage=set()
        if Harness.oracle is None:
            self.m.attach_gte_oracle()
            Harness.oracle=(self.m.gte_state,self.m.gte_backend)
        else: self.m.gte_state,self.m.gte_backend=Harness.oracle
        self.lib=lib=C.CDLL(str(build(source,trapv)))
        P=C.c_void_p; U=C.c_uint32
        lib.ae_ram_create.restype=P; self.r=lib.ae_ram_create()
        lib.ae_ptr.argtypes=[P,U,U]; lib.ae_ptr.restype=P
        lib.ae_ram_clear_error.argtypes=[P]; lib.ae_ram_failed.argtypes=[P]
        lib.ae_ram_error.argtypes=[P]; lib.ae_ram_error.restype=C.c_char_p
        self.main=lib.ae_ptr(self.r,0x80000000,mr.RAM_SIZE)
        self.scr=lib.ae_ptr(self.r,mr.SCRATCH_BASE,mr.SCRATCH_SIZE)
        self.available={n for n in FUNCS if hasattr(lib,'ae_'+n)}
        assert self.available==set(FUNCS), ('Missing C exports',set(FUNCS)-self.available)
        for n in self.available:
            _,_,hw,args,ret=FUNCS[n]
            f=getattr(lib,'ae_'+n)
            f.argtypes=[P]+([P] if hw else [])+[C.c_int32 if k=='s' else U for k in args]
            f.restype=None if ret=='v' else U
        self.hooks=Hooks(); self.log=[]; self.idle=0
        self.keep=[Idle(lambda u,r:self.record('wait_idle',result=self.idle)),
                   Wait(lambda u,r,n:self.record('wait_loop',n)),
                   Adsr(lambda u,r,ch,a1,a2:self.record('set_voice_adsr',ch,a1,a2)),
                   Off(lambda u,r,half,mask:self.record('voice_key_off',half,mask)),
                   Print(lambda u,r,fmt,argc,a,b,c:self.record('printf_error',fmt,argc,*[a,b,c][:argc]))]
        for (n,_),cb in zip(Hooks._fields_[1:],self.keep): setattr(self.hooks,n,cb)
        self.counts={}; self.target=None; self.null_cases=0
        reg.declare('stack_window','801FF000..80200000')

    def record(self,n,*args,result=None):
        self.log.append((n,*args)); return result

    def run(self,n,args=(),setup=lambda m:None,idle=0,expect_trap=False):
        if n not in self.available or self.target and n!=self.target: return
        self.m.restore(self.base); setup(self.m)
        self.m.gte_reset()
        initial=self.m.snapshot()
        C.memmove(self.main,initial[0],mr.RAM_SIZE); C.memmove(self.scr,initial[1],mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.r); self.log=[]; self.idle=idle
        original_log=[]; checkpoints={}
        def original_record(name,*values):
            if name not in checkpoints:
                checkpoints[name]=(bytes(self.m.ram[:0x1FF000]),bytes(self.m.scratch),list(original_log))
            original_log.append((name,*values))
        hooks={0x800A1398:lambda cpu,r:original_record('wait_idle') or idle,
               0x8009D79C:lambda cpu,r:original_record('wait_loop',r[4]) or 0,
               0x800A3720:lambda cpu,r:original_record('printf_error',r[4],
                   1 if r[4]==0x800A627C else 3,*[mr.signed(x) for x in r[5:6 if r[4]==0x800A627C else 8]]) or 0}
        write=self.m.write
        adsr_values={}
        def hw_write(address,size,value):
            if 0x1F801C00<=address<0x1F801E00:
                offset=address-0x1F801C00
                assert size==2, (address,size)
                value &= 0xFFFF
                if offset in (0x18C,0x18E):
                    original_record('voice_key_off',(offset-0x18C)//2,value)
                elif offset%16==8:
                    adsr_values[offset//16]=value
                elif offset%16==10:
                    original_record('set_voice_adsr',offset//16,adsr_values.pop(offset//16),value)
                else: raise AssertionError(f'unexpected SPU write {address:08X}')
            else: write(address,size,value)
        self.m.write=hw_write
        trap=None
        try: result=self.m.call(FUNCS[n][0],args,hooks=hooks)
        except mr.MipsError as e:
            if not expect_trap: raise
            trap=e
        finally: self.m.write=write
        actual=getattr(self.lib,'ae_'+n)(self.r,*([C.byref(self.hooks)] if FUNCS[n][2] else []),*args)
        self.counts[n]=self.counts.get(n,0)+1
        if expect_trap:
            assert trap and 'overflow' in str(trap), (n,trap)
            assert self.lib.ae_ram_failed(self.r) and b'overflow' in self.lib.ae_ram_error(self.r), n
        elif self.lib.ae_ram_failed(self.r): raise AssertionError(self.lib.ae_ram_error(self.r))
        if not expect_trap and FUNCS[n][4]!='v' and result[2]!=(actual&0xFFFFFFFF):
            raise AssertionError(f'{n} {args}: return MIPS {result[2]:08X} C {actual&0xFFFFFFFF:08X}')
        actual_ram=C.string_at(self.main,0x1FF000)
        if actual_ram!=bytes(self.m.ram[:0x1FF000]):
            off=next(i for i,(a,b) in enumerate(zip(actual_ram,self.m.ram)) if a!=b)
            raise AssertionError(f'{n} {args}: RAM {0x80000000+off:08X} MIPS {self.m.ram[off]:02X} C {actual_ram[off]:02X}')
        if C.string_at(self.scr,1024)!=bytes(self.m.scratch): raise AssertionError(n+' scratch')
        if self.log!=original_log: raise AssertionError(f'{n}: hooks {original_log} != {self.log}')
        # Every reached missing callback must stop precisely at its first invocation.
        for index,(field,ctype) in enumerate(Hooks._fields_[1:]):
            if field not in checkpoints: continue
            self.null_cases+=1
            C.memmove(self.main,initial[0],mr.RAM_SIZE); C.memmove(self.scr,initial[1],1024)
            self.lib.ae_ram_clear_error(self.r); self.log=[]
            setattr(self.hooks,field,ctype())
            try:
                getattr(self.lib,'ae_'+n)(self.r,C.byref(self.hooks),*args)
            finally: setattr(self.hooks,field,self.keep[index])
            assert self.lib.ae_ram_failed(self.r), (n,field,'NULL callback did not fail')
            assert b'not provided' in self.lib.ae_ram_error(self.r), (n,field)
            expected_ram,expected_scr,expected_log=checkpoints[field]
            assert C.string_at(self.main,0x1FF000)==expected_ram, (n,field,'writes after missing callback')
            assert C.string_at(self.scr,1024)==expected_scr, (n,field,'scratch after missing callback')
            assert self.log==expected_log, (n,field,'hooks after missing callback')

def scenarios(h,scale=1,seed=94423):
    rng=random.Random(seed)
    for i in range(13):
        def setup(m,i=i):
            for k in range(12):
                m.write_bytes(0x800BF610+8*k,rng.randbytes(8)); m.write16(0x800BF610+8*k,-1 if k==i else 1)
                m.write16(0x800BF612+8*k,9 if k==i else 10)
            m.write32(0x801E0000,0x87651234)
        h.run('SfxIsQueued',(9,),setup)
        for t in [0,1,2,0x10001,-1]:
            h.run('SoundDelayedQueueAdd',(t,0x10009,0x801E0000),setup)
        h.run('SoundDelayedQueueAdd',(1,9,0x800BF610+8*min(i,11)),setup)
        def found(m,i=i):
            setup(m)
            if i<12: m.write16(0x800BF610+8*i,0)
        h.run('SfxIsQueued',(0x10009,),found)
    def zero_type(m):
        for i in range(12): m.write16(0x800BF610+8*i,0)
        m.write32(0x801E0000,0x87651234)
    h.run('SoundDelayedQueueAdd',(1,9,0x801E0000),zero_type)
    for i in range(25):
        def setup(m,i=i):
            for k in range(24):
                m.write_bytes(0x800BF370+28*k,rng.randbytes(28))
                m.write16(0x800BF370+28*k,-1 if k==i else 7)
                m.write16(0x800BF372+28*k,8 if k==i else 9)
        h.run('SoundLoopAllocSlot',(),setup)
        h.run('SoundLoopFree',(7,8),setup)
        h.run('SoundLoopFree',(-1,8),setup)
    for flag in [-2147483648,-1,0,1,2,3,2147483647]:
        h.run('SeqSetTickFlag',(flag,),lambda m:m.write32(0x800B4424,0x76543210))
    for maximum in [-32768,-1,0,1,4096,32767,0x10001]:
        for vol in [-65536,-32768,-32767,-4097,-4096,-1,0,1,4096,4097,32767,32768,65535,65536]:
            h.run('SpuVolumeClamp',(maximum,vol))
    for mode in range(8):
        h.run('SfxLookupTone',(0x80 if mode==1 else 2,3 if mode==4 else 1,3 if mode==6 else 1),
              lambda m,mode=mode: bank_setup(m,rng,mode))
    def high_bank(m):
        bank_setup(m,rng)
        m.write32(0x800E1184+12*128,0x801C0400)
        m.write32(0x801C041C,0x100); m.write32(0x801C0420,0x200)
    h.run('SfxLookupTone',(128,1,1),high_bank)
    for free in range(25):
        for pending in [0,1,2,3]:
            def setup(m,free=free,pending=pending):
                bank_setup(m,rng)
                m.write8(0x800F75DE,pending&1); m.write8(0x800F75DF,pending>>1)
                for i in range(24):
                    m.write_bytes(0x800F5AC0+288*i,rng.randbytes(288))
                    m.write32(0x800F5AE4+288*i,0xFF000000 if i==free else rng.choice([1,0x10000,0x10001]))
            h.run('SfxVoiceAlloc',(2,1,1),setup)
            h.run('SfxVoiceStart',(0x102,0x101,0x101),setup)
            h.run('SfxVoiceStartEx',(0x82 if pending&2 else 2,0x101,0x101,
                  rng.choice([-32768,-4097,-1,0,4096,4097,32767,0x10001]),
                  rng.choice([-32768,-4097,-1,0,4096,4097,32767,0x10001]),rng.getrandbits(32)),setup)
    h.run('SfxVoiceAlloc',(2,1,1),lambda m:bank_setup(m,rng,2))
    h.run('SfxVoiceStart',(2,1,1),lambda m:bank_setup(m,rng),idle=1)
    h.run('SfxVoiceStartEx',(0x82,1,1,4096,4096,4096),lambda m:bank_setup(m,rng),idle=1)
    for handle in [-1,0,1,23,24,32767,32768,32769,32791,32792,65536,65560]:
        for active in [False,True]:
            h.run('SfxVoiceStop',(handle,),lambda m,handle=handle,active=active:stop_setup(m,rng,handle,active))
    h.run('SfxVoiceStop',(0,),lambda m:stop_setup(m,rng,0,True),idle=1)
    for distance in [0,1,2048,2049,4096,8191,8192,8193,32767]:
        for range_ in [-1,0,2048,4096,8192,32767]:
            for camera in [False,True]:
                h.run('SoundEmitterCalcPan',(0x801E0000,range_,0x801E0010),
                      lambda m,d=distance,c=camera:pan_setup(m,(d,0,0),c,(0,0),(0,100)))
    for i in range(max(1,round(250*scale))):
        pos=tuple(rng.randint(-15000,15000) for _ in range(3))
        cam=tuple(rng.randint(-5000,5000) for _ in range(2))
        facing=tuple(rng.randint(-5000,5000) for _ in range(2))
        out=rng.choice([0x801E0010,0x801E0000,0x800BF688])
        h.run('SoundEmitterCalcPan',(0x801E0000,32767,out),
              lambda m,pos=pos,cam=cam,facing=facing:pan_setup(m,pos,True,cam,facing))
    for pos in [(-32768,0,-32768),(32767,32767,32767),(-32768,-32768,-32768),(0,0,0)]:
        for range_ in [-32768,0,2048,8192,32767,0x17FFF]:
            def setup(m,pos=pos):
                pan_setup(m,pos,False)
                for i in range(3): m.write16(0x800BF688+2*i,32767)
            h.run('SoundEmitterCalcPan',(0x801E0000,range_,0x801E0010),setup)
    def overflow_setup(m):
        pan_setup(m,(-32768,0,-32768),True)
        m.write16(0x800BF688,-32768); m.write16(0x800BF68C,-32768)
    h.run('SoundEmitterCalcPan',(0x801E0000,8192,0x801E0010),overflow_setup,expect_trap=True)
    for mode in range(7):
        h.run('PlaySfxAt',(9,0x801E0000),lambda m,mode=mode:request_setup(m,rng,mode),idle=int(mode==6))
    for mode in [0,1,-1]:
        for failure in [0,2,5,6,7]:
            def setup(m,failure=failure):
                request_setup(m,rng,failure)
                for i in range(24):
                    m.write_bytes(0x800BF370+28*i,rng.randbytes(28))
                    m.write16(0x800BF370+28*i,0 if failure==7 else -1)
            h.run('SoundLoopStartEx',(mode,0xA009,0xFEDC),setup,idle=int(failure==6))
    for handle in [-1,0,1,23,24,32768,32791,65536]:
        def setup(m,handle=handle):
            stop_setup(m,rng,handle)
            for i in range(24):
                m.write_bytes(0x800BF370+28*i,rng.randbytes(28))
                m.write16(0x800BF370+28*i,9 if i==23 else 10)
                m.write16(0x800BF372+28*i,handle)
        h.run('SoundLoopStopAndFree',(9,handle),setup)
    h.run('SoundLoopStopAndFree',(9,0),setup,idle=1)
    real_scenarios(h,rng)
    chain_scenarios(h,rng)

def chain_scenarios(h,rng):
    """Keep RAM between requests: retained pending settings, duplicate queue IDs,
    loop pool exhaustion, stop/free and slot reuse. No reset between chain steps.
    """
    if h.target: return
    base=h.base
    def step(name,args=(),setup=lambda m:None):
        h.run(name,args,setup)
        h.base=h.m.snapshot()
    try:
        def initial(m):
            request_setup(m,rng)
            stop_setup(m,rng,0,False)
            for i in range(24):
                m.write32(0x800F5AE4+288*i,0)
                m.write16(0x800E080A+102*i,0)
                m.write16(0x800BF370+28*i,-1)
        step('PlaySfxAt',(9,0x801E0000),initial)
        step('PlaySfxAt',(9,0x801E0000))
        for i in range(23): step('SoundLoopStartEx',(i&1,9,i))
        step('SoundLoopStartEx',(1,9,24)) # SFX pool full; pending settings retained.
        step('SoundLoopStopAndFree',(9,1))
        step('SoundLoopStartEx',(0,9,25)) # consumes the retained settings.
        # Release the one-shot's slot, then fill the loop table's last free entry.
        step('SfxVoiceStop',(0,))
        step('SoundLoopStartEx',(0,9,26))
        step('SoundLoopStartEx',(0,9,27)) # loop table full.
        step('SoundLoopStopAndFree',(9,0))
        step('SoundLoopStartEx',(1,9,28))
    finally: h.base=base

def real_scenarios(h,rng):
    """All 198 original effect records, common and area-1 stage SShd headers.
    Emitter positions are actual monkey nav nodes in the loaded area member.
    """
    common=(ROOT/'extract/members/00B.bin').read_bytes()
    container=(ROOT/'extract/members/01B.bin').read_bytes()
    chunks=dump_sound.parse(container)['shd']
    off,size=next((o,s) for o,s in chunks if struct.unpack_from('<I',container,o+0x20)[0]!=0xFFFFFFFF)
    stage=container[off:off+size]
    area=(ROOT/f'extract/members/{dump_areas.area_members()[1]:03X}.bin').read_bytes()
    mk=dump_monkeys.monkeys(area)
    positions=[mk['pos'](i)[:3] for i in range(mk['nodes'])]
    table=bytes(h.base[0][0xB2D08:0xB2D08+198*8])
    for id in range(198):
        slot,program,note=struct.unpack_from('<3h',table,8*id)
        assert slot in (0,2), (id,slot)
        pos=positions[id%len(positions)]
        def setup(m,id=id,pos=pos):
            m.write_bytes(0x801A0000,common); m.write_bytes(0x801C0000,stage)
            m.write32(0x800E1180,1); m.write32(0x800E1184,0x801A0000)
            m.write32(0x800E1198,1); m.write32(0x800E119C,0x801C0000)
            m.write_bytes(0x800F75C0,bytes(40)); m.write8(0x800F75C3,1)
            m.write16(0x800BF670,0); m.write16(0x800BF674,2)
            for i in range(24):
                m.write_bytes(0x800F5AC0+288*i,bytes(288))
                m.write16(0x800BF370+28*i,-1)
            for i in range(12): m.write16(0x800BF610+8*i,-1)
            pan_setup(m,pos,True,(pos[0]-100,pos[2]-100),(pos[0],pos[2]-100))
            for i in range(3): m.write16(0x800BF688+2*i,pos[i])
        h.run('SfxLookupTone',(slot,program,note),setup)
        h.run('SfxVoiceAlloc',(slot,program,note),setup)
        h.run('PlaySfxAt',(id,0x801E0000),setup)
        h.run('SoundLoopStartEx',(id&1,id,0x1234),setup)

def request_setup(m,rng,mode=0):
    bank_setup(m,rng)
    pan_setup(m,(9000,0,0) if mode==1 else (100,0,100))
    m.write16(0x800BF670, -1 if mode==2 else 2)
    m.write16(0x800B2D08+8*9,0); m.write16(0x800B2D0A+8*9,1); m.write16(0x800B2D0C+8*9,1)
    for i in range(12):
        m.write_bytes(0x800BF610+8*i,rng.randbytes(8))
        m.write16(0x800BF610+8*i,1 if mode in (3,4) else -1)
        m.write16(0x800BF612+8*i,9 if mode==3 else 10)
    for i in range(24):
        m.write_bytes(0x800F5AC0+288*i,rng.randbytes(288))
        m.write32(0x800F5AE4+288*i,1 if mode==5 or i else 0)

def pan_setup(m,pos=(100,0,100),enabled=True,camera=(0,0),facing=(100,0)):
    for i in range(3):
        m.write16(0x800BF688+2*i,0); m.write16(0x801E0000+2*i,pos[i])
    m.write16(0x800F448A,int(enabled))
    m.write16(0x800F45A8,camera[0]); m.write16(0x800F45AC,camera[1])
    m.write16(0x800F45B0,facing[0]); m.write16(0x800F45B4,facing[1])

def stop_setup(m,rng,handle,active=True):
    m.write32(0x800E1874,0x1F801C00)
    for i in range(24):
        slot=0x800F5AC0+288*i
        m.write_bytes(slot,rng.randbytes(288)); m.write8(slot+0x26,int(active))
        channel=0x800E07F0+102*i
        m.write_bytes(channel,rng.randbytes(102))
        m.write16(channel+0x1A,0 if i%3==0 else 1)
        m.write16(channel+6,(handle&0x7FFF) if i%3==1 else 0x7FFF)

def bank_setup(m,rng,mode=0):
    header=0x801C0000; table=header+0x100
    for slot in range(256):
        m.write32(0x800E1180+12*slot,1); m.write32(0x800E1184+12*slot,header)
    m.write_bytes(header,rng.randbytes(1024))
    m.write_bytes(0x800F75C0,rng.randbytes(40))
    m.write8(0x800F75C3,0 if mode==2 else 1)
    m.write32(header+0x1C,0x100); m.write32(header+0x20,0xFFFFFFFF if mode==0x7 else 0x200)
    m.write32(header+0x14,0x300)
    m.write16(table,2); m.write16(table+4,0xFFFF if mode==5 else 0x21)
    m.write16(table+0x20,2); m.write16(table+0x24,0x80)
    if mode==3: m.write32(0x800E1198,0)

def mutation_sources(original):
    # Hand-selected behavioural mutations, never generated port code.
    specs=[
        ('SfxIsQueued','queued type sign','ae_rds16(r,entry+AE_SQ_TYPE)>=0','ae_rds16(r,entry+AE_SQ_TYPE)>0'),
        ('SfxIsQueued','queued id truncation','id = s16(id);','id = id;'),
        ('SfxIsQueued','last queue entry','i<12; ++i','i<11; ++i'),
        ('SoundDelayedQueueAdd','free type equality','ae_rds16(r,entry+AE_SQ_TYPE)<0','ae_rds16(r,entry+AE_SQ_TYPE)<=0'),
        ('SoundDelayedQueueAdd','reserve wrong id','ae_wr16(r,entry+AE_SQ_ID,id);','ae_wr16(r,entry+AE_SQ_ID,id+1);'),
        ('SoundDelayedQueueAdd','positional type truncation','s16(type)==1','type==1'),
        ('SoundDelayedQueueAdd','swap volume inputs','entry+AE_SQ_RIGHT,ae_rd16(r,pan+2)','entry+AE_SQ_RIGHT,ae_rd16(r,pan)'),
        ('SoundDelayedQueueAdd','overwrite nonpositional volume','if (s16(type)==1)','if (1)'),
        ('SoundLoopAllocSlot','last loop entry','i<24; ++i','i<23; ++i'),
        ('SoundLoopAllocSlot','wrong reserved index','entry+AE_SL_INDEX,i','entry+AE_SL_INDEX,i+1'),
        ('SoundLoopFree','wrong active reset','entry+AE_SL_ACTIVE,0','entry+AE_SL_ACTIVE,1'),
        ('SoundLoopFree','wrong index reset','entry+AE_SL_INDEX,0xFFFF','entry+AE_SL_INDEX,0'),
        ('SoundLoopFree','skip id match',' && ae_rds16(r,entry+AE_SL_ID)==id',''),
        ('SeqSetTickFlag','query boundary','flag<2','flag<=2'),
        ('SpuVolumeClamp','wrong phase','result |= 0x8000;','result |= 0x4000;'),
        ('SpuVolumeClamp','wrong maximum','result=(uint32_t)maximum;','result=(uint32_t)maximum+1;'),
        ('SfxLookupTone','bank byte mask','AE_SB_SIZE*(bank&0xFF)','AE_SB_SIZE*(bank&0x7F)'),
        ('SfxLookupTone','wrong header global','AE_SOUND_BANK_HEADER,header','AE_SOUND_BANK_HEADER,header+4'),
        ('SfxLookupTone','program upper equality','ae_rd16(r,table)<program','ae_rd16(r,table)<=program'),
        ('SfxLookupTone','note upper equality','<(note&0xFF)','<=(note&0xFF)'),
        ('SfxLookupTone','offset low bit','offset&0xFFFE','offset&0xFFFF'),
        ('SfxVoiceAlloc','all flag bytes occupied','&0x00FF00FFu','&0xFFFFFFFFu'),
        ('SfxVoiceAlloc','wrong active flag','voice+AE_SV_ACTIVE,1','voice+AE_SV_ACTIVE,2'),
        ('SfxVoiceAlloc','wrong event pointer','voice+AE_SV_EVENTS,events','voice+AE_SV_EVENTS,events+2'),
        ('SfxVoiceAlloc','wrong program','voice+AE_SV_PROGRAM,program','voice+AE_SV_PROGRAM,note'),
        ('SfxVoiceAlloc','pending flag retained','AE_SS_PENDING_FLAG,0','AE_SS_PENDING_FLAG,1'),
        ('SfxVoiceAlloc','pending pan retained','AE_SS_PENDING_PAN,0','AE_SS_PENDING_PAN,1'),
        ('SfxVoiceAlloc','scale not consumed','AE_SS_SCALE,0','AE_SS_SCALE,1'),
        ('SfxVoiceStart','ignore timeout','if (wait_idle(r,hw)) return -1;','if (wait_idle(r,hw) && 0) return -1;'),
        ('SfxVoiceStartEx','ignore bank flag','if (bank&0x80)','if (0)'),
        ('SfxVoiceStartEx','wrong volume maximum','ae_SpuVolumeClamp(r,4096,s16(left))','ae_SpuVolumeClamp(r,4095,s16(left))'),
        ('SfxVoiceStartEx','swap volumes','AE_SS_RIGHT,ae_SpuVolumeClamp(r,4096,s16(right))','AE_SS_RIGHT,ae_SpuVolumeClamp(r,4096,s16(left)+(right&0))'),
        ('SfxVoiceStop','wrong release state','channel+AE_SC_RELEASE,1','channel+AE_SC_RELEASE,2'),
        ('SfxVoiceStop','wrong key-off bank','i<16?0:1','i<16?1:0'),
        ('SfxVoiceStop','wrong key-off mask','1u<<(i&15)','2u<<(i&15)'),
        ('SfxVoiceStop','wrong wait delay','hw->user,r,2);','hw->user,r,1);'),
        ('SfxVoiceStop','high handle ADSR','hw->user,r,i,0,0','hw->user,r,i,1,0'),
        ('SoundEmitterCalcPan','near distance threshold','distance>2048','distance>=2048'),
        ('SoundEmitterCalcPan','wrong distance divisor','range-2048','range-2047'),
        ('SoundEmitterCalcPan','flip pan direction','if (cross>0)','if (cross<0)'),
        ('SoundEmitterCalcPan','wrong audibility','right>=8 || left>=8','right>4096 || left>4096'),
        ('SoundEmitterCalcPan','swap result pair','pan[0]=(int16_t)left; pan[1]=(int16_t)right;','pan[0]=(int16_t)right; pan[1]=(int16_t)left;'),
        ('PlaySfxAt','wrong queue type','queue_reserve(r,1,s16(id))','queue_reserve(r,0,s16(id))'),
        ('PlaySfxAt','wrong scale','pan[0],pan[1],4096','pan[0],pan[1],4095'),
        ('PlaySfxAt','wrong remembered handle','effect+AE_SE_LAST_VOICE,voice','effect+AE_SE_LAST_VOICE,voice+1'),
        ('PlaySfxAt','wrong printf ID','0x800A622Cu,3,id,','0x800A622Cu,3,id+1,'),
        ('SoundLoopStartEx','wrong mode branch','if (mode==0)','if (mode!=0)'),
        ('SoundLoopStartEx','lost id flags','loop+AE_SL_ID,id','loop+AE_SL_ID,(uint32_t)id&0xFFF'),
        ('SoundLoopStartEx','wrong extra','loop+AE_SL_EXTRA,extra','loop+AE_SL_EXTRA,extra+1'),
        ('SoundLoopStopAndFree','halfword precheck','if (voice>=0)','if (s16(voice)>=0)'),
        ('SoundLoopStopAndFree','wrong free id','ae_SoundLoopFree(r,s16(id),s16(voice))','ae_SoundLoopFree(r,s16(id+1),s16(voice))'),
    ]
    for function,label,before,after in specs:
        start=original.index('ae_'+function+'(')
        # Include the relevant static helper for mutations of queue and pan logic.
        if function=='SoundDelayedQueueAdd': start=original.index('static uint32_t queue_reserve')
        if function=='SoundEmitterCalcPan': start=original.index('static int32_t emitter_pan')
        end=original.index('\n}',original.index('ae_'+function+'(',start))+2
        body=original[start:end]
        assert before in body, (function,label,before)
        yield function,label,original[:start]+body.replace(before,after)+original[end:]

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--coverage',action='store_true')
    ap.add_argument('--cases',type=float,default=1); ap.add_argument('--seed',type=int,default=94423)
    ap.add_argument('--trapv',action='store_true'); ap.add_argument('--mutate',action='store_true')
    a=ap.parse_args(); start=time.time()
    if a.mutate:
        original=(ROOT/'src/sound/ae_sound.c').read_text()
        mutations=list(mutation_sources(original)); survivors=[]
        out=ROOT/'extract/mips'; out.mkdir(parents=True,exist_ok=True)
        for i,(function,label,source) in enumerate(mutations):
            path=out/f'sound_mutation_{i}.c'; path.write_text(source)
            h=Harness(path,a.trapv); h.target=function
            try: scenarios(h,a.cases,a.seed)
            except (AssertionError,mr.MipsError) as e: print(f'caught {function}: {label}: {str(e)[:100]}',flush=True)
            else: survivors.append((function,label)); print(f'SURVIVED {function}: {label}',flush=True)
        print(f'Mutations: {len(mutations)}; caught: {len(mutations)-len(survivors)}; survivors: {len(survivors)}')
        return int(bool(survivors))
    h=Harness(trapv=a.trapv); scenarios(h,a.cases,a.seed)
    summary=reg.coverage_summary([(n,v[0],v[1],True) for n,v in FUNCS.items() if n in h.available]
                                +[(n,a,s,False) for n,(a,s) in EXTRA_COVERAGE.items()],h.m.coverage)
    for n,k in h.counts.items(): print(f'{n:28} {k}')
    if a.coverage: reg.print_coverage(summary)
    gaps=any(s['uncovered_count'] or s['stale_declared'] for s in summary.values())
    if a.cases==1 and a.seed==94423 and not a.trapv:
        reg.write_results(h.counts,summary,not gaps,extra={'missing_hook_cases':h.null_cases})
    if gaps: raise AssertionError('Undeclared instruction coverage gaps')
    print(f'PASS {sum(h.counts.values())} cases + {h.null_cases} missing-hook checks in {time.time()-start:.2f}s')
    return 0

if __name__=='__main__': sys.exit(main())
