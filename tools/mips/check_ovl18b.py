"""Differential checks for the readable special-area overlay member 18B ports."""
import argparse
from collections import Counter
import ctypes as C
import random
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_pausemenu as pmcheck  # noqa: E402

U, I, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x801FEF04
OVERLAY_BASE, OVERLAY_SIZE = 0x80136000, 0x3000
WORK = 0x801C0000
UI, OBJECTS = 0x80138CA0, 0x80138D00
FRAME, CURSOR, OT_INDEX = 0x800EC350, 0x800EC354, 0x800EC35C
OT_BASE, OT_STRIDE = 0x800F03CC, 0x4078
VIEW_MATRIX, SCENE_MODE = 0x800E78C0, 0x80138CE8
FRAME_MODEL_INSTANCES, MODEL_DRAW_BASE = FRAME + 0x80, 0x800E7D00
SCAN_RECORDS, SCRIPT_END = 0x80138B7C, 0x3FC0

# name: address, byte-size, C symbol, return-kind, argument-kinds
FUNCS = {
    "StageClearUi_LookAt_18B": (0x80136000, 0x1D8, "ae_StageClearUi_LookAt_18B", "v", "uu"),
    "SaveUi_StringTableIndex_18B": (0x801361D8, 0x30, "ae_SaveUi_StringTableIndex_18B", "u", "us"),
    "Mode_SpecialAreaEnter": (0x80136208, 0x74C, "ae_Mode_SpecialAreaEnter", "s", ""),
    "SpecialEnterUi_DrawWipe": (0x80136954, 0x8C0, "ae_SpecialEnterUi_DrawWipe", "s", "u"),
    "SpecialEnterUi_DrawText": (0x80137214, 0x120, "ae_SpecialEnterUi_DrawText", "v", "u"),
    "SpecialEnterUi_DrawMessageShadow": (0x80137334, 0x70, "ae_SpecialEnterUi_DrawMessageShadow", "v", "u"),
    "SpecialEnterUi_DrawBlink": (0x801373A4, 0x12C, "ae_SpecialEnterUi_DrawBlink", "v", ""),
    "SpecialEnterUi_RunScript": (0x801374D0, 0x804, "ae_SpecialEnterUi_RunScript", "v", ""),
    "StageClearUi_SetDrawStp_18B": (0x80137CD4, 0x78, "ae_StageClearUi_SetDrawStp_18B", "v", ""),
    "StageClearUi_CopyStrip_18B": (0x80137D4C, 0xA4, "ae_StageClearUi_CopyStrip_18B", "v", "s"),
    "SpecialEnterUi_UpdateObjects": (0x80137DF0, 0x2E8, "ae_SpecialEnterUi_UpdateObjects", "v", "u"),
    "SpecialEnterUi_DrawScene": (0x801380D8, 0x2E0, "ae_SpecialEnterUi_DrawScene", "v", ""),
    "SpecialEnterUi_SetAreaTablePointer": (0x801383B8, 0x20, "ae_SpecialEnterUi_SetAreaTablePointer", "v", ""),
    "SpecialEnterUi_TextWidth": (0x801383D8, 0x90, "ae_SpecialEnterUi_TextWidth", "s", "u"),
    "SpecialEnterUi_PlayXa": (0x80138468, 0x28, "ae_SpecialEnterUi_PlayXa", "v", ""),
    "StageClearUi_DrawScanlines_18B": (0x80138490, 0x3E4, "ae_StageClearUi_DrawScanlines_18B", "v", ""),
}
EXTRA_COVERAGE = {}
NO_HOOKS = {"SaveUi_StringTableIndex_18B", "SpecialEnterUi_SetAreaTablePointer"}

# Calls left at the SDK, archive, sound, text, or renderer boundary.
API = {
    0x8005CFEC: ("InitDisplay", 0), 0x80028350: ("ResetGraph", 1),
    0x80028850: ("ClearImage", 4), 0x8002FAF0: ("SetGeomScreen", 1),
    0x80011194: ("ArcLoadEntry", 1), 0x80011104: ("LoadListStart", 2),
    0x800116C0: ("LoadListStep", 0), 0x8001231C: ("CdMgrStopStream", 0),
    0x800125B8: ("CdMgrUpdate", 0), 0x8005DF48: ("BeginFrame", 0),
    0x800286CC: ("DrawSync", 1), 0x80028CC0: ("DrawOTag", 1),
    0x80028D30: ("PutDrawEnv", 1), 0x80028EFC: ("PutDispEnv", 1),
    0x8008DDB0: ("TextSetCursor", 2), 0x8008DE1C: ("TextLineWidth", 1),
    0x80011F6C: ("XaPlay", 1), 0x80015C34: ("MeshInit", 2),
    0x80016A60: ("ModelTransformSkinned", 1), 0x80017CD4: ("ModelDraw", 3),
    0x8002B380: ("DrawMove", 4), 0x8002B3E0: ("SetDrawStp", 2),
    0x800305B0: ("ScaleMatrixL", 2), 0x800315E0: ("SetBackColor", 3),
    0x80031600: ("SetColorMatrix", 1), 0x80031630: ("SetFarColor", 3),
    0x80086170: ("SoundStopAndRestoreVolume", 0),
    0x80016F34: ("DrawAreaProps", 1), 0x8008DD68: ("TextNextGlyph", 4),
    0x8008D9D0: ("FontDrawGlyph", 4), 0x800A342C: ("SoundStopAllVoices", 1),
    0x8002E520: ("VSync", 1), 0x800A1F3C: ("SeqTrackClose", 1),
    0x800A2B6C: ("SeqTrackSetPlayState", 2), 0x800A1CCC: ("SoundBankFree", 1),
    0x8002B52C: ("PadGetState", 1), 0x8002B5F8: ("PadInfoMode", 3),
    0x80012CA0: ("PadSetupDualShock", 1), 0x80013128: ("PadDemoPlaybackStep", 0),
    0x8005D100: ("StageRequest", 3), 0x800140B4: ("StickToButtons", 0),
    0x80014208: ("PadRumbleReset", 1), 0x8002D3B0: ("SetBackColor", 3),
    0x8002D3D0: ("SetFarColor", 3), 0x8002D3F0: ("SetColorMatrix", 1),
}

