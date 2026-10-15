"""Compare the hand-written solid queries with SCUS-94423, including RAM and scratchpad."""
import argparse
import ctypes as C
from pathlib import Path
import random
import os
import shutil
import subprocess
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mips'))
import check_world as world
import monkey_harness as mh
import mips_ram as mr
import exclusion_registry as reg
sys.path.insert(0,str(ROOT/'tools'))
import dump_areas

FUNCS = {
    'CollProbeCounterInc': (0x80071D20, 28, 's', False, ''),
    'GetColliderQuadrantMaskXZ': (0x800889B4, 56, 's', False, 'ss'),
    'ColliderSlopeCorner': (0x8004FE48, 216, 's', False, 'uu'),
    'ColliderSlopeSide': (0x8004FC8C, 444, 's', False, 'uusu'),
    'ColliderHalfSpaceInside': (0x8004FF20, 168, 's', False, 'uu'),
    'ColliderTypeActive': (0x80079A28, 224, 's', False, 'us'),
    'ColliderStripHeight': (0x8004F548, 416, 's', False, 'uu'),
    'ColliderStripHeightOut': (0x8004F514, 52, 's', False, 'uuu'),
    'CollShape_Box': (0x8004E33C, 264, 's', False, 'suuu'),
    'CollShape_Ramp2': (0x8004ED30, 356, 's', False, 'suuu'),
    'CollShape_Ramp4': (0x8004EE94, 364, 's', False, 'suuu'),
    'CollShape_Ramp45': (0x8004EA1C, 544, 's', False, 'suuu'),
    'CollShape_Wedge': (0x8004F000, 816, 's', False, 'suuu'),
    'CollHalfPipeHeight': (0x8004FFC8, 272, 's', False, 'suuu'),
    'CollShape_Cylinder': (0x8004EC3C, 244, 's', False, 'suuu'),
    'ColliderRingHeight': (0x8004F438, 220, 's', False, 'uuu'),
    'DiagonalSignedDistance': (0x800774B8, 660, 's', False, 'uuuu'),
    'ColliderTrenchHeight': (0x800500D8, 952, 's', False, 'suuu'),
    'CollShape_WallThin': (0x8004E444, 504, 's', True, 'suuu'),
    'CollShape_WallEdge': (0x8004E63C, 700, 's', True, 'suuu'),
    'CollShape_WallDiagonal': (0x8004E8F8, 292, 's', False, 'suuu'),
    'CollShape_DiagonalBox': (0x8004F330, 264, 's', False, 'suuu'),
    'ColliderSlopeHeight': (0x8004F984, 776, 's', True, 'uuuu'),
    'GetSurfaceType': (0x800797F8, 560, 's', False, 'sss'),
    'ColliderHeightAt': (0x8004DF38, 1028, 's', True, 'usss'),
    'GetSolidHeight': (0x8004D8C0, 1656, 's', True, 'sss'),
    'VectorNormalS': (0x8003019C, 20, 's', False, 'uu'),
}
EXTRA_COVERAGE = {}
COL, POINT, OUT = 0x801D0000, 0x801D0100, 0x801D0200

class Hooks(C.Structure):
    _fields_=[('stale_wall_axis',C.c_int32),('stale_edge_x',C.c_int16),
              ('stale_edge_z',C.c_int16),('stale_initial_layer',C.c_int32),
              ('stale_slope_corner_x',C.c_int32),('stale_slope_corner_z',C.c_int32),
              ('stale_slope_angle_low',C.c_int16),('stale_slope_angle_high',C.c_int16)]

def build(source=None, tag='solid', trapv=False):
    out = ROOT/'extract/mips'; out.mkdir(parents=True,exist_ok=True)
    dll = out/f'{tag}_{os.getpid()}_{time.time_ns()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11',*(['-O1','-ftrapv'] if trapv else ['-O2']),'-Wall','-Wextra','-Werror',
                    '-shared','-static-libgcc','-I'+str(ROOT/'src/world'),
                    str(ROOT/'src/core/ae_ram.c'),str(ROOT/'src/core/ae_sdk.c'),
                    str(ROOT/'src/player/native_player.c'),
                    str(source or ROOT/'src/world/ae_solid.c'),'-o',str(dll),'-lm'],check=True)
    return dll

