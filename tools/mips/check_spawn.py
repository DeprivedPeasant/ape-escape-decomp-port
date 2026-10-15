"""Instruction differential check of ae_spawn on real EXE + area 1 RAM.
Run with .venv/Scripts/python.exe tools/mips/check_spawn.py [--coverage] [--mutate].
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import time
import struct

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/mips'),str(ROOT/'tools')]
import mips_ram as mr
import monkey_harness as mh
import check_world as cw
import exclusion_registry as reg
import dump_areas

# address, byte size, return kind, takes spawn hooks, argument kinds
FUNCS={
    'ProjectileSpawn':(0x80059304,1872,'s',True,'uusu'),
    'ParticleAlloc':(0x80069548,228,'u',False,''),
    'SpawnOneParticle':(0x8006962C,192,'v',False,'suu'),
    'TrailSpawn':(0x8006ED78,196,'v',False,'usss'),
    'EffectObjAlloc':(0x8006FE7C,108,'u',False,''),
    'EffectObjSpawn':(0x80070130,340,'v',True,'uusssu'),
    'EffectSpawnFromDef':(0x80070284,44,'v',True,'uussu'),
    'SpawnEffectGroup':(0x800696EC,460,'v',True,'suuu'),
    'SpawnParticle':(0x800698B8,44,'v',True,'su'),
    'ProjectileAllocSlot':(0x80084AC8,128,'u',False,''),
    'ProjectileInit':(0x80084B48,44,'v',False,'uuuu'),
    'ProjectileSetColor':(0x80084B74,68,'v',False,'uu'),
    'ProjectileSetByte2':(0x80084BB8,8,'v',False,'uu'),
    'ProjectileSetUpdate':(0x80084C78,28,'v',False,'uuuu'),
    'SparkAdd':(0x8006E054,136,'v',False,'uu'),
    'SparkAddRandom':(0x8006E0DC,228,'v',False,'uus'),
    'CameraShake':(0x80048A00,444,'v',False,'s'),
    'SpawnPuff':(0x8006B3CC,36,'v',True,'u'),
    'EffectBigBurst':(0x8006B3F0,384,'v',True,'sss'),
}
HOOKS=[('PlaySfx',0x800877F8,1),('MatrixLookDir',0x8001484C,2),('MatrixFromDirXZ',0x80014804,2)]
EXTRA_COVERAGE={'VectorNormalSS':(0x800301E0,48),'VectorNormal core':(0x80030210,192),
                'ApplyMatrixSV':(0x800300B0,80),'GetGroundHeight':(0x80043400,1272)}
PaddingFn=C.CFUNCTYPE(C.c_uint16,C.c_void_p,C.c_void_p,C.c_uint32)
GTE=None  # reuse the loaded oracle DLL across mutation builds on Windows
class Hooks(C.Structure):
    _fields_=[('user',C.c_void_p),('world',C.c_void_p)]+[(n,cw.HookFn) for n,_,_ in HOOKS]+[('position_padding',PaddingFn)]

def build(source=None,trapv=False):
    out=ROOT/'extract/mips'; out.mkdir(parents=True,exist_ok=True)
    dll=out/f'spawn_{os.getpid()}_{time.time_ns()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O1' if trapv else '-O2',
                    *(['-ftrapv'] if trapv else []),'-Wall','-Wextra','-Werror','-shared','-static-libgcc',
                    '-I'+str(ROOT/'src/effects'),str(ROOT/'src/core/ae_ram.c'),
                    str(ROOT/'src/core/ae_sdk.c'),str(ROOT/'src/world/ae_world.c'),
                    str(source or ROOT/'src/effects/ae_spawn.c'),'-o',str(dll)],check=True)
    return dll

class Side(cw.Side):
    def __init__(self,dll):
        # Same RAM FFI, but a distinct module API and hook table.
        saved=cw.FUNCS; cw.FUNCS=FUNCS
        try: super().__init__(dll)
        finally: cw.FUNCS=saved
        self.world_hooks=self.hooks
        self.hooks=Hooks(); self.hooks.world=C.addressof(self.world_hooks)
        self.null_hooks=Hooks(); self.keep_spawn={}
        for name,_,_ in HOOKS:
            cb=cw.HookFn(lambda u,r,a,b,c,d,n=name:self.model.hook(n,a,b,c,d))
            self.keep_spawn[name]=cb; setattr(self.hooks,name,cb)
        cb=PaddingFn(lambda u,r,f:self.padding[f])
        self.keep_spawn['position_padding']=cb; self.hooks.position_padding=cb
    def call(self,name,args,hooks=None):
        return getattr(self.lib,'ae_'+name)(self.ram,*([C.byref(hooks or self.hooks)] if FUNCS[name][3] else []),*args)

class Model:
    def __init__(self,mem,ctx): self.mem=mem; self.ctx=ctx; self.log=[]
    def hook(self,name,a,b,c,d):
        if name in ('MatrixLookDir','MatrixFromDirXZ'):
            direction=tuple(self.mem.rd(a+2*k,2) for k in range(3))
            self.log.append((name,'local' if a>=0x801FF000 else a,direction,'local' if b>=0x801FF000 else b))
            # Nontrivial rotation + translation; unused matrix padding randomized.
            for k,value in enumerate(self.ctx['matrix']): self.mem.wr(b+2*k,value,2)
            return mr.signed(self.ctx['result'])
        if name=='GetSolidHeight':
            self.log.append((name,a,b,c)); self.mem.wr(0x1F8003F0,0x3412,2)
            return self.ctx['solid']
        self.log.append((name,a))
        self.mem.wr(0x801EFFF0,(self.mem.rd(0x801EFFF0,4)+a)&0xFFFFFFFF,4)
        return mr.signed(self.ctx['result'])

class Harness(mh.MonkeyHarness):
    def __init__(self,source=None,trapv=False):
        global GTE
        self.funcs=FUNCS
        self.m=mr.MipsRam.from_extract(ROOT/f'extract/members/{dump_areas.area_members()[1]:03X}.bin')
        self.m.coverage=set(); self.base=self.m.snapshot()
        if GTE is None:
            self.m.attach_gte_oracle(); GTE=(self.m.gte_state,self.m.gte_backend)
        else: self.m.gte_state,self.m.gte_backend=GTE
        self.c=Side(build(source,trapv)); self.lib=self.c.lib
        self.stack_lo=0x1FF000; self._mask=[]; self.counts={}; self.failures=0; self.stop_after=1
        reg.declare('stack_window','801FF000..80200000')
        reg.declare('shared_random','spawn position padding')
        reg.declare('ram_mask','matrix hook local addresses normalized')
    def run_case(self,name,args,setup,ctx):
        self.m.restore(self.base); setup(self.m)
        self.m.gte_reset()
        # Seed only genuinely unwritten local padding, at each nesting depth.
        # Positions are eight bytes although only the first six are initialized.
        depths={'SpawnEffectGroup':[0], 'SpawnParticle':[24], 'SpawnPuff':[48],
                'EffectBigBurst':[56+24], 'ProjectileSpawn':[104+24]}
        for depth in depths.get(name,[]): self.m.write16(mr.DEFAULT_SP-depth-64+30,ctx['padding'])
        if name=='EffectBigBurst': self.m.write16(mr.DEFAULT_SP-56+30,ctx['burst_padding'])
        initial=self.m.snapshot(); self.sync_to_c()
        self.c.padding={0x800696EC:ctx['padding'],0x8006B3F0:ctx['burst_padding']}
        mmem=type('MMem',(),{'rd':lambda _,a,n:self.m.read(a,n),'wr':lambda _,a,v,n:self.m.write(a,n,v)})()
        mm=Model(mmem,ctx); cm=Model(cw.Mem(self.c),ctx); self.c.model=cm
        checkpoints={}
        def original_hook(n,regs):
            checkpoints.setdefault(n,(bytes(self.m.ram[:self.stack_lo]),bytes(self.m.scratch),list(mm.log)))
            return mm.hook(n,*regs[4:8])
        hooks={a:(lambda cpu,regs,n=n:original_hook(n,regs)) for n,a,_ in HOOKS}
        hooks[0x8004D8C0]=lambda cpu,regs:original_hook('GetSolidHeight',regs)
        def memset(cpu,regs):
            for k in range(regs[6]): cpu.write8(regs[4]+k,regs[5]&255)
            return regs[4]
        hooks[0x800A3740]=memset
        regs=self.m.call(FUNCS[name][0],args,hooks=hooks)
        cret=self.c.call(name,args)
        self.counts[name]=self.counts.get(name,0)+1
        self.compare(name,str(args),regs[2],cret or 0,mm.log,cm.log,FUNCS[name][2])
        # Required callbacks fail closed at the first missing dependency. Compare
        # earlier game writes and log entries, not just the presence of an error.
        if FUNCS[name][3] and not getattr(self,'only',None):
            for missing in {e[0] for e in mm.log if e[0] in dict((n,a) for n,a,_ in HOOKS)}:
                self.m.restore(initial); self.sync_to_c(); self.c.model=Model(cw.Mem(self.c),ctx)
                setattr(self.c.hooks,missing,cw.HookFn())
                self.c.call(name,args)
                setattr(self.c.hooks,missing,self.c.keep_spawn[missing])
                if not self.lib.ae_ram_failed(self.c.ram): self.fail(name,'missing '+missing,'did not fail closed')
                expected, scratch, log=checkpoints[missing]
                if C.string_at(self.c.main,self.stack_lo)!=expected or bytes(self.c.scr)!=scratch or self.c.model.log!=log:
                    self.fail(name,'missing '+missing,'writes or callbacks continued past missing dependency')
            if 'GetSolidHeight' in checkpoints:
                self.m.restore(initial); self.sync_to_c(); self.c.model=Model(cw.Mem(self.c),ctx)
                self.c.world_hooks.GetSolidHeight=cw.HookFn()
                self.c.call(name,args)
                self.c.world_hooks.GetSolidHeight=self.c.keep['GetSolidHeight']
                expected,scratch,log=checkpoints['GetSolidHeight']
                if (not self.lib.ae_ram_failed(self.c.ram) or C.string_at(self.c.main,self.stack_lo)!=expected
                        or bytes(self.c.scr)!=scratch or self.c.model.log!=log):
                    self.fail(name,'missing GetSolidHeight','did not stop at world dependency')
            # If this call needs padding, omit that callback. No public game
            # storage is changed before the requirement is checked.
            if name in ('SpawnEffectGroup','SpawnParticle','SpawnPuff','EffectBigBurst'):
                self.m.restore(initial); self.sync_to_c(); self.c.model=Model(cw.Mem(self.c),ctx)
                needs_padding=name=='EffectBigBurst'
                if not needs_padding:
                    typ=275 if name=='SpawnPuff' else args[0]
                    table=args[3] if name=='SpawnEffectGroup' else 0x800B1828
                    if typ>=256:
                        stream=self.m.read32(table+4*(typ-(256 if typ<456 else 456)))
                        needs_padding=bool(self.m.read16(stream))
                self.c.hooks.position_padding=PaddingFn()
                self.c.call(name,args)
                self.c.hooks.position_padding=self.c.keep_spawn['position_padding']
                # Direct single particles / empty groups do not need this hook.
                if needs_padding and not self.lib.ae_ram_failed(self.c.ram):
                    self.fail(name,'missing padding','did not fail closed')
                if self.lib.ae_ram_failed(self.c.ram):
                    if C.string_at(self.c.main,self.stack_lo)!=initial[0][:self.stack_lo] or self.c.model.log:
                        self.fail(name,'missing padding','game writes preceded missing padding')

POS=0x801D0000; COLOR=POS+16; DEF=POS+32; PTAB=POS+0x100; GTAB=POS+0x300; STREAM=POS+0x400
def scenarios(h,scale=1,seed=94423):
    rng=random.Random(seed)
    blob=(ROOT/f'extract/members/{dump_areas.area_members()[1]:03X}.bin').read_bytes()
    ah=struct.unpack_from('<I',blob,0x4D0)[0]; grid=ah+4*struct.unpack_from('<I',blob,ah+4)[0]
    w,d,nc=struct.unpack_from('<HHI',blob,grid); cells=grid+8; heights=cells+w*d*12+nc*4
    def run(name,args,setup=lambda m:None):
        if getattr(h,'only',None) and name!=h.only: return
        padding=rng.getrandbits(16)
        ctx=dict(padding=padding,burst_padding=rng.getrandbits(16),result=rng.getrandbits(32),solid=32767,
                 matrix=[4096,0,0,0,4096,0,0,0,4096]+[rng.getrandbits(16) for _ in range(7)])
        def wrapped(m):
            m.write_bytes(POS,rng.randbytes(0x600))
            m.write_bytes(0x800B9900,rng.randbytes(0x1C00))
            for k in range(100): m.write8(0x800B9900+52*k,0)
            m.write8(0x800BAD50,rng.randrange(100)); m.write8(0x800B1BD0,rng.randrange(16))
            m.write8(0x800B1B74,rng.randrange(8)); m.write8(0x800B1B41,rng.randrange(8))
            m.write16(0x800AE8F0,rng.getrandbits(16))
            m.write_bytes(0x800F5380,rng.randbytes(32*52))
            for k in range(32): m.write8(0x800F5380+52*k,0)
            for k in range(60): m.write32(PTAB+4*k,0x800B0460+2*k)
            m.write32(0x800F44E4,mr.AREA_BASE+ah); m.write16(0x800EBFA0,w); m.write16(0x800EBFA2,d)
            m.write32(0x800EBFA8,mr.AREA_BASE+cells); m.write32(0x800EBFB4,mr.AREA_BASE+heights)
            m.write_bytes(0x800B8D30,rng.randbytes(16*48))
            for k in range(16): m.write8(0x800B8D30+48*k+12,0)
            for k in range(3): m.write32(0x800EC204+4*k,rng.randint(-10000,10000)); m.write32(0x800EC210+4*k,rng.getrandbits(32))
            setup(m)
        h.run_case(name,args,wrapped,ctx)
    for i in range(max(1,round(32*scale))):
        for name,args in [('ParticleAlloc',()),('EffectObjAlloc',()),('ProjectileAllocSlot',()),
                          ('ProjectileInit',(0x800F5380,0x12345678,POS,i)),
                          ('ProjectileSetByte2',(0x800F5380,rng.getrandbits(32))),
                          ('ProjectileSetColor',(0x800F5380,i%16)),
                          ('ProjectileSetUpdate',(0x800F5380,*[rng.getrandbits(32) for _ in range(3)])),
                          ('SparkAdd',(POS,COLOR)),('SparkAddRandom',(POS,COLOR,i%4)),
                          ('SpawnOneParticle',(i,POS+(i%4),PTAB)),
                          ('SpawnParticle',(i,POS)),('SpawnPuff',(POS,)),
                          ('EffectBigBurst',(rng.randint(-32768,32767),rng.randint(-32768,32767),rng.randint(-32768,32767)))]:
            run(name,args)
    for cursor in [0,98,99,100,254,255]:
        for priority in [0,1]:
            def setup(m,cursor=cursor,priority=priority):
                m.write8(0x800BAD50,cursor)
                for k in range(100):
                    m.write8(0x800B9900+52*k,1); m.write8(0x800B9900+52*k+2,priority)
            run('ParticleAlloc',(),setup)
    for free in range(33):
        def setup(m,free=free):
            for k in range(32): m.write8(0x800F5380+52*k,0 if k==free else 3)
        run('ProjectileAllocSlot',(),setup)
    for type in [0,199,200,201,255]: run('SpawnOneParticle',(type,POS,PTAB))
    for width in [-2147483648,-32768,-1,0,1,3,4,32767,2147483647]: run('TrailSpawn',(POS,0xABCD,width,-1))
    for count in [-1,0,1,2,16,17]:
        for direction in [0,POS]:
            run('EffectObjSpawn',(POS,DEF,count,-8,257,direction))
            run('EffectSpawnFromDef',(POS,DEF,count,-1,direction))
    for type in [2,255,256,455,456,457]:
        for stream in [[],[7,0,0,0],[7,-32768,2,8,50,32767,0,2,3,51,-1,5,-32768,9],
                       [7,0,-3,0,51,0,0,12,1]]:
            def setup(m,type=type,stream=stream):
                if type>=256:
                    m.write32(GTAB+4*(type-(256 if type<456 else 456)),STREAM)
                    for k,v in enumerate(stream+[0]): m.write16(STREAM+2*k,v)
            run('SpawnEffectGroup',(type,POS,PTAB,GTAB),setup)
    for level in [-2147483648,-1,0,1,2,3,255,256,2147483647]:
        for active in [0,1]:
            for old in [0,1,2,255]:
                def setup(m,active=active,old=old): m.write8(0x800E6958,active); m.write8(0x800E6959,old)
                run('CameraShake',(level,),setup)
    for kind in [-1,0,1,2,3,4,5,6,7,8,9,10,11,256]:
        for variant in [0,1,2,3,4,5,255,256,257]:
            def setup(m,variant=variant):
                for k in range(3): m.write16(POS+2*k,rng.randint(-min(w,d)*200,min(w,d)*200))
            velocity=variant if kind in (6,10) else POS+16
            run('ProjectileSpawn',(POS,0x800E5500,kind,velocity),setup)
    for kind in [1,2,6,10]:
        for free in [0,1,15,16]:
            for secondary_full in [False,True]:
                def setup(m,kind=kind,free=free,secondary_full=secondary_full):
                    for k in range(16): m.write8(0x800B8D30+48*k+12,0 if k==free else kind)
                    for k in range(32): m.write8(0x800F5380+52*k,3 if secondary_full else 0)
                run('ProjectileSpawn',(POS,0x800E5500,kind,0 if kind in (6,10) else COLOR),setup)
    for kind in [6,7,8,9,10]:
        for sign in [-1,0,1]:
            for variant in [0,1]:
                def setup(m,sign=sign):
                    for k in range(3):
                        m.write16(POS+2*k,1000); m.write32(0x800EC204+4*k,1000+sign*2000)
                        m.write32(0x800EC210+4*k,0)
                run('ProjectileSpawn',(POS,0x800E5500,kind,variant if kind in (6,10) else COLOR),setup)
    for dy in [-2147483648,-98304,-3075,-3072,-3069,-1,0,1,3069,3072,3075,98304,2147483647]:
        def setup(m,dy=dy):
            m.write16(POS+2,0); m.write32(0x800EC208,dy)
        run('ProjectileSpawn',(POS,0x800E5500,7,COLOR),setup)
    for count in [0,1,2,3,4,5,15,16]:
        def setup(m,count=count):
            for k in range(16): m.write8(0x800B8D30+48*k+12,6 if k<count else 0)
        run('ProjectileSpawn',(POS,0x800E5500,6,0),setup)
    # Every nonnull default script/group in the EXE, with both index namespaces.
    for index in range(50):
        for type in [index,index+200]: run('SpawnParticle',(type,POS))
    for index in range(26):
        if h.m.read32(0x800B1828+4*index):
            for type in [256+index,456+index]: run('SpawnParticle',(type,POS))

def mutation_sources(original):
    # Deliberate semantic errors, kept separate from the implementation. Each
    # anchor must exist once within its function; source drift is a hard error.
    specs=[
        ('ParticleAlloc','scan length','n<AE_PARTICLE_COUNT','n<AE_PARTICLE_COUNT-1'),
        ('ParticleAlloc','cursor advance','+1)&255','+2)&255'),
        ('ParticleAlloc','replacement priority','AE_P_PRIORITY : AE_P_KIND','AE_P_KIND : AE_P_KIND'),
        ('SpawnOneParticle','type byte','p+AE_P_KIND,(uint32_t)type','p+AE_P_KIND,(uint32_t)type+1'),
        ('SpawnOneParticle','type table boundary','type<200','type<201'),
        ('SpawnOneParticle','loop depth','AE_P_LOOP_DEPTH,255','AE_P_LOOP_DEPTH,0'),
        ('SpawnOneParticle','command','0x2C000000','0x2D000000'),
        ('SpawnOneParticle','velocity low byte','AE_P_VEL_X,0','AE_P_VEL_X,1'),
        ('TrailSpawn','life','AE_T_LIFE,15','AE_T_LIFE,14'),
        ('TrailSpawn','width division','asr(width,2)','asr(width,1)'),
        ('TrailSpawn','negative width','0u-(uint32_t)width','(uint32_t)width'),
        ('TrailSpawn','ring wrap','(cursor+1)&7','(cursor+1)&3'),
        ('EffectObjAlloc','header','AE_E_HEADER,16','AE_E_HEADER,15'),
        ('EffectObjAlloc','ring wrap','(cursor+1)&15','(cursor+1)&7'),
        ('EffectObjSpawn','delay ordering','count--;','count-=2;'),
        ('EffectObjSpawn','spacing','(uint32_t)count*(uint32_t)spacing','(uint32_t)count*((uint32_t)spacing+1)'),
        ('EffectObjSpawn','matrix output offset','e+AE_E_MATRIX+off+4*k','e+AE_E_MATRIX+off+4*k+4'),
        ('EffectSpawnFromDef','fixed spacing','count,delay,6,direction','count,delay,5,direction'),
        ('SpawnEffectGroup','table split','type<456','type<457'),
        ('SpawnEffectGroup','count rounding','),8); stream+=4','),7); stream+=4'),
        ('SpawnEffectGroup','y offset','pos+2)+ae_rd16','pos+2)-ae_rd16'),
        ('SpawnParticle','default tables','AE_PARTICLE_SCRIPTS,AE_EFFECT_GROUPS','AE_PARTICLE_SCRIPTS+4,AE_EFFECT_GROUPS'),
        ('ProjectileAllocSlot','state','AE_R_STATE,3','AE_R_STATE,2'),
        ('ProjectileAllocSlot','byte2','AE_R_BYTE2,128','AE_R_BYTE2,127'),
        ('ProjectileInit','position count','k<3','k<2'),
        ('ProjectileSetColor','flag','|16','|8'),
        ('ProjectileSetByte2','byte2 value','AE_R_BYTE2,value','AE_R_BYTE2,value+1'),
        ('ProjectileSetUpdate','callback address','0x80084C04','0x80084C08'),
        ('SparkAdd','color halfword stride','color+2*k','color+k'),
        ('SparkAddRandom','mode branch','mode==1','mode==0'),
        ('SparkAddRandom','random mask','random&127','random&63'),
        ('CameraShake','priority','level<(int32_t)','level<=(int32_t)'),
        ('CameraShake','y mask','8u<<strength','7u<<strength'),
        ('SpawnPuff','particle kind','0x113,pos','0x112,pos'),
        ('EffectBigBurst','ring count','n<16','n<15'),
        ('EffectBigBurst','y offset','-32','-31'),
        ('ProjectileSpawn','volley limit','count>=4','count>=5'),
        ('ProjectileSpawn','owner','AE_J_OWNER,owner','AE_J_OWNER,owner+4'),
        ('ProjectileSpawn','kind6 variant bound','(variant&255)>=5','(variant&255)>=4'),
        ('ProjectileSpawn','kind7 division','scaled/24','scaled/23'),
        ('ProjectileSpawn','kind8 divisor','kind==8 ? 128 : 32','kind==8 ? 127 : 32'),
        ('ProjectileSpawn','kind5 tpage','AE_J_TPAGE,0x5B','AE_J_TPAGE,0x5A'),
        ('ProjectileSpawn','kind1 byte2','record,0x38','record,0x39'),
        ('ProjectileSpawn','kind10 vertical speed','LOCAL_VELOCITY+2,46','LOCAL_VELOCITY+2,45'),
        ('ProjectileSpawn','kind10 subtype','velocity==1 ? 65 : 33','velocity==1 ? 33 : 65'),
    ]
    for function,label,before,after in specs:
        start=original.index('ae_'+function+'(')
        end=original.find('\n}',start)+2
        if function in ('ProjectileSetByte2','SpawnPuff'): end=original.index('\n',start)
        body=original[start:end]
        assert before in body,(function,label,before)
        yield function,label,original[:start]+body.replace(before,after)+original[end:]

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--coverage',action='store_true')
    ap.add_argument('--cases',type=float,default=1); ap.add_argument('--seed',type=int,default=94423)
    ap.add_argument('--trapv',action='store_true'); ap.add_argument('--mutate',action='store_true')
    a=ap.parse_args(); start=time.time()
    if a.mutate:
        original=(ROOT/'src/effects/ae_spawn.c').read_text(); mutations=list(mutation_sources(original)); survivors=[]
        for i,(function,label,source) in enumerate(mutations):
            path=ROOT/f'extract/mips/spawn_mutation_{i}.c'; path.write_text(source)
            h=Harness(path); h.only=function
            try: scenarios(h,a.cases,a.seed)
            except SystemExit:
                if not h.failures: raise
            if h.failures: print('caught:',function,label)
            else: survivors.append((function,label)); print('SURVIVED:',function,label)
        print(f'Mutations: {len(mutations)}; caught: {len(mutations)-len(survivors)}; survivors: {len(survivors)}; {time.time()-start:.1f}s')
        return int(bool(survivors))
    h=Harness(trapv=a.trapv); scenarios(h,a.cases,a.seed)
    for name,cases in h.counts.items(): print(f'{name:28} {cases}')
    summary=h.finish(EXTRA_COVERAGE,show_coverage=a.coverage)
    gaps=any(s['own'] and (s['uncovered_count'] or s['stale_declared']) for s in summary.values())
    if a.cases==1 and a.seed==94423 and not a.trapv:
        reg.write_results(h.counts,summary,not h.failures and not gaps,function_names=list(FUNCS))
    print(f'{"FAIL: coverage gaps" if gaps else "OK"}: {sum(h.counts.values())} cases; {time.time()-start:.1f}s')
    return int(bool(h.failures or gaps))
if __name__=='__main__': sys.exit(main())