HOOK = C.CFUNCTYPE(I, P, P, C.c_char_p, U, U, C.POINTER(U))

class OvlHooks(C.Structure):
    _fields_ = [("user", P), ("stack_pointer", U), ("area", P), ("pad", P),
                ("items", P), ("stage_fade", P), ("pause_menu", P),
                ("native_call", HOOK), ("renderer", HOOK)]

class PadHooks(C.Structure):
    _fields_ = [("user", P), ("pad_get_state", C.CFUNCTYPE(I,P,P,I)),
                ("pad_info_mode", C.CFUNCTYPE(I,P,P,I,I,I)),
                ("pad_setup_dualshock", C.CFUNCTYPE(None,P,P,I)),
                ("pad_demo_step", C.CFUNCTYPE(I,P,P)),
                ("stage_request", C.CFUNCTYPE(None,P,P,I,I,I)),
                ("stick_to_buttons", C.CFUNCTYPE(U,P,P)),
                ("pad_rumble_reset", C.CFUNCTYPE(None,P,P,I))]

class ItemsHooks(C.Structure):
    _fields_ = [("user", P), ("GetLevelFirstArea", P), ("printf", P),
                ("SetBackColor", C.CFUNCTYPE(None,P,P,U,U,U)),
                ("SetFarColor", C.CFUNCTYPE(None,P,P,U,U,U)),
                ("SetColorMatrix", C.CFUNCTYPE(None,P,P,U)),
                ("rsin", C.CFUNCTYPE(I,P,P,I)), ("rcos", C.CFUNCTYPE(I,P,P,I))]

class SoundHw(C.Structure):
    _fields_ = [("user", P), ("wait_idle", C.CFUNCTYPE(I,P,P)),
                ("wait_loop", C.CFUNCTYPE(None,P,P,U)),
                ("set_voice_adsr", C.CFUNCTYPE(None,P,P,U,C.c_uint16,C.c_uint16)),
                ("voice_key_off", C.CFUNCTYPE(None,P,P,U,C.c_uint16)),
                ("printf_error", C.CFUNCTYPE(None,P,P,U,U,I,I,I)),
                ("set_voice_volume", C.CFUNCTYPE(None,P,P,U,U,C.c_uint16))]

class StageHooks(C.Structure):
    pass
_stage_text = (ROOT / "src/world/ae_stage.h").read_text(encoding="utf-8")
_stage_body = _stage_text.split("typedef struct AeStageHooks",1)[1].split("} AeStageHooks;",1)[0]
_stage_cb = C.CFUNCTYPE(I,P,P,U,U,U,U)
StageHooks._fields_ = [("user",P),("stack_pointer",U)] + [
    (name,_stage_cb) for name in re.findall(r"AeStageHookFn\s+(\w+);",_stage_body)]

class StageFadeHooks(C.Structure):
    _fields_ = [("user",P),("stack_pointer",U),("caller_a3",U),
                ("stage",C.POINTER(StageHooks)),("sound_hw",C.POINTER(SoundHw))] + [
                    (name,_stage_cb) for name in ("DebugTextPrintf","SeqTrackClose",
                    "SeqTrackSetPlayState","SoundBankFree","SoundStopAllVoices",
                    "RenderScreenFlash","RenderFadeSpiral")]

class AreaHooks(C.Structure):
    pass
_area_text = (ROOT / "src/world/ae_areaset.h").read_text(encoding="utf-8")
_area_body = _area_text.split("typedef struct AeAreaSetHooks",1)[1].split("} AeAreaSetHooks;",1)[0]
_area_cb = C.CFUNCTYPE(I,P,P,U,U,U,U)
AreaHooks._fields_ = [("user",P),("body_hooks",P)] + [
    (name,_area_cb) for name in re.findall(r"AeAreaSetHookFn\s+(\w+);",_area_body)]

PauseHooks, PauseCb = pmcheck.make_hooks_type()

class MipsMem:
    def __init__(self,cpu): self.cpu=cpu
    def rd8(self,a): return self.cpu.read8(a)
    def rd16(self,a): return self.cpu.read16(a)
    def rd32(self,a): return self.cpu.read32(a)
    def wr8(self,a,v): self.cpu.write8(a,v)
    def wr16(self,a,v): self.cpu.write16(a,v)
    def wr32(self,a,v): self.cpu.write32(a,v)
    def write_bytes(self,a,b): self.cpu.write_bytes(a,b)

class CMem:
    def __init__(self,dll,ram): self.dll,self.ram=dll,ram
    def rd8(self,a): return self.dll.ae_rd8(self.ram,a)
    def rd16(self,a): return self.dll.ae_rd16(self.ram,a)
    def rd32(self,a): return self.dll.ae_rd32(self.ram,a)
    def wr8(self,a,v): self.dll.ae_wr8(self.ram,a,v)
    def wr16(self,a,v): self.dll.ae_wr16(self.ram,a,v)
    def wr32(self,a,v): self.dll.ae_wr32(self.ram,a,v)
    def write_bytes(self,a,b):
        for i,x in enumerate(b): self.wr8(a+i,x)

