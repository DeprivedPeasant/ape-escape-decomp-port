"""Differential integration checks for central calls, area loading and stage frames.

Reports failed/faulting cases rather than treating agreed faults as successful proofs.
The original Mode_Stage loop is bounded at its next PadUpdate for one frame.
"""
import argparse
import ast
import csv
import ctypes as C
import json
import random
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools/mips'), str(ROOT/'tools')]
import check_game as cg
import check_stage as stage
import check_areaset as areaset
import check_fxscript as fxscript
from types import SimpleNamespace
import dump_areas
import mips_ram as mr
import monkey_harness as mh
import exclusion_registry as reg
from check_hud import HudDraw, DrawFn
from generate_game_hooks import SDK_ARITY

TRACE = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_char_p, C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32))
P,U,I=C.c_void_p,C.c_uint32,C.c_int32
MASK=0xFFFFFFFF



def hook_inventory():
    """The generated hook inventory (extract/mips/game_hook_inventory.json), rebuilt from the headers when absent."""
    path = ROOT / 'extract/mips/game_hook_inventory.json'
    if path.exists():
        return json.loads(path.read_text())
    sys.path.insert(0, str(ROOT / 'tools'))
    import generate_game_hooks
    return generate_game_hooks.inventory()[0]

def norm(word):
    return 'stack' if 0x801FF000 <= word < 0x80200000 else word & MASK


def duplicate_overlay_fixture(source):
    """Two existing port adapters share an address only in this test table."""
    anchor='    {0x8013D81Cu, '
    assert anchor in source
    return source.replace(anchor,
        '    {0x8013D81Cu, "TestMember181", call_ae_ZakoAreaModeNoop, 0x00000181u},\n'+anchor,1)


