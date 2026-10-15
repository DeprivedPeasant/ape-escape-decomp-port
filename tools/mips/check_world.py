"""Diff src/world against the real EXE and area 1, including RAM, scratch and ordered hooks.
Usage: .venv/Scripts/python.exe tools/mips/check_world.py [--coverage] [--mutate] [--trapv]
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

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mips'))
sys.path.insert(0, str(ROOT / 'tools'))
import mips_ram as mr
import monkey_harness as mh
import exclusion_registry as reg
import dump_areas

FUNCS = {
    'ClearWords': (0x80032708,36,'v',False,'us'),
    'StageExitIsActive': (0x8005DCE4,52,'s',False,''),
    'TargetListAdd': (0x8007CD88,60,'s',False,'u'),
    'TargetListRemove': (0x8007CDC4,56,'v',False,'u'),
    'ObjSlotGetAnimIndex': (0x80052C4C,84,'s',False,'s'),
    'ObjSlotSetField_CF': (0x80052CA0,76,'v',False,'su'),
    'ObjSlotGetTarget': (0x80052D04,104,'s',False,'uu'),
    'SetDAT_800E77E2': (0x80052CEC,12,'v',False,'u'),
    'GimmickGetInfo': (0x8005B7BC,196,'s',False,'su'),
    'GimmickEvent': (0x8005B880,732,'s',True,'usu'),
    'GetGroundHeightEx': (0x800433B0,80,'s',True,'sssu'),
    'GetGroundHeight': (0x80043400,1272,'s',True,'sss'),
}
EXTRA_COVERAGE = {}
HOOKS = [('GetSolidHeight',0x8004D8C0,3),('PlaySfxAt',0x80087A20,2),
         ('GimmickActivate',0x8008FC30,2),('MeshInitFromAreaTable',0x80015CC8,2),
         ('FUN_8013d358',0x8013D358,3),('CameraShake',0x80048A00,1),('EventCamStart',0x8004D39C,1)]
HookFn = C.CFUNCTYPE(C.c_int32,C.c_void_p,C.c_void_p,*([C.c_uint32]*4))
class Hooks(C.Structure):
    _fields_ = [('user',C.c_void_p)] + [(n,HookFn) for n,_,_ in HOOKS] + [
        ('ground_stale_x',C.c_uint16),('ground_stale_z',C.c_uint16)]

def build(source=None, trapv=False, tag='world'):
    out=ROOT/'extract/mips'
    out.mkdir(parents=True,exist_ok=True)
    dll=out/f'{tag}_{os.getpid()}_{time.time_ns()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O1' if trapv else '-O2',
                    *(['-ftrapv'] if trapv else []),'-Wall','-Wextra','-Werror','-shared','-static-libgcc',
                    '-I'+str(ROOT/'src/world'),str(ROOT/'src/core/ae_ram.c'),
                    str(source or ROOT/'src/world/ae_world.c'),'-o',str(dll)],check=True)
    return dll

class Side:
    def __init__(self, dll):
        self.lib=lib=C.CDLL(str(dll))
        P=C.c_void_p; U=C.c_uint32
        lib.ae_ram_create.restype=P
        lib.ae_ptr.argtypes=[P,U,U]; lib.ae_ptr.restype=P
        lib.ae_ram_clear_error.argtypes=[P]
        lib.ae_ram_failed.argtypes=[P]
        lib.ae_ram_error.argtypes=[P]; lib.ae_ram_error.restype=C.c_char_p
        self.ram=lib.ae_ram_create()
        self.main=(C.c_uint8*mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram,0x80000000,mr.RAM_SIZE))
        self.scr=(C.c_uint8*mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram,mr.SCRATCH_BASE,mr.SCRATCH_SIZE))
        for name,(_,_,ret,hh,args) in FUNCS.items():
            f=getattr(lib,'ae_'+name)
            f.argtypes=[P]+([P] if hh else [])+[C.c_int32 if k=='s' else U for k in args]
            f.restype=None if ret=='v' else C.c_int32
        self.hooks=Hooks(); self.null_hooks=Hooks(); self.keep={}; self.model=None
        for name,_,_ in HOOKS:
            cb=HookFn(lambda u,r,a,b,c,d,n=name: self.model.hook(n,a,b,c,d))
            self.keep[name]=cb; setattr(self.hooks,name,cb)
    def call(self,name,args,hooks=None):
        f=getattr(self.lib,'ae_'+name)
        return f(self.ram,*([C.byref(hooks or self.hooks)] if FUNCS[name][3] else []),*args)

class Mem:
    def __init__(self, side): self.side=side
    def blob(self,a):
        return (self.side.scr,a-mr.SCRATCH_BASE) if mr.SCRATCH_BASE<=a<mr.SCRATCH_BASE+1024 else (self.side.main,a&0x1FFFFF)
    def rd(self,a,n):
        b,o=self.blob(a); return int.from_bytes(bytes(b[o:o+n]),'little')
    def wr(self,a,v,n):
        b,o=self.blob(a); b[o:o+n]=(v&((1<<(8*n))-1)).to_bytes(n,'little')

class Model:
    def __init__(self,mem,ctx): self.mem=mem; self.ctx=ctx; self.log=[]
    def hook(self,name,a,b,c,d):
        nargs=next(n for nm,_,n in HOOKS if nm==name)
        entry=[name,*([a,b,c,d][:nargs])]
        if name=='PlaySfxAt': entry.append(tuple(self.mem.rd(b+2*k,2) for k in range(3)))
        self.log.append(tuple(entry))
        # Deterministic side effects exercised identically, with call-time data dependencies.
        if name=='GetSolidHeight':
            self.mem.wr(0x1F8003F0,0x1234,2)
            self.mem.wr(0x1F8003F8,self.ctx['effect'],2)
            self.mem.wr(0x1F8003FC,0x4321,2); self.mem.wr(0x1F8003FE,0x8765,2)
            return self.ctx['solid']
        if name=='GimmickActivate': self.mem.wr(b+10,self.ctx['effect'],2)
        if name=='MeshInitFromAreaTable': self.mem.wr(a,b,4)
        self.mem.wr(0x800B1000,(self.mem.rd(0x800B1000,4)+sum([a,b,c,d][:nargs]))&0xFFFFFFFF,4)
        return mr.signed(self.ctx['effect'])

class WorldHarness(mh.MonkeyHarness):
    def __init__(self,source=None,trapv=False,tag='world'):
        self.funcs=FUNCS
        member=dump_areas.area_members()[1]
        self.m=mr.MipsRam.from_extract(ROOT/f'extract/members/{member:03X}.bin')
        self.m.coverage=set(); self.base=self.m.snapshot()
        self.c=Side(build(source,trapv,tag)); self.lib=self.c.lib
        self.stack_lo=0x1FF000; self._mask=[]; self.counts={}; self.failures=0; self.stop_after=1
        reg.declare('stack_window','801FF000..80200000')
        reg.declare('shared_random','GetGroundHeight stale slope halfwords')
    def run(self,name,args,setup,ctx):
        self.m.restore(self.base); setup(self.m)
        initial=self.m.snapshot()
        self.sync_to_c()
        self.c.hooks.ground_stale_x=ctx['stale'][0]; self.c.hooks.ground_stale_z=ctx['stale'][1]
        mmem=type('MMem',(),{'rd':lambda _,a,n:self.m.read(a,n),'wr':lambda _,a,v,n:self.m.write(a,n,v)})()
        mm=Model(mmem,ctx); cm=Model(Mem(self.c),ctx); self.c.model=cm
        checkpoints={}
        def original_hook(name, registers):
            if name not in checkpoints:
                checkpoints[name]=(bytes(self.m.ram[:self.stack_lo]), bytes(self.m.scratch), list(mm.log))
            return mm.hook(name,*registers[4:8])
        hooks={a:(lambda cpu,r,n=n: original_hook(n,r)) for n,a,_ in HOOKS}
        regs=self.m.call(FUNCS[name][0],args,hooks=hooks)
        cret=self.c.call(name,args)
        self.counts[name]=self.counts.get(name,0)+1
        self.compare(name,str(args),regs[2],cret or 0,mm.log,cm.log,FUNCS[name][2])
        # Test every missing hook separately, preserving earlier hook side effects.
        if mm.log and FUNCS[name][3]:
            for missing in sorted(set(e[0] for e in mm.log)):
                self.m.restore(initial); self.sync_to_c(); self.c.model=Model(Mem(self.c),ctx)
                setattr(self.c.hooks,missing,HookFn())
                self.c.call(name,args)
                setattr(self.c.hooks,missing,self.c.keep[missing])
                if not self.lib.ae_ram_failed(self.c.ram) or b'not provided' not in self.lib.ae_ram_error(self.c.ram):
                    self.fail(name,'NULL '+missing,'missing hook did not fail closed')
                expected_ram,expected_scr,expected_log=checkpoints[missing]
                if name=='GetGroundHeightEx':
                    expected_ram=bytearray(expected_ram)
                    out=args[3]&0x1FFFFF
                    expected_ram[out:out+4]=expected_scr[0x3FC:0x400]
                if (C.string_at(self.c.main,self.stack_lo)!=expected_ram or bytes(self.c.scr)!=expected_scr
                        or self.c.model.log!=expected_log):
                    self.fail(name,'NULL '+missing,'continued RAM writes or hooks after missing callback')

def scenarios(h,scale,seed):
    rng=random.Random(seed)
    def run(name,args,setup=lambda m:None,solid=None):
        stale=(rng.getrandbits(16),rng.getrandbits(16))
        ctx=dict(solid=solid if solid is not None else rng.choice([-32768,-100,0,100,16384,16385,32767,0x12348000]),
                 effect=rng.getrandbits(32),stale=stale)
        target=getattr(h,'mutation_function',None)
        if target and name!=target and not (target=='GetGroundHeight' and name=='GetGroundHeightEx'):
            return
        def wrap(m):
            setup(m)
            # Original slope locals differ by the wrapper's 32-byte frame.
            sp=mr.DEFAULT_SP-64-(32 if name=='GetGroundHeightEx' else 0)+16
            m.write16(sp,stale[0]); m.write16(sp+2,stale[1])
        h.run(name,args,wrap,ctx)
    reps=max(1,round(scale))
    for n in [-32768,-1,0,1,2,3,31,64,257]:
        run('ClearWords',(0x801E0000,n),lambda m:m.write_bytes(0x801E0000,rng.randbytes(1100)))
    for code in range(256):
        run('StageExitIsActive',(),lambda m,c=code:m.write8(0x800F447C,c))
    for i in range(33):
        def setup(m,i=i):
            for k in range(32): m.write32(0x800F51F0+4*k,0 if k>=i else 0x80100000+4*k)
        run('TargetListAdd',(0x80123456,),setup)
    for ptr in [0,0x80123456,0xFFFFFFFF]:
        def setup(m,ptr=ptr):
            for k in range(32): m.write32(0x800F51F0+4*k,rng.choice([ptr,ptr,0,0x80101111]))
        run('TargetListRemove',(ptr,),setup)
    for i in [-128,-2,-1,0,1,7,15,127]:
        for slot in [-32768,-1,0,15,32767]:
            def setup(m,i=i,slot=slot):
                m.write_bytes(0x800E0000,rng.randbytes(0x10000))
                m.write8((0x800E77B0+slot)&0xFFFFFFFF,i)
            for name,args in [('ObjSlotGetAnimIndex',(slot+65536,)),('ObjSlotSetField_CF',(slot+65536,rng.getrandbits(32))),
                              ('ObjSlotGetTarget',(slot&0xFFFFFFFF,0x801E0000))]: run(name,args,setup)
    for v in [0,1,255,256,0xFFFFFFFF,rng.getrandbits(32)]: run('SetDAT_800E77E2',(v,))
    for i in [-1,0,1,7]:
        def setup(m,i=i):
            m.write_bytes(0x800E6960,rng.randbytes(0x1000))
            m.write8(0x800E77B0,0xFF); m.write8(0x800F77B0,i)
        run('ObjSlotGetTarget',(65536,0x801B0000),setup)
    for counts in [[],[0],[1],[2,3],[-1],[32767,32767,32767],[-32768,32767]]:
        for index in [-2,-1,0,1,4,5,32767,0x10000]:
            def setup(m,counts=counts):
                # Full signed-index range, safely below stack.
                m.write_bytes(0x801C0000,rng.randbytes(0x30000))
                m.write32(0x800F456C,0x801D0000); m.write32(0x800F45D8,len(counts))
                m.write32(0x800F4570,0x801E0030)
                for k,n in enumerate(counts): m.write16(0x801D0000+8*k,n)
            # Positive huge indices can exceed RAM: choose backed indices for positive total.
            if index==32767 and sum(counts)>32767: continue
            run('GimmickGetInfo',(index,0x801B0000),setup)
    for event in list(range(18))+[0xFFFFFFFF]:
        for state in [-1,0,1,2,3,4,5]:
            for arg in range(5):
                def setup(m,event=event,state=state,arg=arg):
                    m.write32(0x800F4570,0x801C0000); m.write32(0x800F455C,0x801D0000)
                    m.write32(0x800B4D64,0x801B0000)
                    m.write_bytes(0x801C0000,rng.randbytes(24)); m.write16(0x801C0004,0)
                    m.write16(0x801C0000,43 if arg%2 else 1); m.write16(0x801C0002,state)
                    m.write16(0x801C0010,arg-2)
                    m.write_bytes(0x801D0000,rng.randbytes(56)); m.write16(0x801D000E,arg-2)
                    m.write16(0x800F4476,41 if arg%2 else 1)
                run('GimmickEvent',(event,0,arg),setup)
    run('GimmickEvent',(1,-1,0))
    # Real area grid: reconstruct AreaLoad's pointer fixups, retaining original heights/cells.
    blob=(ROOT/f'extract/members/{dump_areas.area_members()[1]:03X}.bin').read_bytes()
    import struct
    ah=struct.unpack_from('<I',blob,0x4D0)[0]; grid=ah+4*struct.unpack_from('<I',blob,ah+4)[0]
    w,d,nc=struct.unpack_from('<HHI',blob,grid)
    cells=grid+8; heights=cells+w*d*12+nc*4
    def real(m):
        m.write32(0x800F44E4,mr.AREA_BASE+ah); m.write16(0x800EBFA0,w); m.write16(0x800EBFA2,d)
        m.write32(0x800EBFA8,mr.AREA_BASE+cells); m.write32(0x800EBFB4,mr.AREA_BASE+heights)
    table=struct.unpack_from('<I',blob,0x4D4)[0]
    groups=struct.unpack_from('<I',blob,table)[0]; instances=table+4+8*groups
    total=sum(struct.unpack_from('<h',blob,table+4+8*k)[0] for k in range(groups))
    collision=ah+4*struct.unpack_from('<I',blob,ah+16)[0]
    colliders=collision+struct.unpack_from('<I',blob,collision+8)[0]+2
    def real_gimmicks(m):
        real(m); m.write32(0x800F456C,mr.AREA_BASE+table+4)
        m.write32(0x800F45D8,groups); m.write32(0x800F4570,mr.AREA_BASE+instances)
        m.write32(0x800F455C,mr.AREA_BASE+colliders); m.write32(0x800B4D64,0x801B0000)
        m.write16(0x800F4476,1)
        for k in range(groups): m.write16(mr.AREA_BASE+table+6+8*k,0)
    for ix in range(-1,total+1): run('GimmickGetInfo',(ix,0x801B1000),real_gimmicks)
    for ix in range(total):
        for event in range(16): run('GimmickEvent',(event,ix,ix),real_gimmicks)
    for _ in range(50*reps):
        args=(rng.randrange(w*512)-w*256,rng.randint(-20000,20000),rng.randrange(d*512)-d*256)
        run('GetGroundHeight',args,real); run('GetGroundHeightEx',(*args,0x801B0000),real)
    # All split classes, triangle quadrants, signed slope extremes and exact diagonal boundaries.
    pairs=[(0,0),(511,0),(0,511),(511,511),(256,256),(255,256),(256,255),
           (200,400),(400,200),(100,200),(200,100)]
    for flag in [0,0x800,0x1800,0x2800,0x3800,0x4800,0x5800,0x6800,0x7800]:
        for fx,fz in pairs:
            for ri in range(4*reps):
                vals=([0,1000,1000,0,500] if ri==0 else
                      [rng.choice([-32768,-32767,-1,0,1,32766,32767,rng.randint(-32768,32767)]) for _ in range(5)])
                def setup(m,flag=flag,vals=vals):
                    m.write32(0x800F44E4,0x801A0000); m.write32(0x801A0000,0)
                    m.write16(0x800EBFA0,1); m.write16(0x800EBFA2,1)
                    m.write32(0x800EBFA8,0x801A0100); m.write32(0x801A0100,flag|0xBEEF0000)
                    m.write32(0x800EBFB4,0x801A0200)
                    for k,v in enumerate(vals): m.write16(0x801A0200+2*k,v)
                args=(fx-256,rng.randint(-32768,32767),fz-256)
                run('GetGroundHeight',args,setup); run('GetGroundHeightEx',(*args,0x801B0000),setup)
    for x,z in [(-257,0),(256,0),(0,-257),(0,256),(0x7FFFFFFF,0),(-0x80000000,0)]:
        run('GetGroundHeight',(x,0,z),setup)
    for solid in [-32768,0,16384,16385,32767]:
        for name in ['GetGroundHeight','GetGroundHeightEx']:
            def disabled(m): m.write32(0x800F44E4,0x801A0000); m.write32(0x801A0000,2)
            run(name,(1,-1,3)+((0x801B0000,) if name.endswith('Ex') else ()),disabled,solid)
    # A logical x-index shift can pass the upper bound only for widths above
    # 32768. Back all cell/height reads of the adjacent accepted index in RAM.
    def wide_grid(m):
        m.write32(0x800F44E4,0x801A0000); m.write32(0x801A0000,0)
        m.write16(0x800EBFA0,65535); m.write16(0x800EBFA2,1)
        m.write32(0x800EBFA8,0x801A0100); m.write32(0x800EBFB4,0x801A0200)
        for index in [32767,32768]:
            m.write32(0x801A0100+4*index,0)
    for x in [255,256]:
        run('GetGroundHeight',(x,0,-256),wide_grid,32767)
        run('GetGroundHeightEx',(x,0,-256,0x80190000),wide_grid,32767)

def mutation_sources(original):
    """Function-scoped mistakes with checked anchors so source edits cannot drift.

    Repeated expressions in one function change together (for example division
    in both slope regions), modeling a consistently wrong implementation rule.
    """
    specs=[
        ('ClearWords','word count bound','i < n','i <= n'),
        ('ClearWords','word stride','p += 4','p += 2'),
        ('StageExitIsActive','exit codes','code != 7','code != 8'),
        ('TargetListAdd','last slot omitted','i<32','i<31'),
        ('TargetListAdd','wrong return index','return i;','return i+1;'),
        ('TargetListRemove','last slot omitted','i<32','i<31'),
        ('TargetListRemove','only first duplicate','if (ae_rd32(r,TARGETS+4*i)==p)','if (i==0 && ae_rd32(r,TARGETS+4*i)==p)'),
        ('ObjSlotGetAnimIndex','unsigned object index','ae_rds8','ae_rd8'),
        ('ObjSlotGetAnimIndex','unsigned slot cast','s16((uint32_t)slot)','(uint16_t)slot'),
        ('ObjSlotSetField_CF','wrong field offset','+0xCF','+0xCE'),
        ('ObjSlotSetField_CF','wrong empty sentinel','i!=-1','i!=0'),
        ('ObjSlotGetTarget','unsigned coordinates','ae_rds16(r,obj+0x9A+2*k)','ae_rd16(r,obj+0x9A+2*k)'),
        ('ObjSlotGetTarget','truncated slot','SLOTS+slot','SLOTS+(uint32_t)s16(slot)'),
        ('SetDAT_800E77E2','wrong byte address','0x800E77E2u','0x800E77E3u'),
        ('GimmickGetInfo','upper bound off by one','ix>=(int32_t)count','ix>(int32_t)count'),
        ('GimmickGetInfo','unsigned group count','ae_rds16(r,groups+8u*i)','ae_rd16(r,groups+8u*i)'),
        ('GimmickGetInfo','reject negative index','if(ix>=','if(ix<0 || ix>='),
        ('GimmickEvent','index zero rejected','index<0','index<=0'),
        ('GimmickEvent','event upper bound','event>=16','event>=15'),
        ('GimmickEvent','activation state','ae_wr16(r,g+2,1);\n        ae_wr16(r,col+24','ae_wr16(r,g+2,2);\n        ae_wr16(r,col+24'),
        ('GimmickEvent','special close state','ae_wr16(r,g+2,5); CALL','ae_wr16(r,g+2,3); CALL'),
        ('GimmickEvent','timer source','ae_wr16(r,g+6,ae_rd16(r,g+10))','ae_wr16(r,g+6,ae_rd16(r,g+8))'),
        ('GimmickEvent','transition state','else if(state==1) ae_wr16(r,g+2,2)','else if(state==1) ae_wr16(r,g+2,3)'),
        ('GimmickEvent','event camera state','ae_wr32(r,0x800F4450u,7)','ae_wr32(r,0x800F4450u,6)'),
        ('GimmickEvent','parameter sixteen','ae_wr16(r,g+16,1)','ae_wr16(r,g+16,2)'),
        ('GimmickEvent','parameter fourteen','ae_wr16(r,g+14,1)','ae_wr16(r,g+14,2)'),
        ('GimmickEvent','conditional state','case 0x8005BAA4u: if(!ae_rds16(r,g+2)) ae_wr16(r,g+2,1)','case 0x8005BAA4u: if(!ae_rds16(r,g+2)) ae_wr16(r,g+2,2)'),
        ('GimmickEvent','unconditional state','case 0x8005BAC4u: ae_wr16(r,g+2,1)','case 0x8005BAC4u: ae_wr16(r,g+2,2)'),
        ('GimmickEvent','argument state bound','if(arg<4)','if(arg<=4)'),
        ('GimmickEvent','close-only state','case 0x8005BB0Cu: if(ae_rds16(r,g+2)==4) ae_wr16(r,g+2,5)','case 0x8005BB0Cu: if(ae_rds16(r,g+2)==4) ae_wr16(r,g+2,2)'),
        ('GimmickEvent','timer reset','ae_wr16(r,g+6,0)','ae_wr16(r,g+6,1)'),
        ('GimmickEvent','no-op dispatch writes','case 0x8005BB44u: break;','case 0x8005BB44u: ae_wr16(r,g+2,1); break;'),
        ('GimmickEvent','activation hook arguments','CALL(GimmickActivate,ae_rds16(r,g+12),g,0)','CALL(GimmickActivate,g,ae_rds16(r,g+12),0)'),
        ('GimmickEvent','sound hook ID','CALL(PlaySfxAt,117,col,0)','CALL(PlaySfxAt,118,col,0)'),
        ('GimmickEvent','mesh hook index','ae_rds16(r,g+16),0);','ae_rds16(r,g+14),0);'),
        ('GimmickEvent','overlay hook coordinates','ae_rds16(r,col+2),ae_rds16(r,col+4)','ae_rds16(r,col+4),ae_rds16(r,col+2)'),
        ('GimmickEvent','camera shake argument','CALL(CameraShake,0,0,0)','CALL(CameraShake,1,0,0)'),
        ('GimmickEvent','event camera argument','CALL(EventCamStart,0,0,0)','CALL(EventCamStart,1,0,0)'),
        ('GimmickEvent','skip fail-closed return','if(ae_ram_failed(r)) return 0;','/* missing failure check */'),
        ('GimmickEvent','skip fail-closed write guard','if(!ae_ram_failed(r)) ae_wr32','ae_wr32'),
        ('GetGroundHeight','unsigned x shift','(int32_t)(xx<<7)>>16','(int32_t)((xx<<7)>>16)'),
        ('GetGroundHeight','grid upper bound','ix>=(int32_t)w','ix>(int32_t)w'),
        ('GetGroundHeight','diagonal equality','fx<fz','fx<=fz'),
        ('GetGroundHeight','wrong region operand','512-fx<fz','512-fz<fz'),
        ('GetGroundHeight','first region label','case 0: case 0x90','case 4: case 0x90'),
        ('GetGroundHeight','second region label','case 1: case 0xA1','case 5: case 0xA1'),
        ('GetGroundHeight','third region label','case 2: case 0x92','case 6: case 0x92'),
        ('GetGroundHeight','fourth region label','case 3: case 0xB3','case 7: case 0xB3'),
        ('GetGroundHeight','division becomes arithmetic shift','fx*sx/512','((fx*sx)>>9)'),
        ('GetGroundHeight','negative interpolation sign','(-sx)*(512-fx)/512','sx*(512-fx)/512'),
        ('GetGroundHeight','slope halfword truncation','s16((uint32_t)(b-a))','(b-a)'),
        ('GetGroundHeight','half slope rounding','sx/2','(sx>>1)'),
        ('GetGroundHeight','missing scratch initialization','ae_wr16(r,SCR,0xFFFF)','ae_wr16(r,SCR,0)'),
        ('GetGroundHeight','missing cell scratch write','ae_wr16(r,SCR+8,cell>>16)','(void)cell'),
        ('GetGroundHeight','missing slope scratch write','ae_wr16(r,SCR+12,(uint32_t)sx)','(void)sx'),
        ('GetGroundHeight','solid tie comparison','terrain<solid','terrain<=solid'),
        ('GetGroundHeight','solid return truncation','s16((uint32_t)CALL(GetSolidHeight,x,y<terrain?y:terrain,z))','CALL(GetSolidHeight,x,y<terrain?y:terrain,z)'),
        ('GetGroundHeight','solid hook y clamp','y<terrain?y:terrain','y>terrain?y:terrain'),
        ('GetGroundHeight','surface bit mask','&0xFFE0','&0xFFFF'),
        ('GetGroundHeightEx','swapped output slopes','out+2,ae_rd16(r,SCR+14)','out+2,ae_rd16(r,SCR+12)'),
        ('GetGroundHeightEx','wrong ground arguments','ae_GetGroundHeight(r,h,x,y,z)','ae_GetGroundHeight(r,h,z,y,x)'),
    ]
    for function,label,before,after in specs:
        start=original.index('ae_'+function+'(')
        end=original.index('\n}',start)+2 if '\n}' in original[start:] else len(original)
        # The leaf setter is a one-line function; stop before the next comment.
        if function=='SetDAT_800E77E2': end=original.index('\n',start)
        body=original[start:end]
        assert before in body, (function,label,before)
        yield function+': '+label, original[:start]+body.replace(before,after)+original[end:]


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--coverage',action='store_true')
    ap.add_argument('--cases',type=float,default=1); ap.add_argument('--seed',type=int,default=94423)
    ap.add_argument('--trapv',action='store_true'); ap.add_argument('--mutate',action='store_true')
    a=ap.parse_args(); start=time.time()
    if a.mutate:
        original=(ROOT/'src/world/ae_world.c').read_text()
        mutations=list(mutation_sources(original)); survivors=[]
        for i,(label,source) in enumerate(mutations):
            path=ROOT/f'extract/mips/world_mutation_{i}.c'; path.write_text(source)
            h=WorldHarness(path,tag=f'world_mutation_{i}')
            h.mutation_function=label.split(':',1)[0]
            try: scenarios(h,a.cases,a.seed)
            except SystemExit:
                if not h.failures: raise
            if not h.failures: print('SURVIVED',label); survivors.append(label)
            else: print('caught:',label)
        print(f'Mutations: {len(mutations)}; caught: {len(mutations)-len(survivors)}; survivors: {len(survivors)}')
        return int(bool(survivors))
    h=WorldHarness(trapv=a.trapv); scenarios(h,a.cases,a.seed)
    for n,k in h.counts.items(): print(f'{n:28} {k}')
    summary=h.finish(EXTRA_COVERAGE,show_coverage=a.coverage)
    gaps=any(s['uncovered_count'] or s['stale_declared'] for s in summary.values())
    if a.cases==1 and a.seed==94423 and not a.trapv:
        reg.write_results(h.counts,summary,not h.failures and not gaps,function_names=list(FUNCS))
    if gaps: print('FAIL: undeclared coverage gaps or stale exclusions'); return 1
    print(f'OK: {sum(h.counts.values())} cases; {time.time()-start:.1f}s')
    return int(bool(h.failures))
if __name__=='__main__': sys.exit(main())
