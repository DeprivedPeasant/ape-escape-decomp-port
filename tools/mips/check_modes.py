"""Resident modes: original EXE instructions versus handwritten C on real area RAM.

All subsystem calls are named boundaries in this unit check; integration checks
run their ported instructions. Compare returns, RAM outside the declared call
stack, all scratchpad bytes, call order, arguments and addressable local buffers.
"""
import argparse
import collections
import ctypes as C
import itertools
import random
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools/mips'), str(ROOT/'tools')]
import mips_ram as mr
import exclusion_registry as reg
import dump_areas

FUNCS = {
    'main': (0x8005BDC0, 0x124), 'Mode_04': (0x8005BEE4, 8),
    'Mode_Stage': (0x8005BF70, 0xA1C), 'Mode_06': (0x8008D1F0, 0x31C),
    'StageInitMemory': (0x80010B50, 0xEC), 'StageRenderInit': (0x8005BEF0, 0x80),
    'BumpAllocPeek': (0x8008C000, 0x1C), 'NullInit': (0x800143B0, 8),
    'ResetFrameFlip': (0x8005DDE0, 0x14),
}
# Independent API inventory from original call sites, not parsed from port C.
HOOKS = [
    ('SetVideoMode',0x8002EDF0,1),('InitSystem',0x8005CD50,0),
    ('ArcLoadMember',0x800112E4,2),('CdMgrWaitIdle',0x8001256C,1),
    ('LoadListInit',0x80011104,2),('LoadListStep',0x800116C0,0),
    ('CdMgrUpdate',0x800125B8,0),('VSync',0x8002E520,1),
    ('InitDisplay',0x8005CFEC,0),('ResetGraph',0x80028350,1),
    ('ClearImage',0x80028850,4),('DrawSync',0x800286CC,1),
    ('MatrixIdentity',0x8004637C,1),('ClearWords',0x80032708,2),
    ('StageRequest',0x8005D100,3),('PadUpdate',0x8001325C,1),
    ('BeginFrame',0x8005DF48,0),('ColorFadeReset',0x8008C390,0),
    ('DebugMenuUpdate',0x8008D50C,0),('PutDispEnv',0x80028EFC,1),
    ('PutDrawEnv',0x80028D30,1),('DrawOTag',0x80028CC0,1),
    ('RunMinigameExe',0x80011A2C,1),('TitleSequence',0x80136074,1),
    ('GalaxyEntry',0x80137654,0),('PauseMenuUpdate',0x800821D4,0),
    ('StageRequestUpdate',0x8005D890,0),('MenuWindowsRun',0x8007D530,0),
    ('MessageBoxUpdateDraw',0x8008DF4C,0),('AreaFeature30Update',0x8009B7FC,0),
    ('StageFadeUpdate',0x80079B10,0),('DrawTextSprites',0x80082288,4),
    ('CameraUpdate',0x80047BD0,0),('SetColorMatrixMode',0x80015D58,4),
    ('PlayerAnimateAndDraw',0x80063628,0),('PlayerUpdate',0x80060780,0),
    ('SpecialActorPlayerNear',0x8009B6C8,0),('RcCarUpdate',0x800994A8,0),
    ('RcCarDraw',0x80099AE0,0),('ProjectileSystemsUpdate',0x80084AA0,0),
    ('ZakoUpdateAll',0x80050C34,0),('MonkeyUpdateAll',0x80042E18,0),
    ('AreaOverlayUpdate',0x8005ACE8,0),('DrawTerrain',0x80043A8C,0),
    ('ItemsUpdateDraw',0x80084598,0),('IndicatorsUpdateDraw',0x800719F8,0),
    ('DrawAreaProps',0x80016F34,0),('EffectScriptUpdate',0x80069BEC,0),
    ('EffectsUpdateAll',0x8006A9F8,0),('HudPopupUpdate',0x800983E8,0),
    ('ColliderScriptsUpdate',0x80088AB8,0),('GimmickUpdateAll',0x8005B68C,0),
    ('PlayerEyeBlinkUpdate',0x80067AF0,0),('CollisionUpdate',0x8007247C,0),
    ('SoundEmitterUpdate',0x8008686C,0),('StageDrawScreenOverlay',0x8005E0F0,0),
    ('VramScrollUpdate',0x800691F8,0),('PlayerCanExitStage',0x8005DC78,0),
    ('StageExitPending',0x8004D104,0),('SoundLoopsPause',0x8007292C,0),
    ('OpenPauseMenu',0x8007F56C,1),('PlaySfx',0x800877F8,1),
    ('IsPauseMenuOpen',0x8007F5D4,0),('QueueFrameFlip',0x8005DEE0,4),
    ('WaitFrameFlip',0x8005DE8C,0),('SoundStop',0x80086FD0,1),
    ('GadgetSlotsRestore',0x8007CA10,0),('ProgressMarkStageResult',0x800963DC,2),
    ('BgmUpdateVolume',0x8008745C,0),('RecordBestTime',0x800961D0,0),
    ('SaveBufferFill',0x80095B3C,0),('IsTimeAttack',0x80097678,0),
    ('SaveCaptureState',0x80095CEC,0),('SaveHeaderFill',0x80095D44,0),
    ('UnlockSpecialArea',0x80097820,0),('ExitSpecialArea',0x800978FC,0),
    ('GetLevelFirstArea',0x8009764C,1),('SoundBankUpload',0x80086CAC,1),
    ('SpecialActorUpdate',0x801ED100,1),('SpecialActorUpdate',0x801ED104,1),
    ('ModeEntry',0x801ED108,0),
]
CALL = C.CFUNCTYPE(C.c_int32,C.c_void_p,C.c_void_p,C.c_char_p,C.c_uint32,
                  C.c_uint32,C.POINTER(C.c_uint32))