class CallModel:
    def __init__(self, profile): self.profile=profile; self.mem=None; self.events=[]; self.load_steps=0; self.pad_polls=0
    def call(self,name,args):
        args=tuple(int(x)&0xFFFFFFFF for x in args)
        self.events.append((name,args))
        if name=="PadGetState":
            self.pad_polls+=1
            if self.profile.get("press_when_ui_state") is not None:
                pressed=(self.mem.rd16(UI+0x28)==self.profile["press_when_ui_state"] and
                         self.pad_polls>=self.profile.get("press_after_polls",1))
                self.mem.wr8(0x800E52D2,0xf7 if pressed else 0xff)
                self.mem.wr8(0x800E52D3,0x9f if pressed else 0xff)
            return 0
        if name=="TextLineWidth":
            width=0
            for i in range(1024):
                ch=self.mem.rd8(args[0]+i)
                if ch==0 or ch==13: break
                width += self.mem.rd8(0x800B3640+ch)
            return width
        if name=="TextNextGlyph":
            glyph=self.mem.rd8(args[0])
            self.mem.wr16(args[1],glyph)
            return (args[0]+1)&0xFFFFFFFF
        if name=="LoadListStep":
            self.load_steps+=1
            if "load_step_result" in self.profile:
                return int(self.profile["load_step_result"])
            if self.profile.get("load_done_after") and self.load_steps>=self.profile["load_done_after"]: return 1
            return 0
        if name=="DrawText" and len(args)>0: return 0
        if name=="RendererLinkOtag":
            tag,packet=args
            link=self.mem.rd8(tag)|(self.mem.rd8(tag+1)<<8)|(self.mem.rd8(tag+2)<<16)
            for i in range(3):
                self.mem.wr8(tag+i,packet>>(8*i))
                self.mem.wr8(packet+i,link>>(8*i))
            return 0
        if name in ("RendererBuildScanlineBase","SpecialEnterUi_DrawWipePackets","SpecialEnterUi_DrawGlyph"):
            return 0
        return int(self.profile.get("returns",{}).get(name,0))

class CPort:
    def __init__(self,dll):
        self.dll=C.CDLL(str(dll)); d=self.dll
        d.ae_ram_create.restype=P; d.ae_ptr.argtypes=[P,U,U]; d.ae_ptr.restype=P
        for n,t in (("ae_rd8",U),("ae_rd16",U),("ae_rd32",U)):
            getattr(d,n).argtypes=[P,U]; getattr(d,n).restype=t
        for n in ("ae_wr8","ae_wr16","ae_wr32"): getattr(d,n).argtypes=[P,U,U]
        d.ae_ram_clear_error.argtypes=[P]; d.ae_ram_failed.argtypes=[P]; d.ae_ram_failed.restype=I
        d.ae_ram_error.argtypes=[P]; d.ae_ram_error.restype=C.c_char_p
        self.ram=d.ae_ram_create()
        self.main=(C.c_uint8*0x200000).from_address(d.ae_ptr(self.ram,0x80000000,0x200000))
        self.scratch=(C.c_uint8*0x400).from_address(d.ae_ptr(self.ram,0x1F800000,0x400))
        self.funcs={}
        for name,(_addr,_size,symbol,ret,args) in FUNCS.items():
            fn=getattr(d,symbol)
            fn.argtypes=([P]+([] if name in NO_HOOKS else [C.POINTER(OvlHooks)])+
                        [I if k=="s" else U for k in args])
            fn.restype={"v":None,"s":I,"u":U}[ret]
            self.funcs[name]=fn
        self.callbacks=[]
        self.hooks=OvlHooks(); self.area=AreaHooks(); self.pad=PadHooks()
        self.items=ItemsHooks(); self.sound=SoundHw(); self.stage=StageHooks()
        self.stage_fade=StageFadeHooks(); self.pause=PauseHooks()
        self.model=None
    def sync(self,snapshot):
        C.memmove(self.main,snapshot[0],0x200000); C.memmove(self.scratch,snapshot[1],0x400)
        self.dll.ae_ram_clear_error(self.ram)