class SolidHarness(mh.MonkeyHarness):
    oracle_cpu = None
    def __init__(self, source=None, tag='solid', trapv=False):
        self.funcs = FUNCS
        self.m = mr.MipsRam.from_extract()
        if SolidHarness.oracle_cpu is None:
            self.m.attach_gte_oracle(); SolidHarness.oracle_cpu=self.m
        else:
            # All runs are sequential; reuse the loaded Windows DLL, reset before every call.
            self.m.gte_state=SolidHarness.oracle_cpu.gte_state
            self.m.gte_backend=SolidHarness.oracle_cpu.gte_backend
        self.m.coverage = set()
        self.base = self.m.snapshot()
        # Reuse the world's ctypes RAM adapter, with this module's function signatures.
        previous = world.FUNCS
        world.FUNCS = {n:s for n,s in FUNCS.items() if n!='VectorNormalS'}
        try:
            self.c = world.Side(build(source, tag=tag,trapv=trapv))
        finally:
            world.FUNCS = previous
        self.lib = self.c.lib
        self.lib.ae_sdk_VectorNormalS.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32]
        self.lib.ae_sdk_VectorNormalS.restype=C.c_int32
        self.stack_lo = 0x1FF000
        self._mask = []
        self.counts = {}
        self.failures = 0
        self.stop_after = 1
        self.only = None
        reg.declare('stack_window', '801FF000..80200000')

    def run(self, name, args, setup=lambda m: None, stale=None, allow_trap=False):
        if self.only is not None and name not in self.only: return
        self.m.restore(self.base)
        self.m.gte_reset()
        setup(self.m)
        self.sync_to_c()
        hooks=Hooks(*(stale or (0,0,0,0)))
        if stale is not None:
            reg.declare('shared_random','solid undefined caller axis and layer')
            if name=='ColliderSlopeHeight' and len(stale)>4:
                reg.declare('shared_random','ColliderSlopeHeight stale corner and angles')
        trapped=False
        try:
            regs = self.m.call(FUNCS[name][0], args,initial_regs={8:hooks.stale_wall_axis,
                                  9:hooks.stale_edge_z,10:hooks.stale_edge_x,21:hooks.stale_initial_layer})
        except mr.MipsError:
            if not allow_trap: raise
            trapped=True
        f=getattr(self.lib, 'ae_sdk_VectorNormalS' if name=='VectorNormalS' else 'ae_'+name)
        result=f(self.c.ram,*([C.byref(hooks)] if FUNCS[name][3] else []),*args)
        self.counts[name] = self.counts.get(name, 0) + 1
        if trapped:
            if not self.lib.ae_ram_failed(self.c.ram): self.fail(name,str(args),'original trapped but C did not')
            self.lib.ae_ram_clear_error(self.c.ram)
            self.compare(name,str(args),0,0,[],[],'v')
            return
        self.compare(name, str(args), regs[2], result, [], [], 's')