class Hooks(C.Structure):
    _fields_ = [('user',C.c_void_p),('original_call',CALL),('stack_pointer',C.c_uint32)]

def build(source=None):
    output=ROOT/'extract/mips'/f'modes_{time.time_ns()}.dll'
    output.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-shared','-static-libgcc','-I'+str(ROOT/'src/game'),
        str(source or ROOT/'src/game/ae_modes.c'),str(ROOT/'src/core/ae_ram.c'),
        '-o',str(output)],check=True)
    return output

class Harness:
    def __init__(self,source=None):
        self.lib=C.CDLL(str(build(source)))
        self.lib.ae_ram_create.restype=C.c_void_p
        self.ram=self.lib.ae_ram_create()
        self.lib.ae_ram_error.argtypes=[C.c_void_p];self.lib.ae_ram_error.restype=C.c_char_p
        self.lib.ae_ram_clear_error.argtypes=[C.c_void_p]
        self.main=(C.c_ubyte*mr.RAM_SIZE).from_address(self.ram)
        self.scratch=(C.c_ubyte*mr.SCRATCH_SIZE).from_address(self.ram+mr.RAM_SIZE)
        self.cpu=mr.MipsRam.from_extract()
        self.cpu.load_area((ROOT/f'extract/members/{dump_areas.area_members()[1]:03X}.bin').read_bytes())
        self.base=self.cpu.snapshot()
        self.spec={address:(name,n) for name,address,n in HOOKS}
        self.coverage=set();self.counts=collections.Counter();self.callback_error=None
        self.callback=CALL(self.invoke)
        self.hooks=Hooks(None,self.callback,mr.DEFAULT_SP)
        self.functions={}
        for name in FUNCS:
            fn=getattr(self.lib,'ae_'+name)
            args=[C.c_void_p]
            if name in {'main','Mode_Stage','Mode_06','StageInitMemory','StageRenderInit'}:
                args.append(C.POINTER(Hooks))
            if name in {'StageInitMemory','BumpAllocPeek'}:args.append(C.c_uint32)
            fn.argtypes=args;fn.restype=C.c_uint32
            self.functions[name]=fn
        reg.declare('stack_window','801FF000..80200000')

    def read(self,side,address,size):
        return self.cpu.read_bytes(address,size) if side=='m' else C.string_at(self.ram+(address&0x1FFFFFFF),size)

    def write(self,side,address,data):
        if side=='m':self.cpu.write_bytes(address,data)
        else:C.memmove(self.ram+(address&0x1FFFFFFF),data,len(data))

    def effect(self,side,address,args):
        name,n=self.spec[address]
        args=tuple(args[:n]);log=self.logs[side];seen=self.seen[side]
        seen[name]+=1
        local=self.read(side,args[0],8) if name=='ClearImage' else None
        log.append((name,address,args,local))
        # Ordered, argument-sensitive side effects make otherwise unused calls observable.
        marker=struct.unpack('<I',self.read(side,0x801ED000,4))[0]
        self.write(side,0x801ED000,struct.pack('<I',(marker*33+address+sum(args))&0xFFFFFFFF))
        p=self.profile
        if name=='ClearWords':self.write(side,args[0],bytes(args[1]*4))
        if name=='StageRequest':self.write(side,0x800F447C,struct.pack('<H',p.get('request',7)))
        if name=='StageRequestUpdate' and (p.get('transition') or p.get('request')==6) and seen[name]>=2:
            self.write(side,0x800F447C,struct.pack('<H',7 if p.get('request')==6 else p.get('request',7)))
        if name=='PadUpdate' and p.get('transition') and seen[name]==1:
            self.write(side,0x800F447C,struct.pack('<H',6))
        if name=='ModeEntry':
            if seen[name]<p.get('dispatches',1):
                self.write(side,0x800F4470,struct.pack('<H',p.get('next_mode',13)))
                return 0
            return -1
        if name=='LoadListStep':return int(seen[name]%3==0)
        if name=='DebugMenuUpdate':return p.get('selection',999) if seen[name]>p.get('debug_frames',0) else -1
        if name=='StageFadeUpdate':return -1 if seen[name]>p.get('frames',0) else 0
        if name=='PauseMenuUpdate':return p.get('pause',0)
        if name=='PlayerCanExitStage':return p.get('can_exit',1)
        if name=='StageExitPending':return p.get('exit_pending',0)
        if name=='IsPauseMenuOpen':return p.get('menu_open',0)
        if name=='IsTimeAttack':return p.get('time_attack',0)
        if name=='GetLevelFirstArea':return p.get('first_area',19)
        if name=='VSync':return p.get('vblank',0x2222)
        return 0

    def invoke(self,user,ram,name,address,count,words):
        try:
            expected,n=self.spec[address]
            assert name.decode()==expected,(name,expected,hex(address))
            assert count==n,(expected,count,n)
            return self.effect('c',address,tuple(words[i] for i in range(count)))
        except Exception as exc:
            self.callback_error=exc
            return 0

    def run(self,name,case,profile=None,arg=0):
        self.cpu.restore(self.base);self.profile=profile or {};p=self.profile
        rng=random.Random(case+0xAEE2026)
        self.cpu.scratch[:]=rng.randbytes(mr.SCRATCH_SIZE)
        self.cpu.write_bytes(0x800B97C0,rng.randbytes(20))
        # Real EXE/area data plus random preexisting state, including clear ranges.
        self.cpu.write_bytes(0x800E5500,rng.randbytes(0x2400))
        self.cpu.write_bytes(0x800EC1E0,rng.randbytes(0x14C))
        self.cpu.write_bytes(0x800F4350,rng.randbytes(0x300))
        for offset,value in [(0x120,p.get('mode',11)),(0x122,p.get('area_kind',3)),
            (0x124,p.get('level',1)),(0x126,p.get('area',1)),(0x12C,p.get('request',7)),
            (0x134,p.get('flip',0)),(0x136,p.get('pending',0)),
            (0x150,p.get('player_mode',0)),(0x152,p.get('lock',0))]:
            self.cpu.write16(0x800F4350+offset,value)
        self.cpu.write32(0x800F4450,p.get('flags',0))
        self.cpu.write32(0x800F4600,p.get('last_vblank',0))
        self.cpu.write32(0x800EC350,15)
        self.cpu.write32(0x800EC358,0x801EC000)
        self.cpu.write32(0x800EC35C,p.get('buffer',0))
        self.cpu.write32(0x800EC230,p.get('player_state',0))
        self.cpu.write32(0x800F5120,p.get('special',0))
        self.cpu.write32(0x800F5A90,p.get('car',0))
        self.cpu.write32(0x800E5478,p.get('buttons',0))
        self.cpu.write32(0x800E5480,0xFFFFFFFF)
        self.cpu.write32(0x800A8A58,p.get('debug',1))
        self.cpu.write32(0x800B4DCC,0x801ED100);self.cpu.write32(0x800B4DD0,0x801ED104)
        self.cpu.write32(0x800B3618,case%4);self.cpu.write32(0x800B361C,case%3)
        if name=='StageInitMemory' or name=='Mode_06':
            for index in range(110):self.cpu.write32(0x800A81B0+index*8,0x801EE000+index*32)
        if name=='main':
            for index in range(40):
                self.cpu.write32(0x800AFA30+index*8,0x801ED108)
                self.cpu.write32(0x800AFA34+index*8,index if case%2 else 0xFFFFFFFF)
        self.logs={'m':[],'c':[]};self.seen={'m':collections.Counter(),'c':collections.Counter()}
        C.memmove(self.main,bytes(self.cpu.ram),mr.RAM_SIZE)
        C.memmove(self.scratch,bytes(self.cpu.scratch),mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram);self.callback_error=None
        self.cpu.coverage=self.coverage
        hooks={address:(lambda cpu,regs,a=address:self.effect('m',a,regs[4:8])) for address in self.spec}
        args=(arg,) if name in {'StageInitMemory','BumpAllocPeek'} else ()
        mret=self.cpu.call(FUNCS[name][0],args,hooks=hooks)[2]
        native=[self.ram]
        if name in {'main','Mode_Stage','Mode_06','StageInitMemory','StageRenderInit'}:native.append(C.byref(self.hooks))
        native.extend(args)
        cret=self.functions[name](*native)
        assert not self.callback_error,(name,case,self.callback_error)
        assert not self.lib.ae_ram_error(self.ram),(name,case,self.lib.ae_ram_error(self.ram))
        if name not in {'NullInit','ResetFrameFlip'}:assert mret==cret,(name,case,'return',hex(mret),hex(cret))
        assert self.logs['m']==self.logs['c'],(name,case,'calls',self.logs)
        end=mr.STACK_WINDOW[0]-0x80000000
        cmem=bytes(self.main)
        if self.cpu.ram[:end]!=cmem[:end]:
            off=next(i for i in range(end) if self.cpu.ram[i]!=cmem[i])
            raise AssertionError((name,case,'RAM',hex(0x80000000+off),self.cpu.ram[off:off+16],cmem[off:off+16]))
        assert bytes(self.cpu.scratch)==bytes(self.scratch),(name,case,'scratchpad')
        self.counts[name]+=1