class Harness:
    def __init__(self,dll):
        reg.declare("stack_window","801FF000..80200000")
        self.area_files=dump_areas.area_members()
        overlay=(ROOT/"extract/members/18B.bin").read_bytes()
        self.coverage=set()
        self.cpu=mr.MipsRam.from_extract(ROOT/f"extract/members/{self.area_files[2]:03X}.bin")
        self.cpu.load_area(overlay,OVERLAY_BASE); self.cpu.attach_gte_oracle(); self.cpu.coverage=self.coverage
        self.gte_backend=self.cpu.gte_backend
        self.cpu_cache={2:self.cpu}; self.base_cache={2:self.cpu.snapshot()}
        self.c=CPort(dll); self.counts={n:0 for n in FUNCS}
        self.failures=0; self.last_scenario={}
        d=self.c.dll
        self.cmem=CMem(d,self.c.ram)
        self._bind_callbacks()

    def _bind_callbacks(self):
        c=self.c
        def make_callback(label,arity=4,ret=I):
            def call(_u,ram,*args):
                model=self.active_c_model
                return model.call(label,list(args[:arity]))
            cb=C.CFUNCTYPE(ret,*([P,P]+[I if label in ("PadGetState",) and i==0 else U for i in range(arity)]))(call)
            c.callbacks.append(cb); return cb
        # Main overlay hook table.
        def native(_u,ram,name,address,argc,words):
            return self.active_c_model.call(name.decode("ascii"),[words[i] for i in range(argc)])
        c.native=HOOK(native); c.renderer=HOOK(native); c.callbacks += [c.native,c.renderer]
        c.hooks.user=None; c.hooks.stack_pointer=mr.DEFAULT_SP
        c.hooks.area=C.cast(C.pointer(c.area),P); c.hooks.pad=C.cast(C.pointer(c.pad),P)
        c.hooks.items=C.cast(C.pointer(c.items),P); c.hooks.stage_fade=C.cast(C.pointer(c.stage_fade),P)
        c.hooks.pause_menu=C.cast(C.pointer(c.pause),P)
        c.hooks.native_call=c.native; c.hooks.renderer=c.renderer
        # Pad callbacks have the original API argument words.
        self._set_cb(c.pad,"pad_get_state",lambda u,r,port:self.active_c_model.call("PadGetState",[port]),I,P,P,I)
        self._set_cb(c.pad,"pad_info_mode",lambda u,r,port,a,b:self.active_c_model.call("PadInfoMode",[port,a,b]),I,P,P,I,I,I)
        self._set_cb(c.pad,"pad_setup_dualshock",lambda u,r,a:self.active_c_model.call("PadSetupDualShock",[a]),None,P,P,I)
        self._set_cb(c.pad,"pad_demo_step",lambda u,r:self.active_c_model.call("PadDemoPlaybackStep",[]),I,P,P)
        self._set_cb(c.pad,"stage_request",lambda u,r,a,b,d:self.active_c_model.call("StageRequest",[a,b,d]),None,P,P,I,I,I)
        self._set_cb(c.pad,"stick_to_buttons",lambda u,r:self.active_c_model.call("StickToButtons",[]),U,P,P)
        self._set_cb(c.pad,"pad_rumble_reset",lambda u,r,a:self.active_c_model.call("PadRumbleReset",[a]),None,P,P,I)
        # Item color SDK hooks.
        for field,label in (("SetBackColor","SetBackColor"),("SetFarColor","SetFarColor")):
            self._set_cb(c.items,field,lambda u,r,a,b,d,label=label:self.active_c_model.call(label,[a,b,d]),None,P,P,U,U,U)
        self._set_cb(c.items,"SetColorMatrix",
                     lambda u,r,matrix:self.active_c_model.call("SetColorMatrix",[matrix]),
                     None,P,P,U)
        # Sound and StageFade boundaries. Test effects begin with absent banks.
        self._set_cb(c.sound,"wait_idle",lambda u,r:0,I,P,P)
        self._set_cb(c.sound,"wait_loop",lambda u,r,n:None,None,P,P,U)
        self._set_cb(c.sound,"set_voice_adsr",lambda u,r,ch,a,b:None,None,P,P,U,C.c_uint16,C.c_uint16)
        self._set_cb(c.sound,"voice_key_off",lambda u,r,h,m:None,None,P,P,U,C.c_uint16)
        self._set_cb(c.sound,"printf_error",lambda u,r,f,n,a,b,d:None,None,P,P,U,U,I,I,I)
        self._set_cb(c.sound,"set_voice_volume",lambda u,r,ch,side,v:None,None,P,P,U,U,C.c_uint16)
        c.stage_fade.stage=C.pointer(c.stage); c.stage_fade.sound_hw=C.pointer(c.sound)
        stage_fade_arities={"DebugTextPrintf":4,"SeqTrackClose":1,
                            "SeqTrackSetPlayState":2,"SoundBankFree":1,
                            "SoundStopAllVoices":1,"RenderScreenFlash":4,
                            "RenderFadeSpiral":4}
        for field,arity in stage_fade_arities.items():
            self._set_cb(c.stage_fade,field,
                         lambda u,r,a,b,d,e,label=field,n=arity:
                             self.active_c_model.call(label,[a,b,d,e][:n]),
                         I,P,P,U,U,U,U)
        # StageFade uses the stage VSync hook through this nested record.
        self._set_cb(c.stage,"VSync",lambda u,r,a,b,d,e:self.active_c_model.call("VSync",[a]),I,P,P,U,U,U,U)
        # Pause menu text path, reached directly by DrawMessageShadow.
        self._set_cb(c.pause,"TextNextGlyph",lambda u,r,a,b,d,e:self.active_c_model.call("TextNextGlyph",[a,b,d,e]),I,P,P,U,U,U,U)
        self._set_cb(c.pause,"FontDrawGlyph",lambda u,r,a,b,d,e:self.active_c_model.call("FontDrawGlyph",[a,b,d,e]),I,P,P,U,U,U,U)
        c.pause.stack_pointer=mr.DEFAULT_SP
        # Empty callback records are sufficient for area functions on tested members.

    def _set_cb(self,obj,field,fn,restype,*argtypes):
        cb=C.CFUNCTYPE(restype,*argtypes)(fn); setattr(obj,field,cb); self.c.callbacks.append(cb)

    def _mips_hooks(self,model):
        hooks={}
        for addr,(name,arity) in API.items():
            def invoke(cpu,regs,label=name,n=arity,m=model):
                args=list(regs[4:8])[:n]
                if n>4: args.extend(cpu.read32(regs[29]+16+4*i) for i in range(n-4))
                return m.call(label,args)
            hooks[addr]=invoke
        # DrawScene copies the base matrix before handing the GTE/GPU body to the renderer.
        def draw_scene(cpu,regs,m=model):
            frame_index=cpu.read32(OT_INDEX)
            args=[2248,VIEW_MATRIX,regs[29]+16,
                  FRAME_MODEL_INSTANCES+frame_index*OT_STRIDE,
                  MODEL_DRAW_BASE,cpu.read32(SCENE_MODE),OBJECTS,
                  OT_BASE+frame_index*OT_STRIDE,cpu.read32(CURSOR)]
            m.call("SpecialEnterUi_RenderScene",args)
            return 0x8013839C
        # DrawText's in-loop packet setup is supplied by the host renderer.
        def draw_glyph(cpu,regs,m=model):
            ui=UI; glyph=regs[6]&0xff; cursor=regs[8]
            args=[cursor,glyph,regs[7],regs[11],regs[10],0x80138B90+glyph,
                  cpu.read16(ui+4),cpu.read16(ui+6),cpu.read32(ui+8)]
            m.call("SpecialEnterUi_DrawGlyph",args)
            return 0x801372E8
        # Wipe packet blocks are whole native draw phases. Keep the state/timer logic after each block in MIPS.
        def wipe_in(cpu,regs,m=model):
            state=cpu.read16(0x801389B0); cursor=cpu.read32(CURSOR)
            idx=cpu.read32(OT_INDEX)
            args=[state,cursor,idx,cpu.read16(UI+0x28),cpu.read16(UI+0x2A),OBJECTS]
            m.call("SpecialEnterUi_DrawWipePackets",args)
            regs[5]=(cursor+80)&0xffffffff
            regs[11]=UI
            return 0x80136E70
        def wipe_out(cpu,regs,m=model):
            state=cpu.read16(0x801389B0); cursor=cpu.read32(CURSOR)
            idx=cpu.read32(OT_INDEX)
            args=[state,cursor,idx,cpu.read16(UI+0x28),cpu.read16(UI+0x2A),OBJECTS]
            m.call("SpecialEnterUi_DrawWipePackets",args)
            regs[7]=(cursor+240)&0xffffffff
            regs[15]=UI
            return 0x801371C4
        def scanline_base(cpu,regs,m=model):
            m.call("RendererBuildScanlineBase",[cpu.read32(CURSOR),cpu.read32(FRAME),
                cpu.read32(OT_INDEX),cpu.read32(FRAME+8)])
            regs[16]=(cpu.read32(CURSOR)+120*168)&0xffffffff
            return 0x801385C0
        def link(tag,packet,next_pc):
            def apply(cpu,regs,m=model):
                m.call("RendererLinkOtag",[tag(cpu,regs),regs[16]])
                return next_pc
            return apply
        patches={0x801381D8:draw_scene,0x80137258:draw_glyph,
                 0x80136CB8:wipe_in,0x80136F3C:wipe_out,
                 0x80138508:scanline_base,
                 0x80137D04:link(lambda cpu,r:OT_BASE+cpu.read32(OT_INDEX)*OT_STRIDE,0,0x80137D30),
                 0x80137DA0:link(lambda cpu,r:OT_BASE+cpu.read32(OT_INDEX)*OT_STRIDE,0,0x80137DD4),
                 0x801374A0:link(lambda cpu,r:r[17],0,0x801374B0),
                 0x801387DC:link(lambda cpu,r:r[18],0,0x801387EC),
                 0x80138818:link(lambda cpu,r:r[18],0,0x80138828)}
        return hooks,patches

    def _args(self,name,scenario,mem):
        ptr=WORK+0x400
        if name=="StageClearUi_LookAt_18B":
            for i,v in enumerate((scenario*31-90,scenario*13+20,500,scenario*17+60,-100,900)):
                mem.wr16(ptr+i*2,v)
            return [ptr,ptr+8]
        if name=="SaveUi_StringTableIndex_18B":
            mem.write_bytes(ptr,b"one\0two\0three\0\0")
            return [ptr,scenario%6-1]
        if name=="Mode_SpecialAreaEnter": return []
        if name=="SpecialEnterUi_DrawWipe": return [draw_context(mem)]
        if name in ("SpecialEnterUi_DrawText","SpecialEnterUi_DrawMessageShadow","SpecialEnterUi_TextWidth"):
            mem.write_bytes(ptr,b"AB\rCD\0")
            for ch in b"ABCD": mem.wr8(0x80138B90+ch,6+ch%4)
            for ch in b"ABCD": mem.wr8(0x800B3640+ch,5+ch%7)
            return [ptr]
        if name=="SpecialEnterUi_RunScript":
            return []
        if name=="StageClearUi_CopyStrip_18B": return [scenario%7-2]
        if name=="SpecialEnterUi_UpdateObjects": return [OBJECTS]
        return []

    def setup(self,name,scenario,mem,profile):
        rng=random.Random(0x18B000+scenario*43+list(FUNCS).index(name))
        for i in range(STACK_HI-STACK_LO): mem.wr8(STACK_LO+i,0)
        mem.wr32(FRAME,scenario); mem.wr32(CURSOR,WORK+0x1000)
        mem.wr32(FRAME+8,WORK+0x7000); mem.wr32(OT_INDEX,scenario%2)
        mem.wr32(UI+8,WORK+0x7000); mem.wr32(UI+0x0C,32)
        mem.wr16(UI+4,0x3FDF); mem.wr16(UI+6,38)
        ui_timer=(10 if scenario==1 else 0 if scenario in (3,7) else scenario)
        mem.wr16(UI+0x28,scenario%3); mem.wr16(UI+0x2A,ui_timer)
        mem.wr16(UI+0x2C,scenario%2); mem.wr32(UI+0x48,7)
        mem.wr32(UI+0x58,0); mem.wr32(UI+0x5C,0)
        wipe_states=(0,1,2,3,4,0,0,3)
        mem.wr16(0x801389B0,wipe_states[scenario%8]); mem.wr16(0x80138B8E,120)
        mem.wr16(0x80138B8C,1 if scenario==6 else 0); mem.wr32(0x80138C90,scenario%12)
        mem.wr16(0x80138CFC,(6,7,21,99,5,100,15,30)[scenario%8])
        mem.wr16(0x800B2D08+172*8,-1); mem.wr16(0x800B2D08+173*8,-1)
        for i in range(12): mem.wr16(0x800BF670+i*2,-1)
        # Pad packets default to present, digital, with no pressed buttons.
        mem.write_bytes(0x800E52D0,bytes([0,0x41,0xff,0xff,0x80,0x80,0x80,0x80]))
        mem.write_bytes(0x800E52F2,bytes([0xff,0,0xff,0xff,0x80,0x80,0x80,0x80]))

        # Screen-state mode selector and the active area index come from the real member header.
        mem.wr16(0x800F4476,(2,6,15,20,30,53,8,44)[scenario%8])
        # Script fixtures exercise every opcode family with valid inline operands.
        script=WORK+0x100
        script_cases=[
            [(0x0000,0,[1]),(0x0040,0,[20,-10,30,4]),(0x0080,0,[4,8,-6,4]),
             (0x0140,0,[1]),(0x0180,0,[0]),(0x00C0,0,[2]),(0x0100,0,[3]),(0x03C0,0,[])],
            [(0x0040,16,[200,-50,300]),(0x0040,17,[0x100,0x200,0x300]),
             (0x0040,18,[0x400,0x500,0x600,3]),(0x0040,19,[-20,40,-60,3])],
            [(0x01C0,0,[2]),(0x0200,0,[])],
            [(0x0300,0,[]),(0x0340,0,[])],
            [(0x0240,0,[]),(0x00C0,0,[2]),(0x0280,0,[])],
            [(0x02C0,0,[-2]),(0x03C0,0,[])],
            [(0x0000,2,[1]),(0x0040,2,[-40,60,80,2]),(0x0080,2,[32,-16,64,2]),
             (0x0140,2,[1]),(0x0180,2,[1])],
            [(0x0340,0,[]),(0x0380,0,[])],
            [(0x0400,0,[])],
            [(0x0340,0,[]),(0x0380,0,[])],
            [],
            [],
            [],
            [],
            [],
        ]
        words=[]
        for opcode,index,operands in script_cases[scenario%len(script_cases)]:
            words.append(opcode|index)
            words.extend(value&0xffff for value in operands)
        script_end_marker=script+len(words)*2
        for offset,word in enumerate(words): mem.wr16(script+offset*2,word)
        mem.wr16(script_end_marker,SCRIPT_END)
        mem.wr32(UI+0x20,script); mem.wr32(UI+0x24,script_end_marker)
        mem.wr32(0x80138CEC,1)
        # Disable scanline text rows unless this scenario is explicitly toggling one.
        for i in range(12):
            rec=SCAN_RECORDS+i*8
            mem.wr16(rec,16); mem.wr16(rec+2,32); mem.wr16(rec+4,0)
            mem.wr8(rec+6,3); mem.wr8(rec+7,0xff)
        mem.wr32(0x80138AAC,WORK+0x400)
        # Model slots are quiescent in base cases; dedicated active cases can be added by profiles.
        for i in range(5):
            o=OBJECTS+i*0x88
            for j in range(0x88): mem.wr8(o+j,0)
        if name=="SpecialEnterUi_RunScript":
            mem.wr32(UI+0x48,6)
            mem.wr16(UI+0x3A,3 if scenario==1 else 0)
            mem.wr16(UI+0x42,3 if scenario==1 else 0)
            for index in (0,2):
                obj=OBJECTS+index*0x88
                table=WORK+0x600+index*0x40
                mem.wr32(obj+0x74,table)
                mem.wr16(table,0); mem.wr16(table+2,8)
                mem.wr16(table+4,10); mem.wr16(table+6,20)
            if scenario==0:
                entries=WORK+0x500
                mem.wr32(UI+0x58,1); mem.wr32(UI+0x30,entries); mem.wr32(UI+0x5C,0)
                mem.wr32(entries,2)
                mem.wr16(entries+6,0); mem.wr16(entries+14,3)
            elif scenario==2:
                mem.wr32(UI+0x44,0)
            elif scenario==3:
                mem.wr32(UI+0x44,6)
            elif scenario==7:
                entries=WORK+0x500
                mem.wr32(UI+0x58,1); mem.wr32(UI+0x30,entries); mem.wr32(UI+0x5C,0)
                mem.wr32(entries,2)
                mem.wr16(entries+6,2); mem.wr16(entries+14,3)
                mem.wr32(UI+0x44,4)
            elif scenario==9:
                entries=WORK+0x500
                mem.wr32(UI+0x58,1); mem.wr32(UI+0x30,entries); mem.wr32(UI+0x5C,5)
                mem.wr32(entries,2)
                mem.wr16(entries+6,0); mem.wr16(entries+14,1)
            elif scenario in (10,11,12,13,14):
                record=SCAN_RECORDS
                mem.wr16(record,120); mem.wr16(record+2,24); mem.wr16(record+4,0)
                mem.wr8(record+6,0xff if scenario==14 else scenario-10); mem.wr8(record+7,1)
        if name=="SpecialEnterUi_UpdateObjects":
            mem.wr32(UI+0x48,7)
            for index in range(3):
                obj=OBJECTS+index*0x88
                model=WORK+0x8000+index*0x40
                bones=WORK+0x9000+index*0x100
                matrices=WORK+0xA000+index*0x100
                animation=WORK+0xB000+index*0x100
                table=WORK+0xC000+index*0x100
                mem.wr32(obj,model); mem.wr16(model+4,0)
                mem.wr32(obj+0x0C,bones); mem.wr32(obj+0x14,matrices)
                mem.wr32(obj+0x18,animation); mem.wr32(animation,0)
                mem.wr32(obj+0x74,table)
                mem.wr16(table,0); mem.wr16(table+2,(0xFFFF if scenario==7 and index==2 else 2))
                mem.wr16(table+4,10); mem.wr16(table+6,8)
                mem.wr16(obj+0x5E,0); mem.wr16(obj+0x70,1)
                mem.wr16(obj+0x5C,(0,3,10)[index])
                mem.wr16(obj+0x72,1)
                for axis,(current,target) in enumerate(((3,100),(-4,-80),(7,160))):
                    mem.wr16(obj+0x60+axis*2,current+index)
                    mem.wr16(obj+0x78+axis*2,target-index*4)
                rotation_left=(3,0,-2,1,4,2,1,3)[scenario%8]
                position_left=(4,-1,0,2,5,1,3,2)[scenario%8]
                mem.wr16(obj+0x7E,rotation_left if index!=1 else 0)
                mem.wr16(obj+0x86,position_left if index!=2 else 0)
                for axis,(source,target) in enumerate(((20,-40),(-30,60),(50,80))):
                    mem.wr32(obj+0x24+axis*4,source+index*8)
                    mem.wr16(obj+0x80+axis*2,target-index*4)
        if name=="SpecialEnterUi_DrawWipe":
            state=wipe_states[scenario%8]; mem.wr16(0x801389B0,state)
            if state==0:
                mem.wr16(0x80138B8E,0 if scenario==6 else 120)
                mem.wr16(0x80138B8C,1 if scenario==6 else 0)
                if scenario==0: mem.wr32(UI+0x48,6)
                if scenario==5: mem.wr32(UI+0x48,7)
                if scenario==6: profile["load_step_result"]=-1
                mem.wr32(UI+0x20,script_end_marker); mem.wr16(script_end_marker,SCRIPT_END)
        if name=="SpecialEnterUi_DrawText" and scenario==7: mem.wr32(UI+0x0C,0xFFFFFFE0)
        if name=="SpecialEnterUi_DrawScene":
            for word in range(8):
                offset=word*4
                mem.wr32(0x800AF0A0+offset,0x12340000+(scenario<<8)+word)
                mem.wr32(VIEW_MATRIX+offset,0xA5A50000+(scenario<<8)+word)
        if name=="SpecialEnterUi_DrawBlink": mem.wr32(0x80138CFC,(6,7,21,99,5,100,15,30)[scenario%8])
        if name=="StageClearUi_DrawScanlines_18B": mem.wr32(0x80138C90,(0,1,10,11,20,0,5,12)[scenario%8])
        if name=="SpecialEnterUi_UpdateObjects": mem.wr32(UI+0x48,7)
        if name=="Mode_SpecialAreaEnter":
            mem.wr16(0x801389B0,0); mem.wr16(0x80138B8E,120); mem.wr16(0x80138B8C,0)
            mem.wr32(UI+0x20,script_end_marker); mem.wr16(script_end_marker,SCRIPT_END)
            profile["load_done_after"]=2
            profile["press_when_ui_state"]=2
            if scenario==7:
                profile["press_after_polls"]=14
                mem.wr32(CURSOR,0x80180000)
        args=self._args(name,scenario,mem)
        return args

    def run(self,name,scenario=0,selector=0,mutant=False):
        mode_areas=(98,92,93,98,92,93,98,92)
        if name=="Mode_SpecialAreaEnter":
            area_id=mode_areas[scenario%len(mode_areas)]
        elif name=="SpecialEnterUi_DrawWipe":
            area_id=98
        else:
            area_id=(1,2,3)[scenario%3]
        if area_id not in self.area_files: area_id=(1,2,3)[scenario%3]
        if area_id not in self.cpu_cache:
            cpu=mr.MipsRam.from_extract(ROOT/f"extract/members/{self.area_files[area_id]:03X}.bin")
            cpu.load_area((ROOT/"extract/members/18B.bin").read_bytes(),OVERLAY_BASE)
            cpu.gte_backend=self.gte_backend; cpu.coverage=self.coverage
            self.cpu_cache[area_id]=cpu; self.base_cache[area_id]=cpu.snapshot()
        self.cpu=self.cpu_cache[area_id]; self.cpu.coverage=self.coverage
        self.cpu.restore(self.base_cache[area_id]); mem=MipsMem(self.cpu)
        profile={}; args=self.setup(name,scenario,mem,profile)
        mem.wr32(MUTATION_SELECTOR,selector); mem.wr32(MUTATION_MARKER,0)
        before=self.cpu.snapshot()
        m=CallModel(profile); m.mem=mem
        cmodel=CallModel(profile)
        self.c.sync(before); cmodel.mem=self.cmem; self.active_c_model=cmodel
        hooks,patches=self._mips_hooks(m)
        address,size,symbol,ret,kinds=FUNCS[name]
        expected=None
        try: expected=self.cpu.call(address,args,hooks=hooks,patches=patches)
        except Exception as e:
            self.fail(name,scenario,f"MIPS execution failed: {e} state={getattr(self.cpu, '_last_machine', None)}"); return False
        cargs=[self.c.ram]
        if name not in NO_HOOKS: cargs.append(C.byref(self.c.hooks))
        cargs.extend(args)
        actual=self.c.funcs[name](*cargs)
        self.counts[name]+=1
        if ret!="v" and (int(actual)&0xffffffff)!=expected[2]:
            self.fail(name,scenario,f"return C={int(actual)&0xffffffff:08X} MIPS={expected[2]:08X}"); return False
        limit=STACK_LO-0x80000000
        got=bytes(self.c.main[:limit]); want=bytes(self.cpu.ram[:limit])
        if got!=want:
            off=next(i for i,(a,b) in enumerate(zip(got,want)) if a!=b)
            if os.environ.get("OVL18B_DEBUG"):
                print(f"  C error={self.c.dll.ae_ram_error(self.c.ram).decode(errors='replace') if self.c.dll.ae_ram_failed(self.c.ram) else None}",flush=True)
                print(f"  events C[{len(cmodel.events)}]={cmodel.events[-18:]!r} MIPS[{len(m.events)}]={m.events[-18:]!r}",flush=True)
                cc,mc=Counter(e[0] for e in cmodel.events),Counter(e[0] for e in m.events)
                print(f"  event count differences: { {k:(cc[k],mc[k]) for k in sorted(set(cc)|set(mc)) if cc[k]!=mc[k]} }",flush=True)
                print(f"  wipe calls C={[e[1] for e in cmodel.events if e[0]=='SpecialEnterUi_DrawWipePackets']!r}",flush=True)
                print(f"  wipe calls MIPS={[e[1] for e in m.events if e[0]=='SpecialEnterUi_DrawWipePackets']!r}",flush=True)
                diffs=[i for i,(a,b) in enumerate(zip(got,want)) if a!=b]
                print("  first RAM differences:",[(f"{0x80000000+i:08X}",got[i:i+8].hex(),want[i:i+8].hex()) for i in diffs[:24]],flush=True)
                print(f"  seed before={before[0][0xAE8F0:0xAE8F4].hex()} C={got[0xAE8F0:0xAE8F4].hex()} MIPS={want[0xAE8F0:0xAE8F4].hex()}",flush=True)
                for base,size,label in ((0x80138CB0,16,"camera"),(UI,0x60,"ui"),(OBJECTS,0x88,"object0"),(0x800E78C0,32,"view")):
                    off=base-0x80000000
                    print(f"  {label} C={got[off:off+size].hex()} MIPS={want[off:off+size].hex()}",flush=True)
            self.fail(name,scenario,f"RAM differs at {0x80000000+off:08X}: C={got[off:off+8].hex()} MIPS={want[off:off+8].hex()}"); return False
        if self.c.dll.ae_ram_failed(self.c.ram):
            self.fail(name,scenario,self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace")); return False
        if bytes(self.c.scratch)!=bytes(self.cpu.scratch): self.fail(name,scenario,"scratchpad differs"); return False
        if cmodel.events!=m.events:
            mismatch=next((i for i,(left,right) in enumerate(zip(cmodel.events,m.events)) if left!=right),min(len(cmodel.events),len(m.events)))
            self.fail(name,scenario,f"hook order differs at {mismatch}: C={cmodel.events[mismatch:mismatch+4]!r} MIPS={m.events[mismatch:mismatch+4]!r} (lengths {len(cmodel.events)}/{len(m.events)})"); return False
        return True

    def fail(self,name,scenario,msg):
        self.failures+=1; print(f"FAIL {name}[{scenario}]: {msg}",flush=True)

    def all(self,cases=8,names=None):
        self.coverage=set()
        for cpu in self.cpu_cache.values(): cpu.coverage=self.coverage
        passed=True
        selected = list(names) if names else list(FUNCS)
        for name in selected:
            self.counts[name]=0
            # The mode touches the full state machine and two distinct area profiles.
            n=cases+(7 if name=="SpecialEnterUi_RunScript" else 0)
            for scenario in range(n): passed=self.run(name,scenario) and passed
            print(f"{name}: {self.counts[name]} cases",flush=True)
        return passed

    def finish(self,extra_coverage=EXTRA_COVERAGE,default_run=False,show_coverage=False,names=None):
        selected = list(names) if names else list(FUNCS)
        items=[(name,FUNCS[name][0],FUNCS[name][1],True) for name in selected]
        items.extend((name,address,size,False) for name,(address,size) in extra_coverage.items())
        summary=reg.coverage_summary(items,self.coverage)
        if show_coverage: reg.print_coverage(summary)
        gaps={n:s for n,s in summary.items() if s["uncovered_count"] or s["stale_declared"]}
        if default_run: reg.write_results(self.counts,summary,not self.failures and not gaps,
            extra={"overlay":"18B.bin","areas":[1,2,3,92,93,94,95,96,97,98]},function_names=selected)
        for name,row in gaps.items():
            print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}",flush=True)
        return not gaps and not self.failures


