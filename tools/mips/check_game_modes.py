"""Compare original main through boot, title and the next mode boundary.

Title sequences 0/6 are explicit archive-fixture selections; sequence 2 keeps
boot's selection. Gameplay instructions execute on the oracle. The XaPlay
entry observer reads unwritten caller bytes, then resumes every instruction.
Test-only renderer fixtures execute original packet builders from semantic
requests, restoring scanline gameplay state so the C logic remains checked.
Only the existing call-stack window is omitted from RAM comparisons.
"""
import argparse
import ctypes as C
import csv
import hashlib
import json
from types import SimpleNamespace
import check_game_area as area
import check_ovl180 as title
import check_ovl181 as boot
import check_ovl188 as start
from check_ovl186d import EXPECTED_APIS as CARD_APIS
import mips_ram as mr
from generate_game_hooks import MODE_NATIVE, MODE_OVERLAY_NATIVE, MODE_RENDER, ADDRESSES, ARCHIVE_ARITY


class OverlaySwitch(Exception):
    pass


class Complete(Exception):
    pass


class CallerInput(Exception):
    pass


class Harness(area.Harness):
    def __init__(self, overrides=None):
        super().__init__(overrides)
        self.raw_trace=False
        self.raw_result=0
        self.rectangle_probe=False
        self.rendering=False
        self.lib.integration_run.argtypes = [C.c_void_p]
        self.lib.integration_run.restype = C.c_int32
        self.lib.integration_stop.argtypes = [C.c_void_p]
        self.lib.integration_bounded_call.argtypes = [C.c_void_p,C.c_uint32]
        self.lib.integration_set_xa_padding.argtypes = [C.c_void_p,C.POINTER(C.c_uint32),C.c_uint32]
        self.lib.integration_mode_words.argtypes=[C.c_void_p,C.c_char_p,C.c_uint32,C.c_uint32,C.POINTER(C.c_uint32)]
        self.lib.integration_mode_words.restype=C.c_int32
        self.lib.integration_missing_count.argtypes=[C.c_void_p]
        self.lib.integration_missing_count.restype=C.c_size_t
        self.lib.integration_missing_name.argtypes=[C.c_void_p,C.c_size_t]
        self.lib.integration_missing_name.restype=C.c_char_p
        self.lib.integration_missing_address.argtypes=[C.c_void_p,C.c_size_t]
        self.lib.integration_missing_address.restype=C.c_uint32
        self.lib.ae_ram_fail.argtypes = [C.c_void_p, C.c_char_p]
        self.spec.update(boot.parse_hook_sites())
        self.spec.update(title.API)
        self.spec.update({a:(n,c) for a,(n,c,_) in start.HOOKS.items()})
        self.spec.update({a:(n,c) for n,(a,c) in MODE_NATIVE.items()})
        self.spec.update({a:(n,c) for n,(_,a,c,_) in MODE_OVERLAY_NATIVE.items()})
        self.spec.update({ADDRESSES[n]:(n,c) for n,c in ARCHIVE_ARITY.items() if n in ADDRESSES})
        self.spec[0x80012CA0]=('PadSetupDualShock',1)
        self.spec.update({0x8005CD50:('InitSystem',0),0x8002EDF0:('SetVideoMode',1)})
        self.spec[0x800305B0]=('ScaleMatrixL',2)
        self.spec.pop(0x80030450,None)  # exact ApplyMatrixLV runs on the oracle
        # Verified EXE ports execute normally except the native archive queue.
        # Like the stage host, mode callers (including XaPlay) use that boundary
        # consistently; the standalone player1 checker proves the queue C port.
        for address in list(self.spec):
            if address in self.ported and address < 0x80100000 and address != 0x80012360:
                del self.spec[address]
        self.byname={n:(a,c) for a,(n,c) in self.spec.items()}

    def callback(self,*args):
        result=super().callback(*args)
        if self.callback_error: self.lib.integration_stop(self.shim)
        return result

    def model(self,name,address,args,mips):
        if name == 'ModeContinue':
            # Observe the entry through the host's existing continuation service.
            # The oracle observes the same entry before its first instruction.
            if self.sequence==6 and self.members[0][-1:]==[0x186]:
                return self.model('Mode_SaveMenu',0x80136AB8,(),False)
            return 1
        if self.raw_trace:
            assert len(self.clog) < 20_000, 'mode guard callback budget exhausted'
            self.clog.append((name,address,tuple(args),None))
            return self.raw_result
        if self.rectangle_probe:
            cpu=self.cpu if mips else self.native_cpu
            if not mips: cpu.ram[:]=memoryview(self.main)
            if name=='ClearImage':
                (self.mlog if mips else self.clog).append(cpu.read_bytes(args[0],8))
                if mips: raise Complete()
                self.lib.integration_stop(self.shim)
            return 0
        side=int(mips)
        if not address and name in self.byname: address=self.byname[name][0]
        cpu=self.cpu if mips else self.native_cpu
        if not mips and not self.rendering:
            cpu.ram[:]=memoryview(self.main)
            cpu.scratch[:]=memoryview(self.scratch)
        renderer_roots={'StageStartUi_SetDrawStp':(0x80139710,()),
                        'StageStartUi_WipeQuadA':(0x80139FB8,tuple(args[:2])),
                        'StageStartUi_WipeQuadB':(0x8013A10C,tuple(args[:2])),
                        'StageStartUi_DrawWipeBackground':(0x80137538,()),
                        'StageStartUi_DrawScanlines':(0x8013A3C4,())}
        if not mips and self.members[0][-1:]==[0x188] and name in renderer_roots:
            # Test-only native renderer fixture: execute only the original
            # packet builder, leaving all overlay gameplay in the C host.
            root,words=renderer_roots[name]
            gameplay=None
            if name=='StageStartUi_DrawScanlines':
                gameplay=(cpu.read32(0x8013B6E4),cpu.read16(0x800AE8F0))
                reset=gameplay[0]==args[2]
                cpu.write32(0x8013B6E4,0 if reset else args[2])
                seed=args[3]&65535
                cpu.write16(0x800AE8F0,((seed-1)*52429)&65535 if reset else seed)
            elif name=='StageStartUi_DrawWipeBackground': assert args[2]==cpu.read32(0x800EC354)
            else: assert args[1 if name=='StageStartUi_SetDrawStp' else 4]==cpu.read32(0x800EC354)
            if name in {'StageStartUi_SetDrawStp','StageStartUi_WipeQuadA','StageStartUi_WipeQuadB'}:
                assert args[0 if name=='StageStartUi_SetDrawStp' else 3]==cpu.read32(0x800EC35C),'renderer frame index'
            if name in {'StageStartUi_WipeQuadA','StageStartUi_WipeQuadB'}:
                assert args[2]==cpu.read16(0x8013B9C4),'renderer texture page'
            self.rendering=True
            try:
                hooks={a:(lambda c,regs,a=a: self.model(self.spec[a][0],a,tuple(regs[4+i] for i in range(self.spec[a][1])),False)) for a in self.spec if a<0x80100000}
                kw={}
                if name=='StageStartUi_DrawWipeBackground':
                    kw['initial_regs']={16:0x800EC350,3:0x00F00180}
                    hooks[0x8013A3C4]=lambda c,regs:0
                    def finish(c,regs): raise Complete()
                    hooks[0x8013759C]=finish
                try: cpu._reference_call(root,args=words,hooks=hooks,limit=10_000,**kw)
                except Complete: pass
            finally: self.rendering=False
            if gameplay:
                cpu.write32(0x8013B6E4,gameplay[0]);cpu.write16(0x800AE8F0,gameplay[1])
            C.memmove(self.main,bytes(cpu.ram),mr.RAM_SIZE)
            C.memmove(self.scratch,bytes(cpu.scratch),mr.SCRATCH_SIZE)
            return 0
        if address in self.spec: name,count=self.spec[address]; args=tuple(args[:count])
        else: args=tuple(args)
        if name=='SeqTrackClose' and self.members[side][-1:]!=[0x188]: args=args[:1]
        if name=='BeginFrame':
            self.frame_modes[side].append(cpu.read16(0x800F4470))
            self.frame_hashes[side].append(hashlib.sha256(cpu.ram[:0x1FF000]+cpu.scratch).hexdigest())
        if name=='ArcLoadEntry' and self.members[side][-1:]==[0x188]:
            start.load_arc_entries_mips(cpu,args[0])
        if name == 'ArcLoadMember':
            member,destination=args
            data=(area.ROOT/f'extract/members/{member:03X}.bin').read_bytes()
            cpu.write_bytes(destination,data)
            cpu._decoded.clear()
            self.members[side].append(member)
            if member==0x180:
                stack=cpu.read_bytes(0x801FF000,0x1000)
                title.Harness.setup(SimpleNamespace(),cpu,'Mode_Title',0)
                cpu.write_bytes(0x801FF000,stack)
                # Sequence 2 preserves boot's actual transition; other
                # fixtures select a title branch at the archive boundary.
                cpu.write16(0x800F44AA,self.sequence)
                cpu.write16(title.WORK+0x1A00,2)
                cpu.write8(title.WORK+0x1A02,0)
                cpu.write16(title.WORK+0x1A04,0)
        log=self.mlog if mips else self.clog
        payload=cpu.read_bytes(args[0],8) if name in {'ClearImage','ClearImage2'} else None
        if name=='CdMgrQueueCommand' and args[1]:
            # The original queue copies the full parameter word at 80012468.
            # Compare it at the native boundary, before its stack storage dies.
            payload=cpu.read_bytes(args[1],4)
        log.append((name,address,tuple(area.norm(a) for a in args),payload))
        if len(log)>20_000 or self.frames[side]>1_200:
            if mips: raise AssertionError('mode fixture exceeded native-call/frame budget')
            self.callback_error='mode fixture exceeded native-call/frame budget'
            self.lib.integration_stop(self.shim)
            return 0
        baseline_stop=(self.sequence==2 and name=='CdMgrUpdate' and self.members[side]==[0x181,0x180] and cpu.read16(0x800F4470)!=26)
        missing_stop=(self.sequence!=2 and self.members[side][-1:]==[0x188] and name in {'LevelCaughtCount','LevelGetInfo','LevelTableGet','LevelProgressGet','AreaKindVisitedSet'}) or name=='Mode_SaveMenu'
        if baseline_stop or missing_stop:
            self.stopped[side]=True
            self.stop_reason[side]=(name,f'{address:08X}')
            if mips: raise Complete()
            C.memmove(self.main,bytes(cpu.ram),mr.RAM_SIZE)
            C.memmove(self.scratch,bytes(cpu.scratch),mr.SCRATCH_SIZE)
            self.lib.integration_stop(self.shim)
            return 0
        self.models[side].mem=title.MipsMem(cpu)
        aliases={'LoadListInit':'LoadListStart'}
        if name=='VSync':
            self.frames[side]+=1
            # Script a connected neutral pad, then a Start press.
            cpu.write8(0x800E52D0,0);cpu.write8(0x800E52D1,0x73)
            cpu.write8(0x800E52D2,0xF7 if self.frames[side]%17==0 else 0xFF)
            cpu.write8(0x800E52D3,0xFF)
            result=self.frames[side]
        elif name=='LoadListStep': result=-1
        elif name=='CdPlayXa': result=1
        elif name=='PadGetState': result=6
        elif name=='PadInfoMode': result=7
        elif name=='CdIntToPos':
            cpu.write32(args[1],args[0]^0x5A37C91D)
            result=0
        else: result=self.models[side].call(aliases.get(name,name),args)
        if not mips:
            C.memmove(self.main,bytes(cpu.ram),mr.RAM_SIZE)
            C.memmove(self.scratch,bytes(cpu.scratch),mr.SCRATCH_SIZE)
        if mips and name=='ArcLoadMember' and args[1]==0x80136000:
            raise OverlaySwitch()
        return result

    def run(self, sequence=2):
        self.sequence=sequence
        self.stop_reason=[None,None]
        self.xa_inputs=[]
        capture_xa=True
        self.cpu.restore(self.base)
        self.cpu.write32(0x800E38B0,1)
        self.cpu.write32(0x800E38E4,0x80100000)
        self.cpu.write8(0x800E52D0,0); self.cpu.write8(0x800E52D1,0x73)
        self.cpu.write32(0x800F444C,0x801E8000)
        self.native_cpu=mr.MipsRam.from_extract()
        self.sync()
        self.models=[title.CallModel({'stream_wait_results':(0,0,1)}) for _ in range(2)]
        self.frames=[0,0];self.members=[[],[]];self.stopped=[False,False]
        self.frame_hashes=[[],[]]
        self.frame_modes=[[],[]]
        self.cpu.coverage=set()
        resume=None
        budget=10_000_000
        start_steps=self.cpu.steps
        while True:
            member=self.members[1][-1] if self.members[1] else 0
            overlay_addresses={v[1] for v in MODE_OVERLAY_NATIVE.values() if v[0]==member}
            hooks={a:h for a,h in self.hooks(0x8005BDC0).items() if a<0x80100000 or a in overlay_addresses}
            if member==0x186:
                hooks[0x80136AB8]=lambda cpu,regs:self.model('Mode_SaveMenu',0x80136AB8,(),True)
            if member==0x188 and capture_xa:
                def capture(cpu,regs):
                    frame=regs[29]-64
                    self.xa_inputs.append((cpu.read32(frame+16)&0xFFFF0000,cpu.read32(frame+32)&0xFFFFFF00))
                    raise CallerInput()
                hooks[0x80011F6C]=capture
            try:
                kwargs={'_resume':resume} if resume else {}
                self.cpu._reference_call(0x8005BDC0,hooks=hooks,limit=budget,**kwargs)
                break
            except OverlaySwitch:
                regs,hi,lo=self.cpu._last_state
                regs[2]=0
                budget=10_000_000-(self.cpu.steps-start_steps)
                assert budget>0,'mode integration instruction budget exhausted'
                resume=(regs,regs[31],None,None,hi,lo,budget)
            except Complete: break
            except CallerInput:
                capture_xa=False
                regs,hi,lo=self.cpu._last_state
                budget=10_000_000-(self.cpu.steps-start_steps)
                assert budget>0
                resume=(regs,0x80011F6C,None,None,hi,lo,budget)
            except Exception:
                print('oracle',self.members,self.frames,self.mlog[-15:],flush=True)
                raise
        assert self.stopped[1], 'original did not finish boot and title'
        if self.xa_inputs:
            area.reg.declare('shared_random','XaPlay byte-local caller padding',checker='check_game_modes')
        words=(C.c_uint32*(len(self.xa_inputs)*2))(*(v for pair in self.xa_inputs for v in pair))
        self.lib.integration_set_xa_padding(self.shim,words,len(self.xa_inputs))
        self.lib.integration_run(self.shim)
        assert self.stopped[0], self.callback_error or (self.lib.ae_ram_error(self.ram) or b'host did not finish boot and title').decode()
        assert self.callback_error is None,self.callback_error
        self.lib.ae_ram_clear_error(self.ram)
        assert self.clog==self.mlog, next(((i,a,b) for i,(a,b) in enumerate(zip(self.clog,self.mlog)) if a!=b),(len(self.clog),len(self.mlog)))
        lo=0x1FF000
        a,b=bytes(self.cpu.ram[:lo]),bytes(memoryview(self.main)[:lo])
        if a!=b:
            diffs=[(f'{0x80000000+i:08X}',x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y]
            (area.ROOT/'extract/codex-task/modes-ram-diffs.json').write_text(json.dumps(diffs,indent=2))
            print('RAM differences',len(diffs),diffs[:30],flush=True)
        assert a==b,next((f'RAM {0x80000000+i:08X}: original {x:02X}, host {y:02X}' for i,(x,y) in enumerate(zip(a,b)) if x!=y),'')
        assert bytes(self.cpu.scratch)==bytes(self.scratch),'scratchpad differs'
        assert self.members[0]==self.members[1],self.members
        assert self.stop_reason[0]==self.stop_reason[1],self.stop_reason
        assert self.frame_hashes[0]==self.frame_hashes[1], 'frame RAM differs'
        assert {0x8005BDC0,0x80136000,0x80137AC0,0x80136074,0x8001325C} <= self.cpu.coverage
        area.reg.declare('stack_window','801FF000..80200000',checker='check_game')
        missing=[dict(name=self.lib.integration_missing_name(self.shim,i).decode(),address=f'{self.lib.integration_missing_address(self.shim,i):08X}') for i in range(self.lib.integration_missing_count(self.shim))]
        report=dict(sequence=sequence,stop=self.stop_reason[0],missing=missing,calls=len(self.clog),
                    vsyncs=self.frames[0],frames=len(self.frame_hashes[0]),members=self.members[0],
                    selected_area=self.cpu.read16(0x800F4476),
                    stage_frame_boundaries=sum(mode in (11,13) for mode in self.frame_modes[0]),
                    next_mode=self.cpu.read16(0x800F4470),instructions=self.cpu.steps,failures=0)
        (area.ROOT/f'extract/mips/game_modes_{sequence}_results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(f'title sequence {sequence}: members {[f"{m:03X}" for m in self.members[0]]}; '
              f'{len(self.frame_hashes[0])} frame boundaries / {self.frames[0]} VSyncs; '
              f'stop {self.stop_reason[0]}; all non-stack RAM and scratchpad equal')
        return report


    def guard_checks(self):
        self.raw_trace=True
        self.raw_result=0x12345678
        for name,address in CARD_APIS:
            words=(C.c_uint32*7)(*[0xA1000000+i for i in range(7)])
            self.lib.integration_set_loaded_overlay(self.shim,0x186)
            before=self.lib.integration_missing_count(self.shim)
            result=self.lib.integration_mode_words(self.shim,name.encode(),address,7,words)
            assert result==self.raw_result,name
            assert self.clog[-1][:3]==(name,address,tuple(words)),name
            assert self.lib.integration_missing_count(self.shim)==before,name
            self.lib.integration_set_loaded_overlay(self.shim,0x188)
            self.lib.integration_mode_words(self.shim,name.encode(),address,7,words)
            assert self.lib.integration_missing_count(self.shim)>before,name
        print(f'MCX native routes: {len(CARD_APIS)} seven-word callbacks and member guards')
        self.raw_result=0
        words=(C.c_uint32*1)(55)
        before=self.lib.integration_missing_count(self.shim)
        self.lib.integration_mode_words(self.shim,b'Exit',0xB0,1,words)
        assert self.lib.integration_missing_count(self.shim)==before,'BIOS Exit was missing'
        assert self.clog[-1][:3]==('Exit',0x800A3700,(55,)),self.clog[-1]
        self.lib.integration_mode_words(self.shim,b'OtherBiosCall',0xB0,1,words)
        assert self.lib.integration_missing_count(self.shim)>before,'unidentified BIOS call accepted as Exit'
        for name,(address,count) in dict(MODE_RENDER,MemCardOp=(0x80010000,2)).items():
            words=(C.c_uint32*count)(*[0x801D0000+i*16 for i in range(count)])
            before=self.lib.integration_missing_count(self.shim)
            self.lib.integration_mode_words(self.shim,name.encode(),address,count,words)
            assert self.clog[-1][:3]==(name,address,tuple(words)),name
            assert self.lib.integration_missing_count(self.shim)==before,name
        self.lib.integration_set_loaded_overlay(self.shim,0)
        with (area.ROOT/'tools/verified.csv').open(newline='') as stream:
            rows=[r for r in csv.DictReader(stream) if r['status']=='diffed' and r['source'].startswith('src/modes/')]
        for row in rows:
            before=len(self.clog)
            self.lib.integration_call(self.shim,int(row['address'],16),0,0,0,0)
            assert len(self.clog)==before+1 and self.clog[-1][1]==int(row['address'],16),row['name']
        # Overlay-local archive leaves also retain their member identity.
        for name,(member,address,count,group) in MODE_OVERLAY_NATIVE.items():
            words=(C.c_uint32*7)(*([0x801D0000,0x801D0010]+[0]*5))
            self.lib.integration_set_loaded_overlay(self.shim,member)
            before=self.lib.integration_missing_count(self.shim)
            self.lib.integration_mode_words(self.shim,name.encode(),address,count,words)
            assert not self.lib.ae_ram_error(self.ram),(name,'native callback not wired')
            assert self.lib.integration_missing_count(self.shim)==before,(name,'native leaf routed as missing')
            self.lib.integration_set_loaded_overlay(self.shim,0)
            self.lib.integration_mode_words(self.shim,name.encode(),address,count,words)
            assert self.lib.integration_missing_count(self.shim)>before,(name,'unloaded native leaf was accepted')
        self.lib.integration_set_loaded_overlay(self.shim,0x180)
        # Five native words must survive the public mode adapter unchanged.
        words=(C.c_uint32*5)(0x801D0000,1,2,3,4)
        self.lib.integration_mode_words(self.shim,b'SetDefDrawEnv',0x8002B280,5,words)
        assert self.clog[-1][2]==tuple(words),self.clog[-1]
        renderer_arrays=self.lib.integration_renderer_words_calls(self.shim)
        self.lib.integration_mode_words(self.shim,b'CameraLookAt',0x80046670,5,words)
        assert self.clog[-1][2]==tuple(words),self.clog[-1]
        assert self.lib.integration_renderer_words_calls(self.shim)==renderer_arrays,'CameraLookAt lost its named renderer callback'
        # Only absent entries may be probed as missing: merged save-menu entry
        # runs a synchronous UI loop and is covered by its owning checker.
        save_merged=any(int(row['address'],16)==0x80136AB8 and
                        '186' in row['notes'] for row in rows)
        if not save_merged:
            self.lib.integration_set_loaded_overlay(self.shim,0x186)
            C.memmove(C.addressof(self.main)+0xF4470,b'\x23\x00',2)
            assert self.lib.integration_mode_words(self.shim,b'ModeEntry',0x80136AB8,0,None)==-1
            assert 'not ported' in self.lib.ae_ram_error(self.ram).decode()
            self.lib.ae_ram_clear_error(self.ram)
        # Title sequence 0 selects mode 8. Keep its missing-entry proof only
        # while that member is unported; merging its ledger rows needs no
        # dispatcher or checker source edit.
        stage_start_merged=any(int(row['address'],16)==0x8013683C and
                               '188.bin' in row['notes'] for row in rows)
        if not stage_start_merged:
            self.lib.integration_set_loaded_overlay(self.shim,0x188)
            C.memmove(C.addressof(self.main)+0xF4470,b'\x08\x00',2)
            assert self.lib.integration_mode_words(self.shim,b'ModeEntry',0x8013683C,0,None)==-1
            error=self.lib.ae_ram_error(self.ram).decode()
            assert 'mode 8 member 188 entry 8013683C is not ported' in error,error
            self.lib.ae_ram_clear_error(self.ram)
            print('Stage-start boundary:',error)
        print(f'Mode guards: {len(rows)} ported entries, {len(MODE_OVERLAY_NATIVE)} native leaves; native fifth word preserved')
        assert self.callback_error is None,self.callback_error
        self.raw_trace=False

    def rectangle_checks(self):
        self.rectangle_probe=True
        self.native_cpu=mr.MipsRam.from_extract()
        for root in (0x8013683C,0x80136A1C,0x801375B4):
            for template in (bytes.fromhex('0000000080010002'),bytes.fromhex('010002003003E001')):
                self.cpu.restore(self.base)
                self.cpu.write_bytes(0x80136000,start.OVERLAY.read_bytes())
                self.cpu.write_bytes(0x8013ADB0,template)
                self.cpu.write16(0x800F4470,10)
                self.cpu.write16(0x800F4476,6)
                self.cpu.write8(0x800DFDE3,0)
                self.cpu.write32(0x8013AE60,0x12345678)
                self.sync()
                self.lib.integration_set_loaded_overlay(self.shim,0x188)
                hooks={a:hook for a,hook in self.hooks(root).items() if a<0x80100000}
                self.cpu._decoded.clear()
                try: self.cpu._reference_call(root,hooks=hooks,limit=10_000)
                except Complete: pass
                else: raise AssertionError('original did not reach ClearImage')
                self.lib.integration_bounded_call(self.shim,root)
                assert self.callback_error is None,self.callback_error
                assert self.mlog==self.clog==[template],(hex(root),self.mlog,self.clog)
                assert self.cpu.ram[:0x1FF000]==bytes(memoryview(self.main)[:0x1FF000]),hex(root)
                assert bytes(self.cpu.scratch)==bytes(self.scratch),hex(root)
        self.rectangle_probe=False
        print('Stage-start rectangles: six original-instruction comparisons')

    def renderer_checks(self):
        self.sequence=2
        self.members=[[0x188],[0x188]]
        self.frames=[0,0]
        self.models=[title.CallModel({}) for _ in range(2)]
        self.native_cpu=mr.MipsRam.from_extract()
        for root in (0x80139710,0x80139FB8,0x8013A10C):
            for frame in (0,1):
                self.cpu.restore(self.base)
                self.cpu.write_bytes(0x80136000,start.OVERLAY.read_bytes())
                self.cpu._decoded.clear()
                self.cpu.write32(0xC,91)
                self.cpu.write32(0x800EC354,0x801E8000)
                self.cpu.write32(0x800EC35C,frame)
                self.cpu.write16(0x8013B9C0,1)
                self.cpu.write16(0x8013B9C4,0)
                self.sync()
                self.lib.integration_set_loaded_overlay(self.shim,0x188)
                hooks={a:hook for a,hook in self.hooks(root).items() if a<0x80100000}
                self.cpu._reference_call(root,args=(19,8),hooks=hooks,limit=10_000)
                self.lib.integration_call(self.shim,root,19,8,0,0)
                assert self.callback_error is None,self.callback_error
                assert self.clog==self.mlog,(hex(root),self.clog,self.mlog)
                assert self.cpu.ram[:0x1FF000]==bytes(memoryview(self.main)[:0x1FF000]),hex(root)
                assert bytes(self.cpu.scratch)==bytes(self.scratch),hex(root)
        print('Stage-start renderer requests: six full-RAM packet comparisons')

    def lookat_checks(self):
        self.sequence=2
        self.members=[[0x188],[0x188]]
        self.frames=[0,0]
        self.models=[title.CallModel({}) for _ in range(2)]
        self.native_cpu=mr.MipsRam.from_extract()
        for eye,target in [((37,-83,147),(56,11,-90)),((-191,215,50),(0,0,-40))]:
            self.cpu.restore(self.base)
            self.cpu.write_bytes(0x80136000,start.OVERLAY.read_bytes())
            self.cpu._decoded.clear()
            self.cpu.write_bytes(0x800AF0A0,bytes(32))
            for offset in (0,8,16):self.cpu.write16(0x800AF0A0+offset,4096)
            for base,vector in ((0x801D0000,eye),(0x801D0010,target)):
                for axis,value in enumerate(vector):self.cpu.write16(base+axis*2,value)
            self.sync()
            self.lib.integration_set_loaded_overlay(self.shim,0x188)
            hooks={a:hook for a,hook in self.hooks(0x80136000).items() if a<0x80100000}
            self.cpu._reference_call(0x80136000,args=(0x801D0000,0x801D0010),hooks=hooks,limit=20_000)
            self.lib.integration_call(self.shim,0x80136000,0x801D0000,0x801D0010,0,0)
            assert self.callback_error is None,self.callback_error
            assert self.cpu.ram[:0x1FF000]==bytes(memoryview(self.main)[:0x1FF000]),'LookAt RAM differs'
            assert bytes(self.cpu.scratch)==bytes(self.scratch),'LookAt scratchpad differs'
            assert self.clog==self.mlog,('LookAt calls',self.clog,self.mlog)
        print('Stage-start LookAt: two original SDK instruction comparisons')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--mutate',action='store_true')
    args=parser.parse_args()
    import sys
    sys.path.insert(0,str(area.ROOT/'tools/codex'))
    import gte_oracle
    oracle=gte_oracle.build()
    gte_oracle.build=lambda:oracle
    harness=Harness()
    reports=[harness.run()]
    harness.guard_checks()
    harness.rectangle_checks()
    harness.renderer_checks()
    harness.lookat_checks()
    reports.append(Harness().run(sequence=0))
    reports.append(Harness().run(sequence=6))
    (area.ROOT/'extract/mips/game_modes_results.json').write_text(json.dumps(dict(runs=reports,failures=0),indent=2)+'\n')
    if args.mutate:
        # Reproduce the pass-7/pass-9 merge: the host uses the native queue,
        # but the mode oracle mistakenly executes its standalone C counterpart.
        inconsistent=Harness()
        del inconsistent.spec[0x80012360]
        try: inconsistent.run(0)
        except AssertionError as error:
            assert 'CdMgrQueueCommand' in str(error) and 'CdIntToPos' in str(error),str(error)
            print('Inconsistent mode CD queue routing rejected',flush=True)
        else: raise AssertionError('inconsistent mode CD queue routing survived')
        source='src/modes/ae_ovl188.c'
        original=(area.ROOT/source).read_text()
        applied=('    (void)ae_sdk_apply_matrix_lv(ae_ptr(ram, O188_LOOK_AT_MATRIX, 32u),\n'
                 '        ae_ptr(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_TRANSLATION, 12u),\n'
                 '        ae_ptr(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_TRANSLATION, 12u));')
        assert applied in original
        path=area.ROOT/'extract/codex-task/old_stage_start_translation.c'
        path.write_text(original.replace(applied,
            '    (void)o188_call(ram, hooks, "MatrixBuild", 0x80030450u, 3u,\n'
            '        (const uint32_t[]){ O188_LOOK_AT_MATRIX, O188_LOOK_AT_TRANSLATION, O188_LOOK_AT_TRANSLATION });'))
        try: Harness({source:path}).lookat_checks()
        except AssertionError as error:
            assert 'LookAt RAM differs' in str(error),str(error)
            print('Old stage-start translation source rejected')
        else: raise AssertionError('old stage-start translation survived')
        for label,old,new in [
            ('renderer_frame','ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET)','ae_rd32(ram, O188_MODE_UI_TIMER)'),
            ('renderer_page','O188_MODE_UI_ANIMATION_FRAME_LIMIT),','O188_MODE_UI_ANIMATION_FRAME),')]:
            assert old in original
            path=area.ROOT/f'extract/codex-task/old_stage_start_{label}.c'
            path.write_text(original.replace(old,new))
            try: Harness({source:path}).renderer_checks()
            except AssertionError as error:
                assert 'renderer' in str(error),str(error)
                print('Old renderer request rejected:',label)
            else: raise AssertionError('old renderer request survived: '+label)
        for function,target in [('Mode_StageStart','rectangle'),('StageStartUi_RunA','temp_vector'),('StageStartUi_RunB','temp_vector')]:
            begin=original.index('int32_t ae_'+function+'(')
            end=original.index('\n}',begin)
            body=original[begin:end]
            copies=(f'    ae_wr32(ram, {target}, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE));\n'
                    f'    ae_wr32(ram, {target} + 4u, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE + 4u));\n')
            assert copies in body
            path=area.ROOT/f'extract/codex-task/old_{function}_rectangle.c'
            path.write_text(original[:begin]+body.replace(copies,f'    (void){target};\n')+original[end:])
            try: Harness({source:path}).rectangle_checks()
            except AssertionError: print('Old rectangle source rejected:',function)
            else: raise AssertionError('old stage-start rectangle survived: '+function)
        path=area.ROOT/'extract/codex-task/old_stage_start_wipe.c'
        begin=original.index('int32_t ae_Mode_StageStart(')
        text=original[:begin]+original[begin:].replace('    ae_wr32(ram, O188_MODE_WIPE_KIND, 0u);\n','',1)
        path.write_text(text)
        try: Harness({source:path}).rectangle_checks()
        except AssertionError: print('Old stage-start wipe initialization rejected')
        else: raise AssertionError('old stage-start wipe initialization survived')
        path=area.ROOT/'extract/codex-task/old_stage_start_mesh_mask.c'
        assert '& 0xFFFF1FFFu' in original
        path.write_text(original.replace('& 0xFFFF1FFFu','& 0x1FFFu'))
        try: Harness({source:path}).run(0)
        except AssertionError as error:
            assert 'RAM' in str(error),str(error)
            print('Old stage-start mesh mask rejected')
        else: raise AssertionError('old mesh mask survived')
        for label,old,new in [
            ('wipe_divisor','(frame * 3 * 64) / 5','(frame * 3 * 64) / 10'),
            ('xa_padding','? hooks->xa_padding(hooks->user, ram, 0u) : 0u','? 0u : 0u')]:
            assert old in original
            path=area.ROOT/f'extract/codex-task/old_stage_start_{label}.c'
            path.write_text(original.replace(old,new))
            try: Harness({source:path}).run(0)
            except AssertionError as error:
                assert ('CdMgrQueueCommand' if label=='xa_padding' else 'RAM') in str(error),str(error)
                print('Old stage-start source rejected:',label)
            else: raise AssertionError('old stage-start source survived: '+label)
        source='src/game/ae_game_stage.c'
        text=(area.ROOT/source).read_text()
        old='g->loaded_overlay = a[0];'
        assert old in text
        path=area.ROOT/'extract/codex-task/mode_member_mutation.c'
        path.write_text(text.replace(old,'g->loaded_overlay = 0;'))
        try: Harness({source:path}).run()
        except AssertionError: print('Mode overlay identity mutation caught')
        else: raise AssertionError('mode overlay identity mutation survived')
        source='src/modes/ae_ovl181.c'
        text=(area.ROOT/source).read_text()
        start=text.index('    CALL1(ram, hooks, "SoundStop", AE_ADDR_SOUND_STOP, 1u);')
        end=text.index('    CALL4(ram, hooks, "ClearImage2"',start)
        old=text[start:end]
        assert old in text
        path=area.ROOT/'extract/codex-task/old_boot_clear_height.c'
        path.write_text(text.replace(old,old.replace('640u, 512u','640u, 256u')))
        try: Harness({source:path}).run()
        except AssertionError as error:
            assert 'ClearImage2' in str(error),str(error)
            print('Old boot clear rectangle rejected')
        else: raise AssertionError('old boot clear rectangle survived')
        old=('    ae_wr32(ram, AE_OVERLAY_LOCAL_RECT, ae_rd32(ram, AE_MODE02_CLEAR_TEMPLATE));\n'
             '    ae_wr32(ram, AE_OVERLAY_LOCAL_RECT + 4u,\n'
             '            ae_rd32(ram, AE_MODE02_CLEAR_TEMPLATE + 4u));')
        assert old in text
        path=area.ROOT/'extract/codex-task/old_mode02_clear_rectangle.c'
        path.write_text(text.replace(old,'    ae_ovl181_rect(ram, 0u, 0u, 640u, 256u);'))
        test=boot.Harness(coverage=False,dll=boot.build_overlay_dll({source:path}))
        try: test.run_one('Mode_02',0)
        except AssertionError as error:
            assert 'ClearImage' in str(error),str(error)
            print('Old minigame clear rectangle rejected')
        else: raise AssertionError('old minigame clear rectangle survived')


if __name__=='__main__':
    main()