def suite(h):
    for name in ('NullInit','Mode_04','ResetFrameFlip','StageRenderInit'):
        for case in range(32):h.run(name,case,dict(buffer=case%2))
    sizes=[0,1,2,3,4,0x7800,0x7FFFFFFD,0x7FFFFFFF,0x80000000,0xFFFFFFFD,0xFFFFFFFF]
    for case in range(160):h.run('BumpAllocPeek',case,arg=sizes[case] if case<len(sizes) else random.Random(case).getrandbits(32))
    for case in range(64):h.run('StageInitMemory',case,arg=case)
    for case in range(80):
        h.run('main',case,dict(dispatches=2 if case%3 else 1,next_mode=13 if case%4 else 4,request=[21,30,7][case%3]))
    for case in range(80):h.run('Mode_06',case,dict(selection=[0,1,2,3,4,999,5][case%7],debug_frames=case%3))
    for case,(request,mode,area_kind) in enumerate(itertools.product(range(36),(11,13,19,24),(0,3))):
        h.run('Mode_Stage',case,dict(request=request,mode=mode,area_kind=area_kind,
            frames=case%3,flags=case%2,player_mode=[0,1,3,8,13][case%5],
            special=[0,4,8,12][case%4],car=case%2,buttons=[0,0x800,0x100][case%3],
            level=[1,7,14,24,29,30][case%6],area=[1,19,36,92,98,99][case%6],
            lock=11 if case%11==0 else 0,player_state=0x80000000 if case%13==0 else 0,
            can_exit=case%2,exit_pending=case%3==0,menu_open=case%2,
            time_attack=case%2,pause=case%5,transition=case%7==0,
            pending=case%2,flip=case%2,buffer=case%2,
            first_area=[19,36,1][case%3],last_vblank=[0,0x2222,0x2223][case%3]))
    h.run('Mode_Stage',300,dict(request=8,mode=19,area_kind=0,time_attack=1,frames=2,car=1))
    h.run('Mode_Stage',301,dict(request=8,mode=19,area_kind=3,time_attack=1))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--mutate',action='store_true')
    args=parser.parse_args();h=Harness();suite(h)
    summary=reg.coverage_summary([(name,address,size,True) for name,(address,size) in FUNCS.items()],h.coverage)
    reg.print_coverage(summary)
    for name in FUNCS:print(f'{name}: {h.counts[name]} cases')
    mutations=[]
    if args.mutate:
        source=(ROOT/'src/game/ae_modes.c').read_text()
        for label,old,new in [
            ('null init side effect','(void)r; }','ae_wr32(r, FLIP_STATE, 1); }'),
            ('peek rounding','size + 3u','size + 2u'),
            ('clear monkey count','slot < 8u','slot < 7u'),
            ('render buffer reset','RENDER_BUFFER_INDEX, 0','RENDER_BUFFER_INDEX, 1'),
            ('mode04 return','(void)r; return 0;','(void)r; return 1;'),
            ('main mode13 area','? 91u : 88u','? 90u : 88u'),
            ('debug launch mode','CURRENT_MODE, 36','CURRENT_MODE, 35'),
            ('stage rng','* 5u + 1u','* 5u + 2u'),
            ('exit result','CURRENT_MODE, 12','CURRENT_MODE, 14'),
            ('flip flags','FLIP_STATE + 2u, 0','FLIP_STATE + 2u, 1'),
        ]:
            assert old in source,label
            mutant=ROOT/'extract/mips'/f'modes_mutant_{len(mutations)}.c'
            mutant.write_text(source.replace(old,new,1))
            test=Harness(mutant)
            try:suite(test)
            except AssertionError:mutations.append(label);print('caught:',label)
            else:raise AssertionError('mutation survived: '+label)
    if not args.mutate:reg.write_results(h.counts,summary,True)
    return 0

if __name__=='__main__':sys.exit(main())