def draw_context(mem): return FRAME+0x80+mem.rd32(OT_INDEX)*OT_STRIDE

def build(source=None,tag="ovl18b"):
    import check_game
    from game_call_inventory import ledger
    out=ROOT/"extract/mips"; out.mkdir(parents=True,exist_ok=True)
    dll=out/f"{tag}_{time.time_ns()}.dll"
    exports=dll.with_suffix(".def")
    modules={row["source"] for row in ledger()}
    modules.update(check_game.SOURCES)
    modules.discard("src/world/ae_stagefade.c")
    modules.discard("src/modes/ae_ovl18b.c")
    inputs=[str(ROOT/rel) for rel in sorted(modules)]
    inputs += [str(ROOT/"src/math/native_sdk_apply.c"),str(ROOT/"src/world/ae_stagefade.c"),str(source or ROOT/"src/modes/ae_ovl18b.c")]
    exports.write_text("EXPORTS\n"+"\n".join(["ae_ram_create","ae_ram_clear_error","ae_ram_failed","ae_ram_error","ae_ptr","ae_rd8","ae_rd16","ae_rd32","ae_wr8","ae_wr16","ae_wr32"]+[v[2] for v in FUNCS.values()])+"\n",encoding="ascii")
    subprocess.run([shutil.which("gcc"),"-std=c11","-O1","-Wall","-Wextra","-Werror",
        "-ffunction-sections","-fdata-sections","-fvisibility=hidden","-Wl,--gc-sections","-Wl,--exclude-all-symbols",
        "-shared","-static-libgcc","-I"+str(ROOT/"src/world"),"-I"+str(ROOT/"src/modes"),"-I"+str(ROOT/"src/player"),
        "-I"+str(ROOT/"src/sound"),"-I"+str(ROOT/"src/monkeys"),"-I"+str(ROOT/"src/bodies"),
        *inputs,str(exports),"-o",str(dll),"-lm"],cwd=ROOT,check=True)
    return dll