class Harness:
    def __init__(self, overrides=None):
        self.lib=C.CDLL(str(cg.build_dll(overrides, 'game_area')))
        self.lib.integration_create.argtypes=[TRACE,P]; self.lib.integration_create.restype=P
        self.lib.integration_ram.argtypes=[P]; self.lib.integration_ram.restype=P
        self.lib.integration_call.argtypes=[P,U,U,U,U,U]; self.lib.integration_call.restype=I
        self.lib.integration_wrapper.argtypes=[P,U,U,U,U,U]; self.lib.integration_wrapper.restype=I
        self.lib.integration_load_area.argtypes=[P,I,I,P,C.c_size_t]; self.lib.integration_load_area.restype=I
        self.lib.integration_set_direction_padding.argtypes=[P,U]
        self.lib.integration_set_spawn_padding.argtypes=[P,C.POINTER(C.c_uint16),U]
        self.lib.integration_frame.argtypes=[P]; self.lib.integration_frame.restype=I
        self.lib.integration_set_overlay.argtypes=[P,U,U,P,C.c_size_t]
        self.lib.integration_set_loaded_overlay.argtypes=[P,U]
        self.lib.integration_overlay_words.argtypes=[P,U,U,U,C.POINTER(U)]
        self.lib.integration_overlay_words.restype=I
        self.lib.integration_renderer_words_calls.argtypes=[P]
        self.lib.integration_renderer_words_calls.restype=U
        self.lib.integration_hook.argtypes=[P,U,U,U,U,U,U,U,U]
        self.lib.integration_hook.restype=I
        self.lib.integration_stack7.argtypes=[P,U,U,U,U,U,U,U]
        self.lib.integration_stack7.restype=I
        self.lib.ae_ram_error.argtypes=[P]; self.lib.ae_ram_error.restype=C.c_char_p
        self.lib.ae_ram_clear_error.argtypes=[P]
        self.clog=[]; self.mlog=[]; self.callback_error=None
        self.trace=TRACE(self.callback)
        self.shim=self.lib.integration_create(self.trace,None)
        self.ram=self.lib.integration_ram(self.shim)
        self.hud_draws=[]
        self.hud_callback=DrawFn(lambda user, ram, draw: self.hud_draws.append(HudDraw.from_buffer_copy(draw.contents)))
        self.lib.integration_set_hud_draw.argtypes=[P,DrawFn]
        self.lib.integration_set_hud_draw(self.shim,self.hud_callback)
        self.main=(C.c_ubyte*mr.RAM_SIZE).from_address(self.ram)
        self.scratch=(C.c_ubyte*mr.SCRATCH_SIZE).from_address(self.ram+mr.RAM_SIZE)
        self.cpu=mr.MipsRam.from_extract(); self.cpu.attach_gte_oracle()
        self.base=self.cpu.snapshot()
        self.spec={}
        records=hook_inventory()
        for r in records:
            if r['address'] and r['kind'] in {'missing','sdk','render','archive'}:
                self.spec.setdefault(r['address'],(r['canonical'],4))
        for name,(address,n) in cg.MISSING.items(): self.spec[address]=(name,n)
        monkey_addresses=dict(mh.parse_hooks())
        for name,kind in cg.MONKEY_HOST.items():
            if name in mh.HOOK_SPECS:
                spec=mh.HOOK_SPECS[name]
                address=monkey_addresses[name]
                if address: self.spec[address]=(name,spec['n'])
        for name,address,n in stage.HOOKS:
            if address in self.spec: self.spec[address]=(name,n)
        # Reuse each reviewed unit checker's meaningful argument counts.
        # This avoids comparing leftover caller registers as API arguments.
        for checker in (ROOT/'tools/mips').glob('check_*.py'):
            tree=ast.parse(checker.read_text(encoding='utf-8-sig'))
            for node in ast.walk(tree):
                if not isinstance(node,ast.Assign):continue
                if not any(isinstance(target,ast.Name) and 'HOOK' in target.id for target in node.targets):continue
                try:value=ast.literal_eval(node.value)
                except (ValueError,TypeError):continue
                if isinstance(value,dict):
                    value=[(spec[0],address,spec[1]) for address,spec in value.items()
                           if isinstance(address,int) and isinstance(spec,(tuple,list)) and
                           len(spec)==2 and isinstance(spec[0],str) and isinstance(spec[1],int)]
                if isinstance(value,(tuple,list)):
                    for item in value:
                        if isinstance(item,(tuple,list)) and len(item)==3 and isinstance(item[0],str) and isinstance(item[1],int) and isinstance(item[2],int) and item[1] in self.spec:
                            self.spec[item[1]]=(self.spec[item[1]][0],item[2])
        # Dict-shaped hook specifications also record meaningful stack words.
        from check_misc import HOOK_ARITY as MISC_ARITY
        for address, (name, count) in list(self.spec.items()):
            if name in MISC_ARITY:
                self.spec[address] = (name, MISC_ARITY[name])
        from check_zakoai import AI_HOOKS
        for name, (address, count) in AI_HOOKS.items():
            if address in self.spec:
                self.spec[address] = (name, count)
        from check_animdraw import TOP_HOOKS
        from check_pausemenu import HOOK_COUNTS, HOOK_ADDRS
        for name, (address, count) in TOP_HOOKS.items():
            if address in self.spec:self.spec[address]=(self.spec[address][0],count)
        for name, count in HOOK_COUNTS.items():
            address=HOOK_ADDRS.get(name)
            if address in self.spec:self.spec[address]=(self.spec[address][0],count)
        for address,(name,n) in list(self.spec.items()):
            if name in SDK_ARITY: self.spec[address]=(name,SDK_ARITY[name])
        # Header-less SDK entry points and pure-memory BIOS operation.
        self.spec.update({0x800A3740:('memset',3),0x8002E520:('VSync',1),
                          0x8002FB00:('SetRotMatrix',1),0x8002FB30:('SetTransMatrix',1),
                          0x80031330:('RotTransPers',4),0x800A1398:('SpuWaitIdle',0),
                          0x8009D79C:('SpuWaitLoop',1)})
        self.spec.update({0x80028D30:('PutDrawEnv',1),0x80028EFC:('PutDispEnv',1),0x80028CC0:('DrawOTag',1)})
        self.spec[0x8005CFEC]=('InitDisplay',0)
        self.spec.update({0x800A1F3C:('SeqTrackClose',1),
                          0x800A2B6C:('SeqTrackSetPlayState',2),
                          0x800A1CCC:('SoundBankFree',1),
                          0x800A342C:('SoundStopAllVoices',1)})
        self.spec[0x80046670]=('CameraLookAt',5)
        self.spec[0x80015240]=('AnimFramePtr',2)
        self.spec[0x80014D70]=('AnimDecodeFrame',1)
        self.spec[0x80030890]=('RotMatrixY',2)
        self.spec[0x8009632C]=('ProgressFlagGet',1)
        self.spec[0x800716EC]=('Indicator_Register',3)
        self.spec[0x800691F8]=('UnknownFunction',0)
        self.spec[0x8002B380]=('DR_MOVE',4)
        self.spec[0x80014208]=('PadRumbleReset',1)
        self.spec[0x80013128]=('PadDemoPlaybackStep',0)
        self.spec[0x80057680]=('UnknownFunction',0)
        self.spec[0x80053550]=('UnknownFunction',1)
        self.spec[0x800892C4]=('UnknownFunction',2)
        self.spec[0x8008A4B0]=('UnknownFunction',2)
        self.spec[0x8005E000]=('Draw_Nop',0)
        self.spec[0x80097A60]=('StageGoalReached',0)
        with (ROOT/'tools/verified.csv').open(newline='') as stream:
            ported={int(row['address'],16) for row in csv.DictReader(stream) if row['status']=='diffed'}
        self.ported=ported
        for address in list(self.spec):
            if address in ported and address<0x80100000 and address not in {0x80046670,0x800A3720,0x800A3700,0x80012360}:
                del self.spec[address]
        self.byname={name:(addr,n) for addr,(name,n) in self.spec.items()}
        self.models={}
        self.cases=0
        # Unported frame entry points are modeled identically on both sides.
        frame_arity={'DrawTextSprites':4,'QueueFrameFlip':4,'SoundStop':1,'OpenPauseMenu':1}
        frame_header=(ROOT/'src/game/ae_game_frame_host.h').read_text()
        frame_names=set(re.findall(r'\b\w+\b',re.search(r'typedef struct AeGameFrameHost \{(.*?)\}',frame_header,re.S)[1]))-{'AeGameFrameFn'}
        symbols={}
        for row in csv.reader((ROOT/'tools/symbols.csv').read_text().splitlines()):
            if len(row)>1 and re.fullmatch('[0-9a-fA-F]{8}',row[0]):symbols[row[1]]=int(row[0],16)
        for name in frame_names:
            if name in symbols and symbols[name] not in ported:self.spec[symbols[name]]=(name,frame_arity.get(name,0))
        self.byname={name:(addr,n) for addr,(name,n) in self.spec.items()}

    def callback(self,_user,name,address,argc,args):
        try:
            name=name.decode()
            words=tuple(args[i] for i in range(argc))
            return self.model(name,address,words,False)
        except Exception as exc:
            self.callback_error=str(exc)
            return 0

    def model(self,name,address,args,mips):
        if name.startswith('ae_'):
            name=name[3:]
        if not address and name in self.byname: address=self.byname[name][0]
        if address in self.spec: name,n=self.spec[address]
        else: n={'BodyMove (player / RC car)':3, 'RenderScreenFlash':1, 'RenderFadeSpiral':1}.get(name,len(args))
        args=tuple(args[:n])
        if name=='Printf':
            count=cg.PRINTF_ARGS.get(args[0])
            if count is not None:args=args[:count+1]
        if name=='BeginFrame':
            # Native presentation owns a dedicated packet arena. A zero cursor
            # aliases low RAM and lets GPU commands become unrelated sound data.
            if mips: self.cpu.write32(0x800EC354,0x801E8000)
            else: struct.pack_into('<I',self.main,0xEC354,0x801E8000)
        if name=='DrawPrim' and self.models.get('_menu_message_animation'):
            value=self.models['_menu_message_animation']
            if mips: self.cpu.write16(0x800F5A10,value)
            else: struct.pack_into('<H',self.main,0xF5A10,value)
        if name=='DrawTextSprites' and self.models.get('_draw_text_close_message'):
            if mips: self.cpu.write8(0x800F5A00,0)
            else: self.main[0xF5A00]=0
        if name=='memset':
            dst,value,size=args
            if mips: self.cpu.write_bytes(dst,bytes([value&255])*size)
            else:
                target=dst & 0x1FFFFFFF
                if target+size>mr.RAM_SIZE: raise ValueError('memset outside main RAM')
                C.memset(self.ram+target,value,size)
            return dst
        (self.mlog if mips else self.clog).append((name,address,tuple(norm(a) for a in args)))
        if name=='Area02_EffectDrawRecord': return args[7]
        if name in {'Area02_SurfaceProjectPoint','Area02_SurfaceProjectVertex'}:
            for output in args[3:5]:
                if not output: continue
                if mips: self.cpu.write32(output,0)
                else: struct.pack_into('<I',self.scratch,output-mr.SCRATCH_BASE,0)
            return 0
        if name=='RotTransPers':
            for output in args[1:4]:
                if mips:self.cpu.write32(output,0)
                elif mr.SCRATCH_BASE<=output<mr.SCRATCH_BASE+mr.SCRATCH_SIZE:
                    for i in range(4):self.scratch[output-mr.SCRATCH_BASE+i]=0
                else:
                    offset=output & 0x1FFFFFFF
                    if offset+4>mr.RAM_SIZE:raise ValueError('projection output outside RAM')
                    for i in range(4):self.main[offset+i]=0
            return 0
        if name=='RotTransSV':
            for output,size in ((args[1],8),(args[2],4)):
                if mips:self.cpu.write_bytes(output,bytes(size))
                else:C.memset(self.ram+(output&0x1FFFFFFF),0,size)
            return 0
        if name=='ProjectEffectPoint':
            output=args[1]
            if mips:
                self.cpu.write32(output,0);self.cpu.write32(output+4,0)
            else:
                for i in range(8):self.scratch[output-mr.SCRATCH_BASE+i]=0
            return 0
        if name=='VSync': return 0x2222
        return self.models.get(name,0)

    def hooks(self,root=0):
        def hook(address):
            def invoke(cpu,regs):
                name,n=self.spec[address]
                args=tuple(regs[4+i] if i<4 else cpu.read32(regs[29]+i*4) for i in range(n))
                return self.model(name,address,args,True)
            return invoke
        return {address:hook(address) for address in self.spec if address!=root}

    def sync(self):
        self.hud_draws=[]
        C.memmove(self.main,bytes(self.cpu.ram),mr.RAM_SIZE)
        C.memmove(self.scratch,bytes(self.cpu.scratch),mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        self.clog=[]; self.mlog=[]; self.callback_error=None

    def direct_helpers(self):
        rng=random.Random(0xAEE2026)
        with (ROOT/'tools/verified.csv').open(newline='') as stream:
            rows=[r for r in csv.DictReader(stream) if r['status']=='diffed']
        from generate_game_calls import SPECIAL
        rows.sort(key=lambda r:(int(r['address'],16),r['c_function'] in SPECIAL,'native' in r['source'],r['c_function']))
        indices={row['c_function']:i for i,row in enumerate(rows)}
        counts={}
        for name,address in [('ae_pad_QuadrantFromSigns',0x8001413C),('ae_player_vec_length3',0x80046618),
                             ('ae_player_can_switch_gadget',0x80066F50),('ae_player_is_swimming',0x80068B7C),
                             ('ae_player_normalize',0x800301B0),('ae_sdk_square0_lane',0x80014498),
                             ('ae_PlayerAct_ScaleMoveVel',0x80065C20),
                             ('ae_PlayerAct_TurnAroundAndJump',0x80066168)]:
            for case in range(96):
                self.cpu.restore(self.base)
                self.cpu.write16(0x800F44A0,case%16)
                self.cpu.write16(0x800EC23E,case if case<80 else rng.randrange(256))
                if name=='ae_pad_QuadrantFromSigns': args=[rng.randrange(-32768,32768),rng.randrange(-32768,32768)]
                elif name=='ae_player_vec_length3': args=[rng.randrange(-16000,16001) for _ in range(3)]
                elif name in {'ae_player_normalize','ae_sdk_square0_lane'}:
                    for axis in range(3):self.cpu.write32(0x80153000+axis*4,rng.randrange(-16000,16001)&MASK)
                    args=[0x80153000,0x80153000 if case&1 else 0x80153020]
                elif name in {'ae_PlayerAct_ScaleMoveVel','ae_PlayerAct_TurnAroundAndJump'}:
                    self.cpu.write8(0x800EC2D2,4 if case%3 else 1)
                    self.cpu.write16(0x800F5124,case%4)
                    self.cpu.write32(0x800F5128,1234)
                    self.cpu.write32(0x800EC2F4,(-700+case)&MASK)
                    self.cpu.write32(0x800EC2FC,(700-case)&MASK)
                    self.cpu.write32(0x800EC038,100)
                    # The original action-table entry ignores incoming a0/a1.
                    args=[rng.getrandbits(32),rng.getrandbits(32)]
                else:args=[]
                self.sync()
                words=[word&MASK for word in args]+[0]*4
                result=self.lib.integration_wrapper(self.shim,indices[name],*words[:4])
                registers=self.cpu.call(address,args,hooks=self.hooks(address))
                self.compare(f'wrapper {name} #{case}')
                if name not in {'ae_PlayerAct_ScaleMoveVel','ae_PlayerAct_TurnAroundAndJump'} and result&MASK!=registers[2]:raise AssertionError(f'{name}: return differs')
            counts[name]=96
        (ROOT/'extract/mips/game_calls_helpers.json').write_text(json.dumps(counts,indent=2))
        print('Direct special-interface wrapper cases:',sum(counts.values()))

    def direct_dispatch_boundaries(self):
        from game_call_inventory import ledger
        from generate_game_calls import SPECIAL
        rows=sorted(ledger(),key=lambda r:(int(r['address'],16),r['c_function'] in SPECIAL,'native' in r['source'],r['c_function']))
        indices={row['c_function']:i for i,row in enumerate(rows)}
        counts={}
        for function,address,name,arity in [
            ('ae_pad_CameraRotMatrix',0x80046670,'CameraLookAt',5),
        ]:
            self.spec[address]=(name,arity)
            self.models[name]=-123
            for case in range(16):
                self.cpu.restore(self.base)
                words=[0x80153000+axis*0x40+case*4 for axis in range(5)]
                self.cpu.write32(0x801FFF10,words[4])
                self.sync()
                result=self.lib.integration_wrapper(self.shim,indices[function],*words[:4])
                registers=self.cpu.call(address,words[:arity],hooks=self.hooks())
                self.compare(f'partial entry host forwarding {function} #{case}')
                if result&MASK!=registers[2]:raise AssertionError(f'{function}: host return differs')
            counts[function]=16
        # PlayerCheckGadgetSelect now composes real ports rather than forwarding
        # to a missing-function callback. Run its original entry unintercepted.
        import check_player2
        self.spec.pop(0x80067974,None)
        for case in range(32):
            self.cpu.restore(self.base)
            check_player2.player_state_tables(self.cpu)
            self.cpu.write16(0x800F44A0,case%16)
            self.cpu.write16(0x800EC23E,70 if case%7==0 else 0)
            self.cpu.write8(0x800EC2D2,1)
            self.cpu.write32(0x800EC1F8,self.cpu.read32(0x800F4DE0))
            for slot,gadget in enumerate((0,1,3,9)):self.cpu.write8(0x800F51A8+slot,gadget)
            for slot in range(12):self.cpu.write16(0x800BF670+slot*2,65535)
            self.cpu.write32(0x1F800054, (0x10,0x80,0x20,0x40,0xF0)[case%5])
            self.cpu.write32(0x1F800058, 0xF0 if case%3 else 0)
            self.sync()
            result=self.lib.integration_wrapper(self.shim,indices['ae_player_gadget_select_request'],0,0,0,0)
            registers=self.cpu.call(0x80067974,hooks=self.hooks(0x80067974))
            self.compare(f'composed gadget-selection entry #{case}')
            if result&MASK!=registers[2]:raise AssertionError('composed gadget-selection return differs')
        counts['ae_player_gadget_select_request']=32
        self.cpu.restore(self.base);self.sync()
        self.lib.integration_wrapper(self.shim,indices['ae_sdk_normalize_core'],0,0,0,0)
        error=self.lib.ae_ram_error(self.ram).decode()
        if 'internal register entry' not in error or bytes(self.main)!=bytes(self.cpu.ram):
            raise AssertionError('internal register-only entry must fail without game RAM writes')
        counts['ae_sdk_normalize_core (unsupported ABI guard)']=1
        self.lib.ae_ram_clear_error(self.ram)
        (ROOT/'extract/mips/game_dispatch_boundaries.json').write_text(json.dumps(counts,indent=2))
        print('Dispatch boundaries: 16 renderer forwards, 32 composed gadget selections; internal-entry rejection: 1')

    def direct_overlay_routes(self):
        self.load(2)
        for member in (0x181,0x18C,0x182,0x18C,0):
            self.cpu.write16(0x8013F2AC,7)
            self.sync()
            self.lib.integration_set_loaded_overlay(self.shim,member)
            result=self.lib.integration_call(self.shim,0x8013D81C,0,0,0,0)
            if member==0x18C:
                assert result==7 and not self.clog, 'loaded overlay port did not execute'
            else:
                assert self.clog and self.clog[0][1]==0x8013D81C, 'wrong member executed overlay code'
            assert bytes(self.main)==bytes(self.cpu.ram), 'dispatch changed unrelated RAM'
        self.lib.integration_set_loaded_overlay(self.shim,0x18C)
        self.cpu.write32(0x800E6960+0xE0,8)
        words=[0x800E6960,(self.cpu.read32(0x800EC204)+2000)&MASK,
               self.cpu.read32(0x800EC208),self.cpu.read32(0x800EC20C),1000]
        self.sync()
        result=self.lib.integration_overlay_words(self.shim,0,0x8005654C,5,(U*5)(*words))
        original=self.cpu.call(0x8005654C,words,hooks=self.hooks(0x8005654C))
        self.compare('overlay name-dispatch five-word port')
        assert result&MASK==original[2], 'name-dispatch return changed'
        assert result==1, 'five-word fixture must depend on the nonzero radius'
        for render,count in ((0,7),(0,9),(1,6),(1,8),(1,11)):
            self.sync()
            words=[(0xF1352468+i*0x1234567)&MASK for i in range(count)]
            name='OverlayRenderProbe' if render else 'OverlayCallProbe'
            self.models[name]=0x1234
            before=self.lib.integration_renderer_words_calls(self.shim)
            result=self.lib.integration_overlay_words(self.shim,render,0x8014FFFF,count,(U*count)(*words))
            assert self.lib.integration_renderer_words_calls(self.shim)-before==render, 'renderer used missing-game route'
            expected=self.model(name,0x8014FFFF,words,True)
            self.compare(f'overlay words render={render} count={count}')
            assert result==expected, 'name-dispatch boundary return lost'
        print('Overlay routes: 5 member guards; 6 array/port comparisons')

    def direct_duplicate_overlay_routes(self):
        self.load(2)
        for member,expected in ((0x181,0),(0x18C,7),(0x182,None),(0x18C,7),(0x181,0)):
            self.cpu.write16(0x8013F2AC,7)
            self.sync();self.lib.integration_set_loaded_overlay(self.shim,member)
            result=self.lib.integration_call(self.shim,0x8013D81C,0,0,0,0)
            if expected is None:
                assert self.clog, 'unknown member executed duplicate address'
            else:
                assert result==expected and not self.clog, 'duplicate address selected wrong member'
            assert bytes(self.main)==bytes(self.cpu.ram), 'duplicate dispatch changed unrelated RAM'
        print('Duplicate overlay address: 5 member-selection checks')

    def direct_hud_hooks(self):
        count=0
        for index,record in enumerate(hook_inventory()):
            if record['kind']!='hud': continue
            for kind in range(1,7):
                self.cpu.restore(self.base);self.sync()
                words=[kind,0xFEDCBA98,0xFFFF8000,0x7FFFFFFF,0,0,0]
                self.lib.integration_hook(self.shim,index,*words)
                assert len(self.hud_draws)==1, f'{record["name"]}: typed request lost'
                draw=self.hud_draws[0]
                assert (draw.kind,draw.asset,draw.x,draw.y)==(kind,words[1],-32768,words[3]), 'typed HUD request changed'
                assert not self.clog, 'HUD request reached an integer/missing boundary'
                assert not self.lib.ae_ram_error(self.ram), 'HUD wire faulted'
                count+=1
        print(f'Typed HUD hooks: 6; request comparisons: {count}')

    def direct_host_hooks(self):
        """Compare every SDK/archive/render wire with its intercepted boundary.

        SDK internals are excluded on both sides by the established boundary,
        rather than executed and compared with a no-op platform.
        """
        from generate_game_hooks import ARCHIVE_ARITY
        rng=random.Random(0xAEEB0A)
        records=hook_inventory()
        counts={}
        for index,record in enumerate(records):
            if record['kind'] not in {'sdk','archive','render'}:continue
            name=record['canonical'];address=record['address']
            arity=(SDK_ARITY[name] if record['kind']=='sdk' else
                   ARCHIVE_ARITY[name] if record['kind']=='archive' else
                   max(len(r['params'])-2 for r in records if r['kind']=='render' and r['canonical']==name))
            # A native render operation replacing an inline block has no
            # original entry; test its typed boundary contract explicitly.
            for case in range(8):
                self.cpu.restore(self.base)
                self.sync()
                words=[0x1F800100+i*0x20 for i in range(7)]
                if name=='memset':words[:3]=[0x80153000,rng.randrange(256),16]
                if arity==1 and name not in {'SetRotMatrix','SetTransMatrix','SetColorMatrix'}:
                    words[0]=rng.getrandbits(32)
                self.spec[address]=(name,arity)
                self.models[name]=rng.randrange(-32768,32768)
                result=self.lib.integration_hook(self.shim,index,*words)
                boundary_words=(words[:len(record['params'])-2]+[0]*6)[:6]
                if address:
                    registers=self.cpu.call(address,boundary_words[:arity],hooks=self.hooks())
                    expected=registers[2]
                else:
                    expected=self.model(name,address,boundary_words[:arity],True)&MASK
                self.compare(f'host hook {record["table"]}.{record["name"]} #{case}')
                if record['ret']!='void' and result&MASK!=expected:
                    raise AssertionError(f'host hook {name}: return differs')
            counts[record['table']+'.'+record['name']]=8
        (ROOT/'extract/mips/game_host_hooks_results.json').write_text(json.dumps(counts,indent=2))
        print(f'Host boundary hooks: {len(counts)}; comparisons: {sum(counts.values())}')

    def direct_extended_hooks(self):
        records=hook_inventory()
        index=next(i for i,r in enumerate(records) if r['table']=='AeGadgetHooks' and r['name']=='EffectSpawnFromDef')
        for case in range(16):
            self.cpu.restore(self.base)
            position,definition,direction=0x801D0000,0x801D0040,0x801D00C0
            self.cpu.write_bytes(position,struct.pack('<hhhh',-123,456,-789,0x1357))
            self.cpu.write_bytes(definition,bytes(84))
            self.cpu.write_bytes(direction,struct.pack('<hhhh',0,0,4096,0))
            self.cpu.write8(0x800B1BD0,0)
            args=[position,definition,1+case%3,-1-case,direction if case&1 else 0]
            self.sync()
            self.lib.integration_hook(self.shim,index,*(args+[0,0]))
            self.cpu.call(0x80070284,args,hooks=self.hooks(0x80070284))
            self.compare(f'EffectSpawnFromDef fifth argument #{case}')
        # All seven words must reach the unported named hook, with no register simulation.
        index=next(i for i,r in enumerate(records) if r['table']=='AeZakoAiHooks' and r['name']=='FUN_8013DF54')
        self.spec[0x8013DF54]=('FUN_8013DF54',7)
        for case in range(16):
            self.cpu.restore(self.base);self.sync()
            words=[(0x13572468 + case * 0x10203 + i * 0x11223344) & MASK for i in range(7)]
            self.lib.integration_hook(self.shim,index,*words)
            boundary_hooks=self.hooks()
            boundary_hooks[0x8013DF54]=lambda cpu,regs: self.model('FUN_8013DF54',0x8013DF54,tuple(regs[4:8])+tuple(cpu.read32(regs[29]+i*4) for i in (4,5,6)),True)
            self.cpu.call(0x8013DF54,words,hooks=boundary_hooks)
            self.compare(f'FUN_8013DF54 seven words #{case}')
            # Observe the helper stack inside the callback, then require exact restoration.
            saved=[0x89ABCDEF ^ i ^ case for i in range(3)]
            for i,value in enumerate(saved):
                self.cpu.write32(0x801FFF10+i*4,value)
            self.sync()
            self.lib.integration_stack7(self.shim,*words)
            self.model('TestStackWords',0,words,True)
            self.compare(f'call7 stack argument forwarding #{case}')
            actual=[struct.unpack_from('<I',self.main,0x1FFF10+i*4)[0] for i in range(3)]
            if actual!=saved:raise AssertionError('call7 did not restore all three stack words')
        print('Extended hook ABI: 48 comparisons; 16 three-word stack restoration checks')

    def direct_frame_branches(self):
        profiles=[('inactive pause mode '+str(value),{},[(0x800BEF74,1,value)]) for value in (2,3)]
        profiles += [('fade exit '+str(value),{},[(0x800F4472,2,value),
                     (0x800F4480,2,2),(0x800F4482,2,47)]) for value in (0,3)]
        profiles += [('transition '+str(pending)+'/'+str(buffer),{},[
            (0x800F447C,1,6),(0x800F4486,2,pending),(0x800F4484,2,buffer),(0x800EC35C,4,buffer^1)])
            for pending in (0,1) for buffer in (0,1)]
        profiles += [('animation only',{},[(0x800F4450,4,1)])]
        profiles += [('RC car render',{},[(0x800F4450,4,1),(0x800F5A90,4,1),
                     (0x800F5A80,2,4096),(0x800F5A8A,2,4096)])]
        profiles += [('collision skip '+str(mode),{},[(0x800F44A0,2,mode),(0x800F4450,4,1)]) for mode in (8,13)]
        profiles += [('debug text',{},[(0x800A8A58,4,1),(0x800EC350,4,15)])]
        for index,(label,models,writes) in enumerate(profiles):
            self.models.clear();self.models.update(models)
            self.load(1)
            self.prepare_player()
            for address,size,value in writes:self.cpu.write(address,size,value)
            if label=='RC car render':
                table=0x80103000+self.cpu.read16(0x80103008)*4
                model=table+self.cpu.read16(table+4+11*4)*4
                self.cpu.write32(0x800EC110,model)
            self.sync();self.frame(1,index)
            if label=='RC car render':
                assert any(name=='ModelTransformRigid' and words[0]==0x800EC110
                           for name,address,words in self.clog),'RcCarDraw must reach the renderer'
            print('Frame branch:',label,'matches')
        (ROOT/'extract/mips/game_frame_branches.json').write_text(json.dumps(dict(profiles=[p[0] for p in profiles],frames=len(profiles),comparisons=self.cases),indent=2))

    def direct_pause_math_hooks(self):
        records=hook_inventory()
        rng=random.Random(0x504D4158)
        for name in ('RotMatrixX','RotMatrixY','RotMatrixZ'):
            index=next(i for i,r in enumerate(records) if r['table']=='AePauseMenuHooks' and r['name']==name)
            address=records[index]['address']
            assert address not in self.spec, 'pure SDK rotation must execute original instructions'
            for case in range(32):
                self.cpu.restore(self.base)
                matrix=0x801D0000
                self.cpu.write_bytes(matrix,rng.randbytes(32))
                angle=([-2147483648,-4097,-1024,0,1024,4097,2147483647][case]
                       if case<7 else rng.getrandbits(32))&MASK
                self.sync()
                result=self.lib.integration_hook(self.shim,index,angle,matrix,0,0,0,0,0)
                original=self.cpu.call(address,(angle,matrix))
                self.compare(f'pause maths {name} #{case}')
                assert result&MASK==original[2]==matrix, 'axis return pointer differs'
        print('Pause SDK maths: 96 real-instruction hook comparisons')

    def compare(self,label):
        error=self.lib.ae_ram_error(self.ram).decode()
        if error: raise AssertionError(f'{label}: C fault: {error}; last calls {self.clog[-6:]}')
        if self.callback_error: raise AssertionError(self.callback_error)
        lo=0x1FF000
        a=bytes(self.cpu.ram[:lo]); b=bytes(self.main[:lo])
        # Existing check_game exclusion: never-initialized fourth position halfword.
        reg.declare('stack_window','801FF000..80200000',checker='check_game')
        reg.declare('ram_mask','800B9900+0x34x100+0x1B:1',checker='check_game')
        a=bytearray(a); b=bytearray(b)
        if self.hud_draws:
            reg.declare('ram_mask','HUD request packet spans and ordering tags',checker='check_game_area')
            for draw in self.hud_draws:
                primitive=draw.primitive_address
                if draw.kind==1 and draw.packet_bytes==24:
                    rd=self.cpu.read16; rb=self.cpu.read8
                    s16=lambda value: C.c_int16(value).value
                    candidates=[
                        (rd(primitive+4),s16(rd(primitive+12)),s16(rd(primitive+14)),rb(primitive+20),rb(primitive+22),rd(primitive+18),rb(primitive+16),rb(primitive+17)),
                        (rd(primitive+4),s16(rd(primitive+12)),s16(rd(primitive+14)),rb(primitive+16),rb(primitive+17),rd(primitive+18),rd(primitive+20),rd(primitive+22)),
                        (rd(primitive+4),s16(rd(primitive+20)),s16(rd(primitive+22)),rb(primitive+16),rb(primitive+17),rd(primitive+18),rd(primitive+12),rd(primitive+14))]
                    requested=(draw.tpage,draw.x,draw.y,draw.u,draw.v,draw.clut,draw.width,draw.height)
                    if requested not in candidates: raise AssertionError(f'{label}: HUD sprite request {requested} differs from packet {primitive:08X}: {candidates}')
                for address,size in [(primitive,draw.packet_bytes),(draw.ordering_entry,4)]:
                    offset=address & 0x1FFFFFFF
                    assert offset+size<=lo
                    b[offset:offset+size]=a[offset:offset+size]
        reg.declare('ram_mask','camera slot target padding (slot +0x18:2)',checker='check_game_area')
        slots=self.cpu.read32(0x800B4D40)
        for index in range(12):
            slot=slots+index*0xB0
            if slots and self.cpu.read8(slot):
                offset=slot+0x18-0x80000000
                b[offset:offset+2]=a[offset:offset+2]
        for i in range(100):
            offset=0xB9900+i*0x34+0x1B
            b[offset]=a[offset]
        reg.declare('ram_mask','monkey+0xC8:4',checker='check_game')
        for i in range(8):
            offset=0xE5500+i*0x250+0xC8
            b[offset:offset+4]=a[offset:offset+4]
        if a!=b:
            offsets=[i for i,(x,y) in enumerate(zip(a,b)) if x!=y]
            raise AssertionError(f'{label}: {len(offsets)} RAM bytes differ: '+str([(hex(0x80000000+i),a[i],b[i]) for i in offsets[:16]])+f'; calls C {self.clog[-4:]} M {self.mlog[-4:]}')
        if bytes(self.cpu.scratch)!=bytes(self.scratch):
            raise AssertionError(f'{label}: scratchpad differs: '+str([
                (hex(mr.SCRATCH_BASE+i),a,b) for i,(a,b) in
                enumerate(zip(self.cpu.scratch,self.scratch)) if a!=b][:16]))
        if self.clog!=self.mlog:
            index=next((i for i,(a,b) in enumerate(zip(self.clog,self.mlog)) if a!=b),min(len(self.clog),len(self.mlog)))
            raise AssertionError(f'{label}: calls differ at {index}: C {self.clog[max(0,index-1):index+3]} M {self.mlog[max(0,index-1):index+3]}')
        # Renderer-owned bytes persist between frames. Share the original packet
        # image after checking requests, so old packet contents cannot masquerade
        # as a fresh game-state difference on the next frame.
        for draw in self.hud_draws:
            for address,size in [(draw.primitive_address,draw.packet_bytes),(draw.ordering_entry,4)]:
                offset=address & 0x1FFFFFFF
                C.memmove(self.ram+offset,bytes(self.cpu.ram[offset:offset+size]),size)
        self.cases+=1

    def load(self,area,entry=0):
        self.cpu.restore(self.base)
        self.cpu._decoded.clear()  # The same addresses contain different overlay instructions.
        for address in list(self.spec):
            if address>=0x80100000:del self.spec[address]
        overlay=0;overlay_blob=None
        if area in areaset.OVERLAY_FOR_AREA:
            overlay=areaset.OVERLAY_FOR_AREA[area]
            overlay_blob=(ROOT/f'extract/members/{overlay:03X}.bin').read_bytes()
            self.cpu.load_area(overlay_blob,0x8013D000)
            for address,name in areaset.OVERLAY_HELPERS[overlay].items():
                self.spec[address]=(name,areaset.HOOK_ARITY[name])
            if overlay==0x19E:
                self.spec[0x8013D000]=('UnknownFunction',1)
                self.spec[0x80140D60]=('FUN_80140d60',0)
                self.spec[0x801402D8]=('FUN_801402d8',1)
        elif area==2:
            overlay=0x18C
            overlay_blob=(ROOT/'extract/members/18C.bin').read_bytes()
            self.cpu.load_area(overlay_blob,0x8013D000)
        # The original 18C ports run unintercepted. Only their declared native
        # renderer and still-unported EXE dependencies are host boundaries.
        if overlay == 0x18C:
            from check_ovl18c import RENDERERS, ORIGINAL_HELPERS
            self.spec.update(RENDERERS)
            self.spec.update({address:spec for address,spec in ORIGINAL_HELPERS.items() if address not in self.ported})
        # Runtime handlers have different identities in each area's overlay.
        # Their caller contracts are known: object, no arguments, or collider/index.
        for table,count,arity in [(0x800B3344,15,2)]:
            for index in range(count):
                address=self.cpu.read32(table+index*4)
                if 0x80010000<=address<0x80200000 and address not in self.ported:
                    self.spec.setdefault(address,('UnknownFunction',arity))
        handlers=self.cpu.read32(0x800AF868+area*4)
        if handlers:
            for group in range(128):
                init=self.cpu.read32(handlers+group*12)
                update=self.cpu.read32(handlers+group*12+4)
                if not init and not update:break
                for address in (init,update):
                    if address and address not in self.ported:
                        self.spec.setdefault(address,('UnknownFunction',1))
        member=dump_areas.area_members()[area]
        blob=(ROOT/f'extract/members/{member:03X}.bin').read_bytes()
        # Persistent allocator cursor belongs to the boot host, not the area image.
        self.cpu.write32(0x800E38E4,0x80100000)
        self.cpu.write32(0x800F4470,0)
        self.sync()
        overlay_buffer=C.create_string_buffer(overlay_blob) if overlay_blob else None
        self.lib.integration_set_overlay(self.shim,overlay,0x8013D000,overlay_buffer,len(overlay_blob or b''))
        buf=C.create_string_buffer(blob)
        cresult=self.lib.integration_load_area(self.shim,area,entry,buf,len(blob))
        self.cpu.load_area(blob)
        self.cpu.write16(0x800F4476,area)
        try:
            self.cpu.call(0x8005BEF0,hooks=self.hooks(0x8005BEF0))
            regs=self.cpu.call(0x8005D100,(1,area,entry),hooks=self.hooks(0x8005D100))
            self.cpu.call(0x8005DDE0,hooks=self.hooks(0x8005DDE0))
        except mr.MipsError as exc:
            raise AssertionError(f'area {area:02X}: MIPS fault: {exc}; C fault {self.lib.ae_ram_error(self.ram)}; last M {self.mlog[-5:]} C {self.clog[-5:]}') from exc
        self.compare(f'area {area:02X}')
        if (cresult&MASK)!=regs[2]: raise AssertionError(f'area {area:02X}: return differs')

    def prepare_player(self):
        """Boot fixture: real player tables and ready controller, before frames.

        PlayerInit is outside this batch. These explicit initial conditions do
        not intercept PlayerUpdate or replace any of its per-frame behavior.
        """
        import check_player2
        check_player2.player_state_tables(self.cpu)
        model=0x80103000+self.cpu.read16(0x80103004)*4
        # Boot-owned actor setup, using the real member header and bone table.
        # ModelInitAuto is ported; execute its original on the oracle below.
        self.sync()
        self.lib.integration_call(self.shim,0x80015BA0,0x800EC1E0,model,0,0)
        self.cpu.call(0x80015BA0,(0x800EC1E0,model),hooks=self.hooks(0x80015BA0))
        self.compare('player bootstrap actor')
        self.cpu.write16(0x800EC2C8,5)  # health
        self.cpu.write8(0x800EC2D2,1)  # net
        self.cpu.write8(0x800EC2D3,1)
        self.cpu.write16(0x800EC23E,0)
        self.cpu.write16(0x800EC26E,4096)
        self.cpu.write16(0x800EC248,0);self.cpu.write16(0x800EC24A,4096);self.cpu.write16(0x800EC24C,0)
        self.cpu.write32(0x800EC1F8,self.cpu.read32(0x800F4DE0))
        for address,value in [(0x800EC014,173),(0x800EC018,-1650),(0x800EC01C,-670),
                              (0x800EC024,1280),(0x800EC02C,200),(0x800EC034,1010)]:
            self.cpu.write32(address,value&MASK)
        self.cpu.write32(0x800E5494,3)
        # No sound-bank payload has been loaded in this fixture. Boot represents
        # those slots as -1; zero would claim a loaded bank with null tables.
        for slot in range(12):self.cpu.write16(0x800BF670+slot*2,65535)
        self.models['PadGetState']=6;self.models['PadInfoMode']=7
        self.sync()
        self.lib.integration_call(self.shim,0x80064208,0,0,0,0)
        self.cpu.call(0x80064208,(0,),hooks=self.hooks(0x80064208))
        self.compare('player bootstrap state selection')
        # The debug fixture chooses ordinary controllable mode after proving
        # the original stage entry. First leave startup mode 12, then apply
        # mode 0's normal entry reset; both calls execute original instructions.
        for index in range(2):
            self.lib.integration_call(self.shim,0x8007B374,0,0,0,0)
            self.cpu.call(0x8007B374,(0,),hooks=self.hooks(0x8007B374))
            self.compare(f'player bootstrap normal mode {index}')
        self.player_states=set()

    def net_catch(self):
        """Close-range encounter using untouched area-01 monkey AI and real pad input.

        Only the initial player placement/facing and caught-ID sentinel are test
        setup. No contacts, monkey states, animation frames or capture flags are
        injected. Each frame compares original gameplay, RAM and host requests.
        """
        self.load(1);self.prepare_player()
        neutral=bytes((0,0x73,255,255,128,128,128,128))
        for frame in range(60):self.frame(1,frame,neutral)
        player,monkey=0x800EC1E0,0x800E5500
        for axis in range(3):
            self.cpu.write32(player+0x24+axis*4,
                (self.cpu.read32(monkey+0x24+axis*4)-(150 if axis==2 else 0))&MASK)
        for offset in (0x3C,0x60,0x74):
            for axis in range(3):self.cpu.write16(player+offset+axis*2,4096 if axis==2 else 0)
        self.cpu.write16(0x800AEB5A,65535)
        self.sync()
        self.cpu.coverage=set()
        caught_frame=None;capture_modes=set()
        for frame in range(49):
            packet=bytes((0,0x73,255,255,128,0 if frame<10 else 128,128,128))
            self.frame(1,frame,packet)
            capture_modes.add(self.cpu.read16(0x800F44A0))
            if self.cpu.read8(monkey+0x7A)==3 and caught_frame is None:
                caught_frame=frame
                assert self.cpu.read16(0x800AEB5A)==self.cpu.read8(monkey+0x7C)
                assert not self.cpu.read32(monkey+0x24C)&2, 'caught body must be unregistered'
        assert caught_frame==11, f'net contact must catch on frame 11, got {caught_frame}'
        assert {0,8}.issubset(capture_modes), 'capture mode must run and return'
        # Persistence is a separate stage service, not the live catch store.
        self.lib.integration_call(self.shim,0x800334C8,0,0,0,0)
        self.cpu.call(0x800334C8,hooks=self.hooks(0x800334C8))
        self.compare('net catch saved activities')
        assert self.cpu.read8(0x800F44BC)==2, 'original saves live caught activity 3 as 2'
        for pc in (0x80041FB8,0x80041FD0,0x8007C2A0,0x80014EF0,0x800334C8):
            assert pc in self.cpu.coverage, f'catch path must execute original {pc:08X}'
        assert 0x80014EF0 not in self.spec and 0x800412B0 not in self.spec
        result=dict(area=1,warmup_frames=60,swing_frames=49,caught_frame=caught_frame,
                    caught_id=self.cpu.read16(0x800AEB5A),saved_activity=2,
                    capture_modes=sorted(capture_modes),original_gameplay_intercepted=False)
        self.cpu.coverage=None
        print('Net catch:',json.dumps(result),flush=True)
        return result

    def flash_regression(self):
        """Keep the flash position intact across the original MatrixLookDir."""
        self.cpu.restore(self.base)
        self.cpu.write8(0x800BAD80,30)
        for axis,value in enumerate((271,-1777,139)):
            self.cpu.write16(0x800BAD84+axis*2,value&65535)
        self.cpu.write16(0x800AE8F0,0x1234)
        tail=bytes.fromhex('aa553412000078560000bc9a0000')
        self.cpu.write_bytes(mr.DEFAULT_SP-64-32-80+34,tail)
        self.sync()
        C.memmove(self.ram+0x1FF0B2,tail,14)
        model=SimpleNamespace(spawn_padding=[])
        self.cpu.call(0x8006C6D4,hooks=self.hooks(0x8006C6D4),patches=cg.mips_patches(model))
        padding=(C.c_uint16*len(model.spawn_padding))(*model.spawn_padding)
        self.lib.integration_set_spawn_padding(self.shim,padding,len(model.spawn_padding))
        self.lib.integration_call(self.shim,0x8006C6D4,0,0,0,0)
        self.compare('GadgetFlashUpdate live MatrixLookDir position')

    def frame(self,area,index,packet=None):
        self.hud_draws=[]
        self.clog=[];self.mlog=[];self.callback_error=None
        # Move, hold/release jump, then flick the right stick to swing the net.
        # Write raw packets on both images; the original PadUpdate interprets them.
        held=10 if 20<=index%90<26 else 0
        lx=255 if index%90<40 else 128
        ly=128
        rx=255 if 50<=index%90<58 else 128
        if packet is None:
            packet=bytes((0,0x73,(~(held>>8))&255,(~held)&255,rx,128,lx,ly))
        self.cpu.ram[0xE52D0:0xE52D8]=packet
        C.memmove(self.ram+0xE52D0,packet,8)
        self.cpu.write8(0x800E52F2,255);self.main[0xE52F2]=255
        assert 0x80060780 not in self.spec, 'original PlayerUpdate must execute'
        mode=self.cpu.read8(0x800E77A0)
        addresses=[(self.cpu.read32(0x800AF574+mode*4),0)]
        count=self.cpu.reads16(0x800E77E0)
        for slot in range(max(0,min(count,64))):
            obj=0x800E6960+slot*0xE4
            kind=self.cpu.read8(obj+0x7E)
            addresses.append((self.cpu.read32(0x800AF350+kind*4),1))
        for hook in range(min(self.cpu.read8(0x800E77A5),4)):
            addresses.append((self.cpu.read32(0x800B8D20+hook*4),0))
        for address,arity in addresses:
            if 0x80010000<=address<0x80200000 and address not in self.ported:
                self.spec.setdefault(address,('UnknownFunction',arity))
        initial={16:0x800F4350,17:0x800EC350,18:0x800EC360,19:0x800EC3BC,20:0x800F0000}
        patches={0x8005C4A4:lambda _cpu,_regs:mr.SENTINEL,
                 0x8005C444:lambda _cpu,_regs:mr.SENTINEL,
                 0x8005C968:lambda _cpu,_regs:mr.SENTINEL}
        projection=SimpleNamespace(last_projection_depth=0,
            call=lambda name,words:self.model(name,0,words,True))
        patches.update(fxscript.Harness.mips_patches(None,projection))
        ambient_padding=[]
        effect_matrix_padding=[]
        def effect_matrix_local(cpu,regs):
            # MatrixLookDir writes only the nine rotation halfwords; the
            # alignment halfword and translation retain caller-owned input.
            effect_matrix_padding.append(cpu.read_bytes(regs[29]+34,14))
            regs[17]=(regs[29]+16)&MASK
            return 0x80070170
        patches[0x8007016C]=effect_matrix_local
        if area==2:
            from check_ovl18c import Harness as OvlHarness
            render_addresses={'Area02_EffectDrawRecord':0x8013E620,
                'Area02_SurfaceProjectPoint':0x8013ED58,'Area02_SurfaceProjectVertex':0x8013EE10,
                'Area02_SurfaceSubmitPrimary':0x8013EE2C,'Area02_SurfaceSubmitSecondary':0x8013EF34}
            overlay_model=SimpleNamespace(call=lambda name,words:self.model(name,render_addresses[name],words,True))
            for entry in ('Area02_EffectDraw','Area02_SurfaceDraw'):
                patches.update(OvlHarness._mips_patches(None,entry,overlay_model,self.cpu))
            def ambient_position(cpu,regs):
                # Observe the unwritten halfword alongside the original sh z.
                ambient_padding.append(cpu.read16(regs[29]+30))
                cpu.write16(regs[29]+28,regs[3])
                return 0x8013D098
            patches[0x8013D094]=ambient_position
        # The same inline GTE boundary used by the animation unit checker.
        def wobble(cpu, regs):
            regs[6]=self.model('RendererWobbleGte',0,
                (regs[16],(regs[20]+16)&MASK,regs[17],0),True)&MASK
            return 0x80063974
        patches[0x80063958]=wobble
        def effect_object(cpu, regs):
            self.model('GpuDrawEffectObj',0,
                (regs[19],0x1F80018C,0x1F8001AC,0),True)
            regs[9]=cpu.read32(0x800EC354)
            return 0x8006FE1C
        patches[0x8006FBC4]=effect_object
        def project_model(cpu,regs):
            count=cpu.read16(cpu.read32(regs[6])+8)
            if count:
                self.model('ModelProjectVertices',0,(regs[2],count,0x1F800000,0x1F8003C0),True)
            return 0x80017E3C
        patches[0x80017DEC]=project_model
        def indicator(cpu,regs):
            kind=cpu.reads16(regs[17])
            words=(regs[17],0x800BBC54 if kind<2 else 0x800BBCC4,64 if kind==1 else 0,kind&MASK)
            self.model('RenderIndicator',0,words,True)
            return 0x80071C54
        def popup(cpu,regs):
            self.model('RenderHudPopup',0,(0x800E07C0,0x800F4350,cpu.read32(0x800EC350),regs[16]),True)
            return 0x800986C4
        patches.update({0x80071B6C:indicator,0x80098478:popup})
        # The ledge routine copies an unwritten fourth vector halfword from
        # its stack. Supply that observed input to the native local storage;
        # compare the resulting player bytes, rather than masking them.
        previous_coverage=self.cpu.coverage
        self.cpu.coverage=set()
        hooks = self.hooks()
        # The shared flash entry is a renderer boundary, not a gameplay port.
        hooks[0x80046990] = lambda cpu, regs: self.model('RenderScreenFlash', 0, (regs[4], 0, 0, 0), True)
        self.cpu.call(0x8005BFCC,hooks=hooks,patches=patches,initial_regs=initial)
        ledge_copy_executed=0x800641EC in self.cpu.coverage
        if previous_coverage is not None: previous_coverage.update(self.cpu.coverage)
        self.cpu.coverage=previous_coverage
        if ledge_copy_executed:
            reg.declare('shared_random','PlayerTryLedgeGrab direction padding',checker='check_game_area')
            self.lib.integration_set_direction_padding(self.shim,self.cpu.read16(0x800EC246))
        if ambient_padding:
            reg.declare('shared_random','Area02 ambient position padding',checker='check_game_area')
        padding=(C.c_uint16*len(ambient_padding))(*ambient_padding)
        self.lib.integration_set_spawn_padding(self.shim,padding,len(ambient_padding))
        if effect_matrix_padding:
            reg.declare('shared_random','EffectObjSpawn matrix unwritten tail',checker='check_game_area')
            assert len(set(effect_matrix_padding))==1, 'distinct matrix inputs need a per-call provider'
            C.memmove(self.ram+0x1FF0B2,effect_matrix_padding[0],14)
        self.lib.integration_frame(self.shim)
        self.lib.integration_call(self.shim,0x8001300C,0,0,0,0)
        self.cpu.call(0x8001300C,hooks=self.hooks(0x8001300C))
        self.compare(f'area {area:02X} frame {index}')
        if hasattr(self,'player_states'):self.player_states.add(self.cpu.read16(0x800EC23E))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--areas',nargs='+',type=lambda x:int(x,16),default=[1,2,5,0x44])
    parser.add_argument('--frames',type=int,default=90)
    parser.add_argument('--keep-going',action='store_true')
    parser.add_argument('--helpers-only',action='store_true')
    parser.add_argument('--host-hooks-only',action='store_true')
    parser.add_argument('--extended-hooks-only',action='store_true')
    parser.add_argument('--dispatch-only',action='store_true')
    parser.add_argument('--frame-branches-only',action='store_true')
    parser.add_argument('--pause-maths-only',action='store_true')
    parser.add_argument('--overlay-routes-only',action='store_true')
    parser.add_argument('--net-catch-only',action='store_true')
    args=parser.parse_args()
    overrides={}
    h=Harness(overrides)
    if args.net_catch_only:
        result=h.net_catch()
        (ROOT/'extract/mips/game_net_catch.json').write_text(json.dumps(result,indent=2))
        return 0
    if args.overlay_routes_only:
        h.direct_overlay_routes()
        return 0
    if args.pause_maths_only:
        h.direct_pause_math_hooks()
        return 0
    if args.frame_branches_only:
        h.direct_frame_branches()
        return 0
    if args.dispatch_only:
        h.direct_dispatch_boundaries()
        return 0
    if args.extended_hooks_only:
        h.direct_extended_hooks()
        return 0
    if args.host_hooks_only:
        h.direct_hud_hooks()
        h.direct_host_hooks()
        return 0
    if args.helpers_only:
        h.direct_helpers()
        return 0
    failures=[]
    player_states={}
    for area in args.areas:
        try:
            h.load(area)
            print(f'area {area:02X}: load matches')
            h.prepare_player()
            matched=0
            for index in range(args.frames):
                try:
                    h.frame(area,index)
                    matched+=1
                except (AssertionError,mr.MipsError) as error:
                    failures.append(str(error));print('FAIL',error)
                    if not args.keep_going:raise
                    # Keep the actual diverged states. Never copy differing
                    # game bytes from the oracle to make later frames pass.
            print(f'area {area:02X}: {matched}/{args.frames} frames match')
            print('  player states:',','.join(f'{state:02X}' for state in sorted(h.player_states)))
            player_states[f'{area:02X}']=sorted(h.player_states)
            if args.frames>=90:
                required={5,8,9,0x80} | ({0x81} if area in (1,2) else set())
                assert required.issubset(h.player_states),'script must move, jump and start the net; open areas must complete its swing'
                if area==5:
                    assert {0x24,0x25,0x80,0x81,0x83}.issubset(h.player_states), 'original stage-entry position must fall, land and complete the net swing'
                if area==0x44:
                    assert {0x24,0x25,0x80,0x81,0x83}.issubset(h.player_states), 'original stage-entry position must fall, land and complete the net swing'
        except (AssertionError,mr.MipsError) as error:
            failures.append(str(error));print('FAIL',error)
            if not args.keep_going:raise
    net_catch=h.net_catch()
    print(f'{"FAIL" if failures else "OK"}: {h.cases} completed integrated comparisons; {len(failures)} failures')
    (ROOT/'extract/mips/game_area_results.json').write_text(json.dumps(dict(diagnostic=False,
        areas=args.areas,frames=args.frames,comparisons=h.cases,failures=failures,
        original_playerupdate_intercepted=False,player_states=player_states,net_catch=net_catch,
        sound_banks='unloaded boot slots',pad_script='move / jump hold and release / net flick'),indent=2))
    return bool(failures)


if __name__=='__main__':sys.exit(main())