def scenarios(h, scale=1, seed=94423):
    rng = random.Random(seed)
    for value in [0, 1, 0x7FFFFFFF, 0xFFFFFFFF]:
        h.run('CollProbeCounterInc', (), lambda m,v=value:m.write32(0x800B2130,v))
    coords = [-0x80000000, -65537, -32769, -32768, -1, 0, 1, 32767, 32768, 65535, 65536, 0x7FFFFFFF]
    for x in coords:
        for z in coords:
            h.run('GetColliderQuadrantMaskXZ', (x,z))
    for typ in range(256):
        def setup(m, typ=typ):
            m.write_bytes(COL, rng.randbytes(64))
            m.write8(COL+16, typ)
            m.write_bytes(POINT, rng.randbytes(12))
            m.write_bytes(OUT, rng.randbytes(16))
        h.run('ColliderSlopeCorner', (COL,OUT), setup)
        h.run('ColliderSlopeSide', (COL,POINT,rng.getrandbits(32),OUT), setup)
        h.run('ColliderHalfSpaceInside', (COL,POINT), setup)
    for typ in [0x46,0x47,0x48,0x49,0x4E,0x4F,0x50,0x51]:
        for radius in [0,1,100,-1,0x7FFFFFFF,-0x80000000]:
            for x,z in [(0,0),(99,0),(100,0),(101,0),(-100,100),(-101,-101),(0,101),(0,-101)]:
                def setup(m,typ=typ,x=x,z=z):
                    m.write_bytes(COL,bytes(64)); m.write8(COL+16,typ)
                    m.write16(COL+6,100); m.write16(COL+10,100)
                    m.write32(POINT,x&0xFFFFFFFF); m.write32(POINT+8,z&0xFFFFFFFF)
                    m.write_bytes(OUT,b'\xA5'*16)
                if typ<74:h.run('ColliderSlopeSide',(COL,POINT,radius,OUT),setup)
                elif radius==0:h.run('ColliderHalfSpaceInside',(COL,POINT),setup)
    for _ in range(round(20*scale)):
        def setup(m):
            m.write_bytes(COL,rng.randbytes(64));m.write8(COL+16,rng.randrange(70,74))
            m.write_bytes(OUT,rng.randbytes(16))
        h.run('ColliderSlopeCorner',(COL,OUT),setup)
    for probe in [0,1,2]:
        for material in range(16):
            for typ in [0,54,78,81,82]:
                for enabled in [0,1]:
                    h.run('ColliderTypeActive',(material*4096+typ,enabled),
                          lambda m,p=probe:m.write16(0x800F45E6,p&65535))
    for typ in [0x4A,0x4C,0x4B,0x4D]:
        for direction in [0,90,-90]:
            for x,z in [(0,0),(100,0),(-100,0),(0,100),(0,-100),(101,0),(0,101),(73,73),(0,72)]:
                def setup(m,typ=typ,direction=direction,x=x,z=z):
                    m.write_bytes(COL,bytes(64)); m.write8(COL+16,typ)
                    m.write16(COL+6,100); m.write16(COL+8,300); m.write16(COL+10,50)
                    m.write16(COL+44,direction&65535)
                    m.write32(POINT,x&0xFFFFFFFF); m.write32(POINT+8,z&0xFFFFFFFF)
                h.run('ColliderStripHeight',(COL,POINT),setup)
                h.run('ColliderStripHeightOut',(COL,POINT,OUT),setup)
    for name in ['CollShape_Box','CollShape_Ramp2','CollShape_Ramp4','CollShape_Ramp45']:
        for typ in [-1,0,7,8,9,10,11,12,13,14,15,16,17,18,*range(19,35),60,61,62,63,64,65,66,67,68]:
            for x,y,z in [(0,0,0),(-100,-500,100),(100,500,-100),(101,0,0),(0,0,101)]:
                def setup(m,x=x,y=y,z=z):
                    m.write_bytes(COL,bytes(64))
                    for off,v in [(0,100),(2,300),(4,100)]:m.write16(COL+off,v)
                    for off,v in [(0,x),(4,y),(8,z)]:m.write32(POINT+off,v&0xFFFFFFFF)
                    m.write_bytes(OUT,b'\xA5'*16)
                    m.write32(OUT,0)
                h.run(name,(typ,COL,POINT,OUT),setup)
    for name,types in [('CollShape_Wedge',[0,85,86,87,88,89]),('CollHalfPipeHeight',[36,37]),
                       ('CollShape_Cylinder',[35,36,37,59,69,70,71,72,73,82,83,84]),
                       ('ColliderTrenchHeight',[83,84])]:
        for typ in types:
            for ex,ez,height in [(32,32,301),(500,600,301),(500,600,-301)]:
                for x,y,z in [(0,0,0),(100,-700,101),(-100,700,-101),(100,-100,101),
                              (500,100,600),(500,0,0),(501,0,0),(0,0,601),(200,200,200),(200,-200,200),(0,308,0)]:
                    def setup(m,ex=ex,ez=ez,height=height,x=x,y=y,z=z):
                        m.write_bytes(COL,bytes(64))
                        for off,v in [(0,ex),(2,height),(4,ez)]:m.write16(COL+off,v&65535)
                        for off,v in [(0,x),(4,y),(8,z)]:m.write32(POINT+off,v&0xFFFFFFFF)
                        m.write_bytes(OUT,b'\xA5'*16)
                        m.write32(0x800AF2EC,123);m.write32(0x800AF2F0,456)
                    h.run(name,(typ,COL,POINT,OUT),setup)
    for radius in [0,100,1000,-100]:
        for distance in [0,67,68,69,99,100,101,131,132,133,963,964,965,1035,1036,1037]:
            def setup(m,radius=radius,distance=distance):
                m.write_bytes(COL,bytes(64));m.write16(COL+6,radius&65535);m.write16(COL+8,333)
                m.write32(POINT,distance);m.write32(POINT+8,0)
            h.run('ColliderRingHeight',(COL,POINT,OUT),setup)
    for orientation in [0,0x400,0x800,0xC00,0x100,0x900]:
        for x,z in [(0,0),(100,100),(-100,100),(100,-100),(-100,-100),(101,100),(-101,100)]:
            def setup(m,x=x,z=z):
                m.write_bytes(COL,bytes(64));m.write16(COL,11);m.write16(COL+4,17)
                m.write32(POINT,x&0xFFFFFFFF);m.write32(POINT+8,z&0xFFFFFFFF)
                m.write_bytes(OUT,b'\xA5'*16)
            h.run('DiagonalSignedDistance',(orientation,POINT,COL,OUT),setup)
    # Wall strip intersection, each edge/corner, both boundary values and both sides.
    for name,types in [('CollShape_WallThin',[3,4,5,6,42,43,44,45,74,75,76,77]),
                       ('CollShape_WallEdge',list(range(46,54))+[0]),
                       ('CollShape_WallDiagonal',list(range(55,59))),
                       ('CollShape_DiagonalBox',list(range(38,42)))]:
        for typ in types:
            for probe in [0,1]:
                pairs=[(0,0),(64,65),(65,64),(-65,-64),(64,-65),(-100,100),(100,-100),
                       (-320,-320),(-192,-192),(320,320),(192,192),(-320,320),(320,-320),
                       (-321,100),(100,-321),(321,100),(100,321),(0,91),
                       (0,-320),(0,-192),(-320,0),(-192,0),(0,320),(0,192),(320,0),(192,0)]
                for x,z in (pairs if probe or name=='CollShape_DiagonalBox' else pairs[:1]):
                    if name=='CollShape_DiagonalBox' and probe:continue
                    def setup(m,x=x,z=z,probe=probe):
                        m.write_bytes(COL,bytes(64));m.write16(COL,300);m.write16(COL+2,301);m.write16(COL+4,300)
                        m.write32(POINT,x&0xFFFFFFFF);m.write32(POINT+8,z&0xFFFFFFFF)
                        m.write_bytes(OUT,b'\xA5'*16);m.write16(0x800F45E6,probe)
                    h.run(name,(typ,COL,POINT,OUT),setup,stale=(1,77,-77,0))
    for typ in range(70,74):
        for x,y,z,radius in [(0,0,0,0),(0,1,0,0),(500,-1,500,0),(-500,0,-500,0),
                             (501,0,501,0),(-501,0,-501,100),(500,0,-500,100),
                             (-500,0,500,100),(700,0,700,1000),(100,0,200,0),
                             (200,0,100,0),(0,0,100,0),(100,0,0,0),(-100,0,0,0),(0,0,-100,0)]:
            def setup(m,typ=typ,x=x,y=y,z=z,radius=radius):
                m.write_bytes(COL,bytes(64));m.write8(COL+16,typ)
                for off,v in [(6,500),(8,600),(10,500),(36,100),(38,100)]:m.write16(COL+off,v)
                for off,v in [(0,x),(4,y),(8,z)]:m.write32(POINT+off,v&0xFFFFFFFF)
                m.write_bytes(OUT,bytes(32));m.write32(OUT,radius)
            h.run('ColliderSlopeHeight',(COL,POINT,OUT,OUT+16),setup)
    for vector in [(0,0,0),(1,2,3),(-32768,0,32767),(123456,-100000,777),(-32768,-32768,-32768)]:
        def setup(m,vector=vector):
            for k,v in enumerate(vector):m.write32(POINT+4*k,v&0xFFFFFFFF)
        h.run('VectorNormalS',(POINT,OUT),setup,allow_trap=True)
    for family in [0,1,2,3,4,5,9,15]:
        for typ in [0,36,37,54,68,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84]:
            for x,y,z in [(0,-100,0),(600,-100,0),(0,100,600),(0,100,0)]:
                def setup(m,family=family,typ=typ,x=x,y=y,z=z):
                    m.write_bytes(COL,bytes(64));m.write16(COL+16,family*4096+typ)
                    for off,v in [(6,500),(8,600),(10,500),(20,32768),(36,100),(38,100)]:m.write16(COL+off,v)
                    m.write16(0x800F45E6,1)
                h.run('ColliderHeightAt',(COL,x,y,z),setup,stale=(0,0,0,0))
    for surface in [0,9,0x409,0x509,0xF09]:
        for x,y,z in [(0,0,0),(101,0,0),(-101,0,0),(0,0,101),(0,0,-101),
                       (0,1,0),(0,-101,0),(0,-100,0),(0,-99,0),(0,-50,0),(0,0,0)]:
            def setup(m,surface=surface):
                m.write16(0x800F45D4,2);m.write32(0x800F455C,COL)
                m.write_bytes(COL,bytes(112));m.write16(COL+18,surface)
                for off in [6,8,10]:m.write16(COL+off,100)
                for off,v in [(20,123),(22,456),(28,789),(42,-1),(44,2),(46,-3)]:m.write16(COL+off,v&65535)
                m.scratch[:]=rng.randbytes(1024)
            h.run('GetSurfaceType',(x,y,z),setup)
    def empty(m):
        m.write32(0x800F45F8,512);m.write32(0x800F45FC,512);m.write32(0x800F45F0,2)
        m.write32(0x800F4558,COL);m.write32(0x800F4560,POINT)
        m.write_bytes(POINT,b'\xFF'*8);m.write16(0x800F45D4,0)
        m.write16(0x800F45E0,65535);m.write16(0x800F45E6,0);m.write16(0x800F45E8,0)
        m.write32(0x800AF2E4,0x1F8003F0);m.write32(0x800AF2E8,0x1F8003D4)
    for x,y,z in [(0,0,0),(-512,0,0),(512,0,0),(0,0,-512),(0,0,512),(0,1,0),(0,40000,0)]:
        h.run('GetSolidHeight',(x,y,z),empty,stale=(0,0,0,-1))
    # Each original grid dispatch code, one shape; disabled surface queries too.
    for typ in range(89):
        def setup(m,typ=typ):
            empty(m);m.write_bytes(COL,bytes(64));m.write16(POINT+6,0)
            m.write16(COL+2,0);m.write16(COL+6,256);m.write16(COL+8,300);m.write16(COL+10,256)
            m.write16(COL+16,65535);m.write16(COL+18,typ);m.write16(0x800F45E6,1)
            m.write16(0x800F45E8,typ%2)
        h.run('GetSolidHeight',(256,-1024,256),setup,stale=(0,0,0,-1),allow_trap=True)
    # Original loaded area data, fixing up only the same global pointers as AreaLoad.
    for area in [1,5,8,0x10]:
        member=dump_areas.area_members()[area]
        blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        u32=lambda off:struct.unpack_from('<I',blob,off)[0]
        header=u32(0x4D0);collision=header+4*u32(header+16)
        grid=collision+u32(collision);shapes=collision+u32(collision+4);colliders=collision+u32(collision+8)
        width,depth=struct.unpack_from('<hh',blob,grid)
        count=struct.unpack_from('<H',blob,colliders)[0]
        def real(m):
            empty(m);m.load_area(blob)
            for addr,value in [(0x800F4558,mr.AREA_BASE+shapes+2),(0x800F4560,mr.AREA_BASE+grid+4),
                               (0x800F455C,mr.AREA_BASE+colliders+2),(0x800F45F0,width),
                               (0x800F45F8,width*256),(0x800F45FC,depth*256)]:m.write32(addr,value)
            m.write16(0x800F45D4,count)
        for _ in range(round(30*scale)):
            x=rng.randrange(-width*256,width*256);z=rng.randrange(-depth*256,depth*256)
            y=rng.choice([0,-1,-512,-1024,-10000,-32768])
            h.run('GetSolidHeight',(x,y,z),real,stale=(0,0,0,-1))
        # Contacts near real colliders, exact edges and the two caller query flags.
        for index in range(min(count,3)):
            offset=colliders+2+56*index
            cx,cy,cz,ex,ey,ez=struct.unpack_from('<6h',blob,offset)
            for dx,dz in [(0,0),(ex,0),(ex+1,0),(0,ez),(0,ez+1)]:
                for probe,ignore in [(0,-1),(1,index)]:
                    def setup(m,probe=probe,ignore=ignore):
                        real(m);m.write16(0x800F45E6,probe);m.write16(0x800F45E0,ignore&65535)
                    h.run('GetSolidHeight',(cx+dx,cy-1,cz+dz),setup,stale=(0,0,0,-1))
    # Arithmetic/trap edge cases and previously uncovered negative gradients.
    for typ in [74,75]:
        def setup(m,typ=typ):
            m.write_bytes(COL,bytes(64));m.write8(COL+16,typ);m.write16(COL+8,100)
            m.write_bytes(POINT,bytes(12))
        h.run('ColliderStripHeight',(COL,POINT),setup,allow_trap=True)
        h.run('ColliderStripHeightOut',(COL,POINT,OUT),setup,allow_trap=True)
    for typ in [83,84]:
        for radius in [256,500,32767,-1]:
            for x,y,z in [(0,2000,0),(0,-2000,0),(0,100,0),(100,0,100)]:
                def setup(m,radius=radius,x=x,y=y,z=z):
                    m.write_bytes(COL,bytes(64));m.write16(COL,radius&65535);m.write16(COL+4,radius&65535)
                    for off,v in [(0,x),(4,y),(8,z)]:m.write32(POINT+off,v&0xFFFFFFFF)
                    m.write32(0x800AF2EC,123);m.write32(0x800AF2F0,456)
                h.run('ColliderTrenchHeight',(typ,COL,POINT,OUT),setup,allow_trap=True)
    for typ in [70,71,72,73]:
        for inner in [-500,100]:
            for height in [-600,600]:
                for x,z in [(0,0),(100,200),(200,100),(500,500),(-500,-500),(-500,500),(500,-500)]:
                    def setup(m,typ=typ,inner=inner,height=height,x=x,z=z):
                        m.write_bytes(COL,bytes(64));m.write8(COL+16,typ)
                        for off,v in [(6,500),(8,height),(10,500),(36,inner),(38,100)]:m.write16(COL+off,v&65535)
                        m.write_bytes(POINT,bytes(12));m.write32(POINT,x&0xFFFFFFFF);m.write32(POINT+8,z&0xFFFFFFFF)
                        m.write_bytes(OUT,bytes(32));m.write32(OUT,1000)
                    h.run('ColliderSlopeHeight',(COL,POINT,OUT,OUT+16),setup)
    for typ in [0,74,75,76,77]:
        for axis in [0,1,2,3]:
            for x,z in [(100,-100),(0,-100),(64,0)]:
                def setup(m,typ=typ,x=x,z=z):
                    m.write_bytes(COL,bytes(64));m.write16(COL,10);m.write16(COL+2,100);m.write16(COL+4,10)
                    m.write_bytes(POINT,bytes(12));m.write32(POINT,x);m.write32(POINT+8,z&0xFFFFFFFF)
                    m.write16(0x800F45E6,1)
                h.run('CollShape_WallThin',(typ,COL,POINT,OUT),setup,stale=(axis,0,0,0))
    def ring_setup(m):
        m.write_bytes(COL,bytes(64));m.write16(COL+16,0x3044);m.write16(COL+6,500)
        m.write16(COL+8,600);m.write16(COL+20,32768);m.write16(0x800F45E6,1)
    h.run('ColliderHeightAt',(COL,500,-100,0),ring_setup)
    def failed_shape(m):
        empty(m);m.write_bytes(COL,bytes(64));m.write16(POINT+6,0)
        m.write16(COL+6,256);m.write16(COL+10,256);m.write16(COL+16,65535);m.write16(COL+18,70)
    h.run('GetSolidHeight',(256,0,256),failed_shape)
    def tied_surfaces(m):
        empty(m);m.write_bytes(COL,bytes(64));m.write16(POINT+6,0)
        m.write16(COL+6,256);m.write16(COL+8,300);m.write16(COL+10,256);m.write16(COL+16,65535)
        m.write32(0x800F455C,OUT);m.write16(0x800F45D4,1);m.write_bytes(OUT,bytes(56))
        for off,value in [(0,256),(4,256),(6,256),(8,300),(10,256),(32,0x1111)]:m.write16(OUT+off,value)
    h.run('GetSolidHeight',(256,-1024,256),tied_surfaces)
    def collider_surface_boundary(m):
        empty(m);m.write32(0x800F455C,COL);m.write16(0x800F45D4,2)
        m.write_bytes(COL,bytes(112))
        for off,value in [(6,300),(8,300),(10,300),(32,0x1111)]:m.write16(COL+off,value)
        for off,value in [(2,-332),(6,300),(8,100),(10,300),(16,0x3036),(18,9),(20,32768),(32,0x1111)]:
            m.write16(COL+56+off,value&65535)
    h.run('GetSolidHeight',(0,-100,0),collider_surface_boundary)
    for surface in [0x409,0x509]:
        def setup(m,surface=surface):
            empty(m);m.write_bytes(COL,bytes(112));m.write32(0x800F455C,COL);m.write16(0x800F45D4,1)
            for off,v in [(6,300),(8,500),(10,300),(16,0x3036),(18,surface),(20,32768),(32,0x1111)]:m.write16(COL+off,v)
        h.run('GetSolidHeight',(0,-100,0),setup,stale=(0,0,0,-1))
    for x,y,z in [(0x7FFFFFF0,0,0),(0,0,0x7FFFFFF0),(0,-0x80000000,0)]:
        def setup(m):
            empty(m);m.write32(0x800F45F8,0x7FFFFFFF);m.write32(0x800F45FC,0x7FFFFFFF)
            m.write32(0x800F45F0,0)
            # gx/gz truncate to zero or a positive index; use a mirrored grid base
            # so the corresponding cell word is valid without allocating an enormous grid.
            gx=mr.signed(x+0x7FFFFFFF);gz=mr.signed(z+0x7FFFFFFF)
            cellx=(abs(gx)//512)*(-1 if gx<0 else 1)
            m.write32(0x800F4560,(POINT-2*cellx)&0xFFFFFFFF)
        h.run('GetSolidHeight',(x,y,z),setup,stale=(0,0,0,-1))
    for typ in [0,69,74]:
        for cx,cz in [(0,0),(100,-100)]:
            stale=(0,0,0,-1,cx,cz,0,1024)
            def setup(m,typ=typ,cx=cx,cz=cz):
                m.write_bytes(COL,bytes(64));m.write8(COL+16,typ)
                for off,v in [(6,500),(8,600),(10,500),(36,100),(38,100)]:m.write16(COL+off,v)
                m.write_bytes(POINT,bytes(12));m.write_bytes(OUT,bytes(32));m.write32(OUT,1000)
                frame=mr.DEFAULT_SP-88
                m.write32(frame+40,cx&0xFFFFFFFF);m.write32(frame+44,cz&0xFFFFFFFF)
                m.write16(frame+48,0);m.write16(frame+50,1024)
            h.run('ColliderSlopeHeight',(COL,POINT,OUT,OUT+16),setup,stale=stale)
    for name in ['CollShape_Ramp2','CollShape_Ramp4','CollShape_Ramp45','CollShape_Cylinder','CollShape_WallEdge']:
        for typ in [-0x80000000,0x7FFFFFFF]:
            def setup(m):
                m.write_bytes(COL,bytes(64));m.write16(COL,100);m.write16(COL+2,333);m.write16(COL+4,100)
                m.write_bytes(POINT,bytes(12));m.write16(0x800F45E6,1)
            h.run(name,(typ,COL,POINT,OUT),setup,stale=(0,0,0,-1))

def check_missing_hooks(h):
    """Each path that consumes an undefined value must reject a NULL policy."""
    for name,args in [('CollShape_WallThin',(74,COL,POINT,OUT)),
                      ('CollShape_WallEdge',(0,COL,POINT,OUT)),
                      ('ColliderSlopeHeight',(COL,POINT,OUT,OUT+16)),
                      ('GetSolidHeight',(0,40000,0))]:
        h.m.restore(h.base)
        h.m.write_bytes(COL,bytes(64));h.m.write_bytes(POINT,bytes(12));h.m.write_bytes(OUT,bytes(32))
        h.m.write32(POINT,100);h.m.write16(0x800F45E6,1)
        h.m.write32(0x800F45F8,512);h.m.write32(0x800F45FC,512)
        h.sync_to_c();getattr(h.lib,'ae_'+name)(h.c.ram,None,*args)
        if not h.lib.ae_ram_failed(h.c.ram) or b'not provided' not in h.lib.ae_ram_error(h.c.ram):
            h.fail(name,'NULL policy','undefined caller value did not fail closed')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--coverage',action='store_true')
    ap.add_argument('--cases',type=float,default=1)
    ap.add_argument('--seed',type=int,default=94423)
    ap.add_argument('--only',nargs='+',choices=list(FUNCS))
    ap.add_argument('--mutate',action='store_true')
    ap.add_argument('--trapv',action='store_true')
    ap.add_argument('--mutation-start',type=int,default=0,help='diagnostic: resume at zero-based mutation index')
    a=ap.parse_args(); start=time.time()
    if a.mutate:
        original=(ROOT/'src/world/ae_solid.c').read_text()
        # Each mutation alters a named semantic decision inside one original function.
        mutations=[
            ('CollProbeCounterInc','+ 1u','+ 2u'),
            ('GetColliderQuadrantMaskXZ','(int16_t)x < 0','(uint16_t)x == 0'),
            ('ColliderSlopeCorner','type < 0x48 ? x + extent_x','type < 0x48 ? x - extent_x'),
            ('ColliderSlopeSide','max_x < px','max_x <= px'),
            ('ColliderHalfSpaceInside','case 0x4E: return ae_rds32(r, point + 8) <','case 0x4E: return ae_rds32(r, point + 8) <='),
            ('ColliderTypeActive','probe == 1','probe == 2'),
            ('ColliderStripHeight','72 < abs32(dz)','71 < abs32(dz)'),
            ('ColliderStripHeightOut','height != 0','height == 0'),
            ('CollShape_Box','ae_wr32(r, out, 0)','ae_wr32(r, out, 1)'),
            ('CollShape_Ramp2','point + 8) / 2','point + 8) / 3'),
            ('CollShape_Ramp4','point + 8) / 4','point + 8) / 5'),
            ('CollShape_Ramp45','add32(along, 256)','add32(along, 257)'),
            ('CollShape_Wedge','height < 0 ? -1 : 0','height < 0 ? 0 : 0'),
            ('CollHalfPipeHeight','height < ae_rds32','height <= ae_rds32'),
            ('CollShape_Cylinder','distance >= ae_rds16','distance > ae_rds16'),
            ('ColliderRingHeight','width = 64','width = 66'),
            ('DiagonalSignedDistance','int32_t sign = -1','int32_t sign = 1'),
            ('ColliderTrenchHeight','radius - 192 <= distance','radius - 191 <= distance'),
            ('CollShape_WallThin','abs32(x) < 65','abs32(x) < 64'),
            ('CollShape_WallEdge','edge_z = -256','edge_z = -255'),
            ('CollShape_WallDiagonal',') >= 65',') >= 64'),
            ('CollShape_DiagonalBox',') > 0',') >= 0'),
            ('ColliderSlopeHeight','arc_radius, 628','arc_radius, 627'),
            ('GetSurfaceType','<< 12','<< 11'),
            ('ColliderHeightAt','height = ae_rds16(r, collider + AE_COL_EXTENT_Y)','height = ae_rds16(r, collider + AE_COL_EXTENT_Y) + 1'),
            ('GetSolidHeight','ae_CollProbeCounterInc(r)','(void)r'),
            ('GetSolidHeight','ae_rds16(r, AE_COLL_IGNORE) == i','ae_rds16(r, AE_COLL_IGNORE) != i'),
            ('GetSolidHeight','best_height < grid_height','best_height <= grid_height'),
            ('GetSolidHeight','height, 32','height, 31'),
            ('GetSolidHeight','contact + 12, collider_slope_x','contact + 12, collider_slope_x + 1'),
            ('GetSolidHeight','contact + 6, 0','contact + 6, 1'),
            ('GetSurfaceType','(surface & 255) != 9','(surface & 255) != 8'),
        ]
        for index,(name,before,after) in enumerate(mutations):
            if index<a.mutation_start:continue
            begin=original.index('int32_t ae_'+name+'(');end=original.index('\n}',begin)+2
            body=original[begin:end];assert before in body,(name,before)
            changed=original[:begin]+body.replace(before,after,1)+original[end:]
            path=ROOT/f'extract/mips/solid_mutation_{index}.c';path.write_text(changed)
            h=SolidHarness(path,tag=f'solid_mutation_{index}');h.only={name}
            try:scenarios(h,a.cases,a.seed)
            except SystemExit as failure:
                if failure.code!=1:raise
            if not h.failures:print('SURVIVED:',name,before);return 1
            print(f'caught mutation {index+1}/{len(mutations)}: {name}: {before}',flush=True)
        print(f'OK: all {len(mutations)-a.mutation_start} selected mutations caught; {time.time()-start:.1f}s')
        return 0
    h=SolidHarness(trapv=a.trapv); h.only=set(a.only) if a.only else None; scenarios(h,a.cases,a.seed)
    if not a.only:check_missing_hooks(h)
    for name,count in h.counts.items(): print(f'{name:30} {count}')
    summary=h.finish(EXTRA_COVERAGE,show_coverage=a.coverage)
    gaps=not a.only and any(s['uncovered_count'] or s['stale_declared'] for s in summary.values())
    if a.cases==1 and a.seed==94423 and not a.only and not a.trapv:
        reg.write_results(h.counts,summary,not h.failures and not gaps,function_names=list(FUNCS))
    if gaps:print('FAIL: undeclared instruction gaps or stale proofs');return 1
    print(f'OK: {sum(h.counts.values())} cases; {time.time()-start:.1f}s')
    return int(bool(h.failures))

if __name__=='__main__': sys.exit(main())