def instrument_mutations(source):
    mutated=source
    for i,(_name,(_a,_s,symbol,_ret,_args)) in enumerate(FUNCS.items(),1):
        pattern=re.compile(r"(\b(?:void|int32_t|uint32_t)\s+"+re.escape(symbol)+r"\s*\([^)]*\)\s*\{)",re.S)
        lines="\n".join(f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {i*2-v}u) ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x18B00000u | {i*2-v}u);" for v in (1,2))
        mutated,n=pattern.subn(r"\1\n"+lines,mutated,count=1)
        if n!=1: raise SystemExit(f"could not instrument {symbol}")
    return mutated

def run_mutations():
    src=ROOT/"src/modes/ae_ovl18b.c"; tmp=ROOT/"extract/mips/ae_ovl18b_mutant.c"
    tmp.parent.mkdir(parents=True,exist_ok=True); tmp.write_text(instrument_mutations(src.read_text(encoding="utf-8")),encoding="utf-8")
    try:
        h=Harness(build(tmp,"ovl18b_mutant")); caught=0
        for i,name in enumerate(FUNCS,1):
            for variant in (1,2):
                before=h.failures; ok=h.run(name,variant-1,selector=i*2-variant,mutant=True)
                if not ok and h.failures>before: caught+=1
                else: print(f"MUTATION SURVIVED {name} selector={i*2-variant}",flush=True)
        print(f"mutations caught: {caught}/{len(FUNCS)*2}",flush=True)
        return 0 if caught==len(FUNCS)*2 else 1
    finally: tmp.unlink(missing_ok=True)

def main():
    p=argparse.ArgumentParser(); p.add_argument("--cases",type=int,default=8); p.add_argument("--coverage",action="store_true"); p.add_argument("--mutate",action="store_true"); p.add_argument("--function",action="append",choices=FUNCS)
    a=p.parse_args()
    if a.mutate: return run_mutations()
    h=Harness(build()); passed=h.all(a.cases,a.function); coverage=h.finish(EXTRA_COVERAGE,default_run=a.cases==8 and not a.function,show_coverage=a.coverage,names=a.function)
    print(f"differential: {sum(h.counts.values())} cases; {'PASS' if passed and coverage else 'FAIL'}",flush=True)
    return 0 if passed and coverage else 1

if __name__=="__main__": sys.exit(main())
