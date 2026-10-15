"""Generate typed host adapters from module hook declarations, not game instructions."""
import collections
import csv
import json
import re
from game_call_inventory import ROOT, declarations, ledger

EXISTING = {"AeBodyHooks", "AeSolidHooks", "AeAnimEventHooks"}
FIELDS = {t: re.sub(r"(?<!^)(?=[A-Z])", "_", t[2:-5]).lower() for t in
          {re.search(r"\b(Ae\w+Hooks)\b", a)[1] for _,_,args in declarations().values()
           for a in args if re.search(r"\b(Ae\w+Hooks)\b", a)}}
FIELDS.update(AeMonkeyHooks="monkey", AeBodyHooks="body", AeWorldHooks="world", AeSolidHooks="solid",
              AeSpawnHooks="spawn", AeGimmickHooks="gimmick", AeAnimEventHooks="anim")
SDK = {"DrawPrim", "MatrixNormal", "VSync", "LoadImage", "StoreImage", "MoveImage", "DrawSync", "SetRotMatrix", "SetTransMatrix",
       "RotTransPers", "RotTransSV", "RotTrans", "CdIntToPos", "CdPosToInt", "PadGetState", "PadInfoMode",
       "SpuWaitIdle", "Printf", "printf", "Exit", "SetBackColor", "SetFarColor", "SetColorMatrix", "SetGeomScreen", "memset", "DR_MOVE",
       "ClearImage", "CdControl", "CdReadyCallback", "RotMatrixX", "RotMatrixY", "ScaleMatrixL", "MulMatrix", "MulMatrix0",
       "PutDrawEnv", "PutDispEnv", "DrawOTag", "ClearOTagR", "SetDrawTPage", "SfxVoiceSetPanVol", "SoundBankSetVolume"}
RENDER = {"ModelDraw", "ModelDrawWithOt", "ModelDrawEnvMap", "ModelTransformRigid", "ModelTransformSkinned",
          "ModelPrepCycleColors", "ModelEmitPrimsCycleColors", "DrawShadow", "DrawRibbonBetween", "MeshInit",
          "MeshInitFromAreaTable", "ColorFadeSet", "SetColorMatrixMode", "ApplyLevelFog", "PropSetMesh",
          "AlertIconDraw", "CopyPlayerEyeTexture", "ItemDrawModel", "ScreenBandDraw", "ModelDrawScreenSpace", "ModelCalcEnvUVs", "ModelEmitPrims", "ModelFillVertexColors",
          "Draw_Nop", "EffectRingDraw", "VertexWobble", "RendererPrimitiveBlock", "RendererWobbleGte", "ModelEmitPrimsScratchColors",
          "HudDrawStatPips", "HudDrawFrameBox", "DrawMenuChrome", "DrawMenuRows", "DrawMenuHighlight",
          "DrawMenuItem", "DrawMenuSelection", "DrawMenuCursor"}
ALIASES = {"SDK_800302D0":"MatrixNormal", "MonkeyClearWords":"ClearWords",
           "pad_get_state":"PadGetState", "pad_info_mode":"PadInfoMode", "stage_request":"StageRequest",
           "area_bump_alloc":"AreaBumpAlloc", "pad_setup_dualshock":"PadSetupDualShock",
           "pad_demo_step":"PadDemoPlaybackStep", "stick_to_buttons":"StickToButtons", "pad_rumble_reset":"PadRumbleReset",
           "SetDrawMove":"DR_MOVE", "exit":"Exit", "Memset":"memset",
           "get_collider_quadrant_mask_xz":"GetColliderQuadrantMaskXZ", "ClearMemory":"memset"}
ADDRESSES = {"DrawPrim":0x80028C64, "MatrixNormal":0x800302D0, "SetRotMatrix":0x8002FB00, "SetTransMatrix":0x8002FB30, "RotTransPers":0x80031330,
    "RotTransSV":0x80031230, "RotMatrixY":0x80030890, "ScaleMatrixL":0x800305B0,
    "RotMatrixX":0x80030A30, "MulMatrix0":0x80031830, "MulMatrix":0x80031470,
    "ClearImage":0x80028850, "CdControl":0x8002408C, "CdReadyCallback":0x80024078,
    "Exit":0x800A3700, "CdIntToPos":0x80025D20, "VSync":0x8002E520,
    "Area53_BossUpdate":0x8013D57C, "Area46_BossUpdate":0x8013F1AC, "Area57_BossUpdate":0x8013E4B4,
    "PutDrawEnv":0x80028D30, "PutDispEnv":0x80028EFC, "DrawOTag":0x80028CC0,
    "memset":0x800A3740, "DrawSync":0x800286CC, "LoadImage":0x80028978, "StoreImage":0x800289D8,
    "MoveImage":0x80028A38, "SetBackColor":0x800315E0, "SetFarColor":0x80031630,
    "SetColorMatrix":0x80031600, "GetColliderQuadrantMaskXZ":0x800889B4, "DR_MOVE":0x8002B380,
    "Area44_Boss_SetAnim":0x80140948, "Area46_Boss_SetAnim":0x8013EF0C,
    "Area46_Boss_ClearFlag":0x8013EF00, "Area56_Boss_SetCameraObjAnim":0x801406F8,
    "Area57_Boss_SetAnim":0x8013F3A4, "Area57_Boss_ClearFlag":0x8013F398}
SDK_ARITY = {"DrawPrim":1, "MatrixNormal":2, "VSync":1, "SetRotMatrix":1, "SetTransMatrix":1, "RotTransPers":4,
    "RotTransSV":3, "RotTrans":3, "CdIntToPos":2, "CdPosToInt":1,
    "PadGetState":1, "PadInfoMode":3, "SpuWaitIdle":0, "DrawSync":1,
    "LoadImage":2, "StoreImage":2, "MoveImage":3, "SetBackColor":3,
    "SetFarColor":3, "SetColorMatrix":1, "memset":3, "DR_MOVE":4,
    "Printf":4, "Exit":1, "SetGeomScreen":1, "ClearImage":4,
    "CdControl":3, "CdReadyCallback":1, "RotMatrixX":2, "RotMatrixY":2,
    "ScaleMatrixL":2, "MulMatrix":2, "MulMatrix0":3,
    "PutDrawEnv":1, "PutDispEnv":1, "DrawOTag":1,
    "ClearOTagR":2, "SetDrawTPage":4, "SfxVoiceSetPanVol":3, "SoundBankSetVolume":2}
ARCHIVE_ARITY = {"ArcGetMemberPos":2, "CdMgrQueueCommand":4, "CdMgrUpdate":0,
                 "LoadListStep":0, "FUN_80025E24":1,
                 "ArcLoadMember":2, "CdMgrWaitIdle":1, "LoadListInit":2}
MODE_PLATFORM_ARITY = {"InitDisplay":0, "InitSystem":0, "SetVideoMode":1,
                       "ResetGraph":1, "RunMinigameExe":1}
SDK_ARITY.update(MODE_PLATFORM_ARITY)
SDK.update({'SetVideoMode', 'ResetGraph'})
ADDRESSES.update(InitDisplay=0x8005CFEC, InitSystem=0x8005CD50,
                 SetVideoMode=0x8002EDF0, ResetGraph=0x80028350,
                 RunMinigameExe=0x80011A2C, ArcLoadMember=0x800112E4,
                 CdMgrWaitIdle=0x8001256C, LoadListInit=0x80011104,
                 LoadListStep=0x800116C0, CdMgrUpdate=0x800125B8)


# Named native APIs used by the mode overlays. These are API declarations,
# independent of instruction translation or the unit checkers.
MODE_NATIVE = {
    'SetDispMask': (0x80028634,1), 'SetGraphDebug': (0x800284C4,1),
    'StopCallback': (0x8002E824,0), 'ClearImage2': (0x800288E0,4),
    'SetGeomOffset': (0x8002FAD0,2), 'SetDefDrawEnv': (0x8002B280,5),
    'SetDefDispEnv': (0x8002B340,5), 'GetVideoMode': (0x8002EE04,0),
    'VSyncCallback': (0x8002E7C0,1), 'ReadGeomScreen': (0x800315D0,0),
    'StSetRing': (0x80025C60,2), 'StSetStream': (0x80025C90,5),
    'StFree': (0x80025EB0,1), 'StRingStatus': (0x80025F60,2),
    'CdPlayXa': (0x800242FC,3), 'LoadTimList': (0x80068D10,1),
    'TextLineWidth': (0x8008DE1C,1), 'SetDrawStp': (0x8002B3E0,2),
    'CdSync': (0x80026A50,0), 'CdReady': (0x8002408C,3),
    'SoundRestoreVolume': (0x80086170,0),
    'SoundStopAndRestoreVolume': (0x80086248,1),
    'srand': (0x800A3750,1), 'memcpy': (0x800A3760,3),
    'MemCardOp': (0x80010000,2),
}
for name, (address, count) in MODE_NATIVE.items():
    ADDRESSES[name] = address
    MODE_PLATFORM_ARITY[name] = count
    SDK_ARITY[name] = count
ARCHIVE_ARITY.update(ArcLoadEntry=1, CdMgrStopStream=0)
ADDRESSES.update(ArcLoadEntry=0x80011194, CdMgrStopStream=0x8001231C)


# Overlay-local native archive/display leaves retain their member identity.
MODE_OVERLAY_NATIVE = {
    'Ovl185_DrawText': (0x185,0x8014666C,4,'renderer'),
    'SaveUi_DrawPortrait': (0x186,0x80139570,2,'renderer'),
    'SaveUi_DrawCursorAnim': (0x186,0x80139CF0,0,'renderer'),
    'SaveUi_DrawFileIcons': (0x186,0x8013A478,0,'renderer'),
    'SaveUi_FadeDraw': (0x186,0x8013A70C,0,'renderer'),
    'SaveUi_DrawModel': (0x186,0x8013A804,4,'renderer'),
    'SaveUi_DrawIcon': (0x186,0x8013946C,3,'renderer'),
    'Ovl181_CdCommand': (0x181,0x80136AEC,1,'archive'),
    'SeqPlayer_CdReadSeek': (0x180,0x80136454,2,'archive'),
    'SeqPlayer_CdPlayXaRange': (0x180,0x801364B0,4,'archive'),
    'SeqPlayer_LoadFiles': (0x180,0x80136884,1,'archive'),
    'SeqPlayer_CdSetFilter': (0x180,0x80136E68,1,'archive'),
    'SeqPlayer_StGetNextWait': (0x180,0x80136E00,2,'archive'),
    'SeqPlayer_SetupDisplay': (0x180,0x80136BD4,0,'platform'),
    'SeqPlayer_CopyScreenToBack': (0x180,0x801367DC,0,'renderer'),
    'SeqPlayer_Present': (0x180,0x80136FF0,1,'renderer'),
    'SeqPlayer_LoadSavedImage': (0x180,0x8013AA70,0,'renderer'),
    'SeqPlayer_SaveRestoreVram': (0x180,0x8013BA80,1,'renderer'),
}
MODE_RENDER = {
    'DebugTextSetCursor': (0x8008CC60,2),
    'DrawMenuArrows': (0x8008211C,3),
    'ModelEmitPrimsAlt': (0x8001A414,3),
    'MatrixBuild': (0x80030450,3),
    'ModelTransform': (0x800160EC,3), 'ModelDrawTransformed': (0x80017EFC,4),
}
MODE_NATIVE_ALIASES = {'Exit': (0x000000B0,)}
ADDRESSES.update({name: address for name,(address,_) in MODE_RENDER.items()})
for name, (_, address, count, group) in MODE_OVERLAY_NATIVE.items():
    ADDRESSES[name] = address
    if group == 'archive': ARCHIVE_ARITY[name] = count
    elif group == 'platform':
        MODE_PLATFORM_ARITY[name] = count
        SDK_ARITY[name] = count


def inventory():
    types, structs = {}, {}
    for path in sorted((ROOT / "src").rglob("*.h")):
        if "native" in path.name or ("/game/" in path.as_posix() and path.name != 'ae_modes.h'): continue
        raw = path.read_text()
        clean = re.sub(r"/\*.*?\*/|//[^\n]*", "", raw, flags=re.S)
        for m in re.finditer(r"typedef\s+(\w+)\s+\(\*(\w+)\)\s*\((.*?)\);",clean,re.S):
            types[m[2]] = (m[1],m[3])
        for m in re.finditer(r"typedef struct (Ae\w+Hooks) \{(.*?)\} \w+;",raw,re.S):
            if m[1] in FIELDS:
                structs[m[1]] = (path,m[2])
    symbols = {}
    for row in csv.reader((ROOT/"tools/symbols.csv").read_text().splitlines()):
        if len(row)>1 and re.fullmatch(r"[0-9a-fA-F]{8}",row[0]): symbols[row[1].lower()] = int(row[0],16)
    for row in ledger(): symbols.setdefault(re.sub(r"\s*\(.*", "",row["name"]).lower(), int(row["address"],16))
    records=[]
    for typ,(path,body) in sorted(structs.items()):
        if typ in EXISTING or typ not in FIELDS: continue
        # Remove comments but retain the comment following each declaration for address resolution.
        for match in re.finditer(r"([^;]+);([^\n]*)", body):
            decl = re.sub(r"/\*.*?\*/|//[^\n]*", "", match[1],flags=re.S).strip()
            inline = re.fullmatch(r"(\w+)\s+\(\*(\w+)\)\s*\((.*?)\)",decl,re.S)
            normal = re.fullmatch(r"(\w+)\s+(\w+)",decl)
            if inline: ret,name,params = inline.groups()
            elif normal and normal[1] in types:
                name = normal[2]; ret,params = types[normal[1]]
            else: continue
            params=[p.strip() for p in params.split(',')]
            # Name-dispatch callbacks carry native pointers, never o32 words.
            if any('const char *' in p for p in params) and any(re.search(r'const uint32_t\s*(?:\*|\w+\[)', p) for p in params):
                continue
            address_match = re.search(r"\b(80[0-9a-fA-F]{6})\b", match[2]) or re.search(r"(80[0-9a-fA-F]{6})",name)
            canonical = ALIASES.get(name,name)
            address = int(address_match[1],16) if address_match else ADDRESSES.get(canonical, symbols.get(canonical.lower(),0))
            # These historical camera hook labels are reversed; the reviewed
            # camera checker records the actual original call addresses.
            if typ=='AeCameraHooks' and name in {'rsin','rcos'}:
                address=0x8002FC20 if name=='rsin' else 0x8002FB50
            if address == 0x800A3720: canonical = "Printf"
            # Address-labelled module hooks use the same public SDK boundary.
            # These identities are recorded in notes/inventory/functions.csv.
            sdk_by_address={value:key for key,value in ADDRESSES.items() if key in SDK}
            if address in sdk_by_address: canonical=sdk_by_address[address]
            # ae_misc's historical ModelDraw field is the item-model helper,
            # whose arguments are (model index, item), not ModelDraw's ABI.
            if address==0x800845E0: canonical='ItemDrawModel'
            kind = "ported" if address in {int(r['address'],16) for r in ledger()} else "missing"
            # These callbacks replace audited packet blocks inside state ports;
            # routing the ring callback back to its public entry recurses.
            missa_render = {'HudDrawTimeRankIcon', 'HudDrawStatPips', 'HudDrawFrameBox',
                            'DrawMenuChrome', 'DrawMenuRows', 'DrawMenuHighlight',
                            'DrawMenuItem', 'DrawMenuSelection', 'DrawMenuCursor',
                            'RendererHudMonkeyBadge', 'TrailRingDraw'}
            if typ == 'AeMissaHooks' and name in missa_render:
                kind = 'render'
                if name == 'TrailRingDraw':
                    canonical, address = 'RenderTrailRingSegment', 0
            if canonical in ARCHIVE_ARITY and canonical != 'ArcGetMemberPos': kind="archive"
            elif canonical in SDK and canonical not in {'RotMatrixX', 'RotMatrixY', 'SoundBankSetVolume', 'SfxVoiceSetPanVol'}: kind="sdk"
            elif canonical == "CameraLookAt": kind="render"
            elif kind != "ported" and (canonical in RENDER or name.startswith(("Render", "Gpu", "DrawDebug", "DrawProjectile", "FinishDebug", "DrawParticle", "DrawRibbon", "ProjectEffect", "ModelProject", "ZakoDebug"))): kind="render"
            if name in {"position_padding", "stale_position_padding", "LocalPositionPadding", "LocalDirectionPadding"}: kind="padding"
            if name in {"CallIndirect", "PlayerCondition", "PlayerAction", "GimmickHandler", "PlayerModeCallback", "ProjectileExpireCallback", "ZakoTypeUpdate", "AreaModeUpdate", "ZakoFrameHook", "ColliderScriptUpdate"}: kind="indirect"
            if typ == 'AeHudHooks' and any('AeHudDraw' in p for p in params): kind='hud'
            if name=='PlayerUpdateLocalFrame':kind='player_locals'
            if name=='PlayerVoiceFallback':kind='player_voice_fallback'
            if name=='PlayerActorCameraTable':kind='player_actor_camera_table'
            if name=='xa_padding':kind='xa_padding'
            if typ=='AeMissaHooks' and name=='BeginFrame':kind='frame'
            records.append(dict(table=typ,field=FIELDS[typ],name=name,canonical=canonical,address=address,
                                kind=kind,ret=ret,params=params))
    return records, structs


def generate():
    records,structs=inventory()
    (ROOT/'extract/mips').mkdir(parents=True, exist_ok=True)
    (ROOT/'extract/mips/game_hook_inventory.json').write_text(json.dumps(records,indent=2))
    print(collections.Counter(r['kind'] for r in records))
    for r in records:
        if not r['address'] and r['kind'] not in {'render','hud','padding','indirect','player_locals','player_voice_fallback','player_actor_camera_table','xa_padding'}: print('UNRESOLVED',r['table'],r['name'],r['params'])
    platform = ['/* Generated by tools/generate_game_hooks.py. */', '#ifndef AE_GAME_PLATFORM_H', '#define AE_GAME_PLATFORM_H',
                'typedef struct AeGamePlatform {']
    sdk_names=sorted({r['canonical'] for r in records if r['kind']=='sdk'} | {'PutDrawEnv','PutDispEnv','DrawOTag'} | set(MODE_PLATFORM_ARITY))
    for name in sdk_names:
        args=', '.join('uint32_t a'+str(i) for i in range(SDK_ARITY[name]))
        platform.append(f'    int32_t (*{name})(void *user, AeRam *ram'+(', '+args if args else '')+');')
    platform+=['} AeGamePlatform;', 'typedef struct AeGameRenderer {']
    render_counts={n:max(len(r['params'])-2 for r in records if r['canonical']==n and r['kind']=='render')
                   for n in sorted({r['canonical'] for r in records if r['kind']=='render'})}
    render_counts.update({n: v[2] for n, v in MODE_OVERLAY_NATIVE.items() if v[3] == "renderer"})
    render_counts.update({n: v[1] for n, v in MODE_RENDER.items()})
    for name,count in render_counts.items():
        args=', '.join('uint32_t a'+str(i) for i in range(count))
        platform.append(f'    int32_t (*{name})(void *user, AeRam *ram'+(', '+args if args else '')+');')
    platform+=['} AeGameRenderer;', 'typedef struct AeGameArchive {']
    for name,count in ARCHIVE_ARITY.items():
        args=', '.join('uint32_t a'+str(i) for i in range(count))
        platform.append(f'    int32_t (*{name})(void *user, AeRam *ram'+(', '+args if args else '')+');')
    platform+=['} AeGameArchive;', '#endif', '']
    (ROOT/'src/game/ae_game_platform.h').write_text('\n'.join(platform))
    native = ['/* Generated named native API routes. */']
    for group, names in [('platform', {n: SDK_ARITY[n] for n in sdk_names}), ('archive', ARCHIVE_ARITY)]:
        for name, count in names.items():
            address = ADDRESSES.get(name, next((r['address'] for r in records if r['canonical'] == name), 0))
            if not address: continue
            args = ', '.join(f'args[{i}]' for i in range(count))
            identity = f' && g->loaded_overlay == 0x{MODE_OVERLAY_NATIVE[name][0]:X}u' if name in MODE_OVERLAY_NATIVE else ''
            condition=f'address == 0x{address:08X}u{identity}'
            for alias in MODE_NATIVE_ALIASES.get(name,()):
                condition+=f' || (address == 0x{alias:08X}u && !strcmp(name, "{name}"))'
            native += [f'if ({condition}) {{',
                       f'    if (g->host.{group}.{name}) return g->host.{group}.{name}(g->host.user, r' + (', ' + args if args else '') + ');',
                       f'    ae_ram_fail(r, "Mode: missing native {name} boundary"); return 0;', '}']
    for name, count in render_counts.items():
        address = ADDRESSES.get(name, next((r['address'] for r in records if r['canonical'] == name), 0))
        if not address: continue
        args = ', '.join(f'args[{i}]' for i in range(count))
        identity = f' && g->loaded_overlay == 0x{MODE_OVERLAY_NATIVE[name][0]:X}u' if name in MODE_OVERLAY_NATIVE else ''
        native += [f'if (address == 0x{address:08X}u{identity}) {{',
                   f'    if (g->host.renderer.{name}) return g->host.renderer.{name}(g->host.user, r' + (', ' + args if args else '') + ');',
                   '    return g->host.render_words ? g->host.render_words(g->host.user, r, name, address, count, args) : 0;', '}']
    (ROOT/'src/game/ae_game_native.inc').write_text('\n'.join(native)+'\n')

    out=['/* Generated typed module wiring; see tools/generate_game_hooks.py. */', '#include "ae_calls.h"', '',
         'static int32_t wire_original_words(void *user, AeRam *ram, const char *name, uint32_t address, uint32_t count, const uint32_t *args) {',
         '    AeGame *g = user; uint32_t words[7] = {0}; (void)ram;',
         '    if (count > 7) return ae_game_missing_words(g, name, address, count, args);',
         '    for (uint32_t i = 0; i < count; ++i) words[i] = args[i];',
         '    if (!ae_game_find_call(address)) return ae_game_missing_words(g, name, address, count, args);',
         '    return ae_game_call7(g, address, words[0], words[1], words[2], words[3], words[4], words[5], words[6]);',
         '}',
         'static int32_t wire_renderer_words(void *user, AeRam *ram, const char *name, uint32_t address, uint32_t count, const uint32_t *args) {',
         '    AeGame *g = user;',
         '    return g->host.render_words ? g->host.render_words(g->host.user, ram, name, address, count, args) : 0;',
         '}',
         'static int32_t wire_overlay_render(void *user, AeRam *ram, const char *name, uint32_t count, const uint32_t *args) {',
         '    return wire_renderer_words(user, ram, name, 0, count, args);',
         '}',
         'static void wire_overlay_render_void(void *user, AeRam *ram, const char *name, uint32_t count, const uint32_t *args) {',
         '    (void)wire_renderer_words(user, ram, name, 0, count, args);',
         '}',
         '/* MissB retains its array-before-count public ABI. */',
         'static int32_t wire_missb_words(void *user, AeRam *ram, const char *name, uint32_t address, const uint32_t args[4], uint32_t count) {',
         '    return ae_game_mode_call(user, ram, name, address, count, args);',
         '}', '']
    for r in records:
        fn='wire_'+r['field']+'_'+r['name']
        args=[]
        for i,param in enumerate(r['params']):
            # Keep the type, replace optional parameter names.
            typ=re.sub(r'\s+\w+$','',param) if re.search(r'\s+\w+$',param) else param
            if '*' in param:
                typ=param[:param.rfind('*')+1]
            args.append(typ+' p'+str(i))
        out+=[f'static {r["ret"]} {fn}('+', '.join(args)+') {', '    AeGame *g = (AeGame *)p0;']
        out += [f'    (void)p{i};' for i in range(1,len(args))]
        words=['(uint32_t)p'+str(i) for i in range(2,len(args))]
        four=(words+['0']*4)[:4]
        name=r['canonical']
        if r['kind']=='hud':
            out += ['    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);', '}', '']
            continue
        if r['kind']=='padding': expr=('g->host.body_direction_padding ? g->host.body_direction_padding(g->host.user, g->ram) : 0' if r['name']=='LocalDirectionPadding' else '0')
        elif r['kind']=='player_locals':
            expr='g->host.player_update_locals ? g->host.player_update_locals(g->host.user, g->ram) : g->call_stack_pointer - 120u'
        elif r['kind']=='player_voice_fallback':
            # Voice 10 always selects SFX 0x8A at 800689CC..800689D0.
            # The original caller's leftover a1 is irrelevant to that path.
            expr='g->host.player_voice_fallback ? g->host.player_voice_fallback(g->host.user, g->ram) : 0x8Au'
        elif r['kind']=='player_actor_camera_table':
            expr='g->host.player_actor_camera_table ? g->host.player_actor_camera_table(g->host.user, g->ram) : (ae_ram_fail(g->ram, "PlayerUpdate: missing actor camera-script table service"), 0)'
        elif r['kind']=='xa_padding':
            expr='g->host.xa_caller_padding ? g->host.xa_caller_padding(g->host.user, g->ram, p2) : 0'
        elif r['table']=='AeMissaHooks' and name=='BeginFrame':
            expr='ae_game_mode_call(g, g->ram, "BeginFrame", 0x8005DF48u, 4, (const uint32_t[]){'+', '.join(four)+'})'
        elif r['kind'] in {'sdk','archive'}:
            counts=SDK_ARITY if r['kind']=='sdk' else ARCHIVE_ARITY
            group='platform' if r['kind']=='sdk' else 'archive'
            values=(words+['0']*counts[name])[:counts[name]]
            tail=(', '+', '.join(values)) if values else ''
            expr=f'g->host.{group}.{name} ? g->host.{group}.{name}(g->host.user, g->ram{tail}) : 0'
        elif r['kind']=='render':
            values=(words+['0']*render_counts[name])[:render_counts[name]]
            tail=(', '+', '.join(values)) if values else ''
            expr=f'g->host.renderer.{name} ? g->host.renderer.{name}(g->host.user, g->ram{tail}) : (g->host.render ? g->host.render(g->host.user, g->ram, "{name}", '+', '.join(four)+') : 0)'
            if r['table']=='AeMissaHooks' and r['name']=='TrailRingDraw':
                expr=f'g->host.renderer.{name} ? g->host.renderer.{name}(g->host.user, g->ram{tail}) : (g->host.render_words ? g->host.render_words(g->host.user, g->ram, "{name}", 0, 5, (const uint32_t[]){{'+', '.join(values)+'}) : 0)'
        elif r['kind']=='indirect':
            if name=='ColliderScriptUpdate':
                addr='ae_rd32(g->ram, 0x800B3344u + ae_rd8(g->ram, '+words[0]+' + 0x12u) * 4u)';argv=four
            elif name=='ProjectileExpireCallback': addr=words[2]; argv=[words[0], words[1], '0','0']
            else: addr=words[0]; argv=(words[1:]+['0']*4)[:4]
            expr='ae_game_call(g, '+addr+', '+', '.join(argv)+')'
        elif r['kind']=='ported':
            if r['table']=='AePadHooks' and name=='StickToButtons':
                # PadUpdate's historical zero-word callback replaces v after
                # computing ~raw buttons (800133E8..80013404).
                expr='ae_game_call(g, 0x800140B4u, ~(uint32_t)(ae_rd8(g->ram, AE_PAD_BUF1 + 3u) | (ae_rd8(g->ram, AE_PAD_BUF1 + 2u) << 8)), 0, 0, 0)'
            elif len(words)==7:
                expr=f'ae_game_call7(g, 0x{r["address"]:08X}u, '+', '.join(words)+')'
            elif len(words)==6:
                expr=f'ae_game_call6(g, 0x{r["address"]:08X}u, '+', '.join(words)+')'
            elif len(words)>4:
                expr=f'ae_game_call5(g, 0x{r["address"]:08X}u, '+', '.join(words[:5])+')'
            else: expr=f'ae_game_call(g, 0x{r["address"]:08X}u, '+', '.join(four)+')'
        elif len(words)>4:
            expr=f'ae_game_missing_words(g, "{name}", 0x{r["address"]:08X}u, {len(words)}, (const uint32_t[]){{'+', '.join(words)+'})'
        else: expr=f'ae_game_missing_call(g, "{name}", 0x{r["address"]:08X}u, '+', '.join(four)+')'
        if r['kind']=='padding': out+=['    (void)g;']
        out += [('    (void)('+expr+');' if r['ret']=='void' else '    return ('+r['ret']+')('+expr+');'), '}', '']
    # camupd_camera_math_hooks changes the copied camera.user to its owning
    # AeCamupdHooks. Preserve that module's callback contract explicitly.
    for r in records:
        if r['table']!='AeCameraHooks':continue
        out += [f'static int32_t nested_camera_{r["name"]}(void *user, AeRam *ram, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {{',
                '    const AeCamupdHooks *owner = user;',
                f'    return wire_camera_{r["name"]}(owner->user, ram, a0, a1, a2, a3);','}']
    out += ['void ae_game_wire_modules(AeGame *g) {']
    for typ,(path,body) in sorted(structs.items()):
        if typ in EXISTING or typ not in FIELDS: continue
        field=FIELDS[typ]
        if re.search(r'void\s*\*\s*user',body): out += [f'    g->{field}.user = g;']
        if re.search(r'\boriginal_call\s*;',body):
            callback = 'ae_game_mode_call' if typ == 'AeModeHooks' else 'wire_original_words'
            out += [f'    g->{field}.original_call = {callback};']
        if typ == 'AeOverlayHooks':
            out += [f'    g->{field}.game = g;', f'    g->{field}.call = ae_game_mode_call;',
                    f'    g->{field}.render = wire_overlay_render;']
        for callback in ('native_call', 'call', 'overlay_call'):
            if typ != 'AeOverlayHooks' and re.search(r'\bAe\w+(?:CallFn|NamedHook)\s+' + callback + r'\s*;', body):
                adapter = 'wire_missb_words' if typ == 'AeMissbHooks' else 'ae_game_mode_call'
                out += [f'    g->{field}.{callback} = {adapter};']
        if re.search(r'\bAe\w+(?:RenderFn|CallFn|NamedHook)\s+renderer\s*;',body): out += [f'    g->{field}.renderer = wire_renderer_words;']
        if typ != 'AeOverlayHooks' and re.search(r'\bAe\w+RenderFn\s+render\s*;',body):
            out += [f'    g->{field}.render = wire_overlay_render_void;']
        if 'ram_stack_pointer' in body: out += [f'    g->{field}.ram_stack_pointer = g->call_stack_pointer;']
        if re.search(r'\buint32_t stack_pointer\s*;', body): out += [f'    g->{field}.stack_pointer = g->call_stack_pointer;']
        if typ=='AePauseMenuHooks': out += ['    g->pause_menu.caller_s5 = g->host.pause_menu_caller_s5;']
        for match in re.finditer(r'const (?:struct )?(Ae\w+Hooks|AeSoundHw)\s*\*\s*(\w+)\s*;',body):
            target='host.sound_hw' if match[1]=='AeSoundHw' else FIELDS[match[1]]
            if typ=='AeStageHooks' and match[2]=='gadget_hooks':
                out += ['    /* ae_stage.c consumes the historical two-callback slot layout. */',
                        '    g->stage.gadget_hooks = (const AeGadgetHooks *)(const void *)&g->legacy_gadget;']
            else: out += [f'    g->{field}.{match[2]} = &g->{target};']
    for r in records: out += [f'    g->{r["field"]}.{r["name"]} = wire_{r["field"]}_{r["name"]};']
    out += ['    g->body.move = g->body_move;', '    g->body.move.stack_pointer = g->call_stack_pointer - 56u;', '    g->camupd.camera = g->camera;']
    for r in records:
        if r['table']=='AeCameraHooks':out += [f'    g->camupd.camera.{r["name"]} = nested_camera_{r["name"]};']
    out+=['}', '']
    (ROOT/'src/game/ae_game_hooks.c').write_text('\n'.join(out))
    lines=['# Generated module hook routing inventory', '', '| Module | Hook | Address | Route |', '|---|---|---|---|']
    lines += [f'| {r["table"]} | {r["name"]} | {r["address"]:08X} | {r["kind"]} |' for r in records]
    lines += ['', '## Name-dispatch interfaces', '',
              'These typed callbacks retain native name/argument pointers. Game calls use the central address/member table; renderer requests preserve the complete word array.', '',
              '| Module | Hook | Route |', '|---|---|---|']
    for typ,(_,body) in sorted(structs.items()):
        if typ not in FIELDS:
            if typ == 'AeOvl186dHooks':
                lines += ['| AeOvl186dHooks | public MCX APIs | AeGameHost.card_call, guarded by member 186 |']
            continue
        if re.search(r'\boriginal_call\s*;',body):
            route = 'named native boundaries and central table, with loaded-member guard' if typ == 'AeModeHooks' else 'central table, with loaded-member guard'
            lines += [f'| {typ} | original_call | {route} |']
        for callback in ('call', 'native_call', 'overlay_call'):
            if re.search(r'\bAe\w+(?:CallFn|NamedHook)\s+' + callback + r'\s*;', body):
                lines += [f'| {typ} | {callback} | central member/address table and named native callbacks; unported calls remain missing |']
        if re.search(r'\bAe\w+(?:RenderFn|CallFn|NamedHook)\s+renderer\s*;',body): lines += [f'| {typ} | renderer | AeGameHost.render_words |']
        if typ != 'AeOverlayHooks' and re.search(r'\bAe\w+RenderFn\s+render\s*;',body):
            lines += [f'| {typ} | render | AeGameHost.render_words (void callback) |']
        if typ == 'AeOverlayHooks': lines += [f'| {typ} | render | AeGameHost.render_words |']
    (ROOT/'notes/port/game_hooks.md').write_text('\n'.join(lines)+'\n')
    shim=['/* Generated test callbacks: preserve host argument words. */']
    wires=[]
    for group, names in [('platform',{n:SDK_ARITY[n] for n in sdk_names}), ('renderer',render_counts),('archive',ARCHIVE_ARITY)]:
        for name,count in names.items():
            address=ADDRESSES.get(name, next((r['address'] for r in records if r['canonical']==name),0))
            args=', '.join('uint32_t a'+str(i) for i in range(count))
            words=['a'+str(i) for i in range(count)]+['0']*(6-count)
            shim += [f'static int32_t test_{group}_{name}(void *user, AeRam *r'+(', '+args if args else '')+') {',
                     '    (void)r;', f'    return trace_call(user, "{name}", 0x{address:08X}u, {count}, '+', '.join(words)+');', '}']
            wires += [f'    host->{group}.{name} = test_{group}_{name};']
    frame_names=re.search(r'typedef struct AeGameFrameHost \{(.*?)\}',(ROOT/'src/game/ae_game_frame_host.h').read_text(),re.S)[1]
    symbols={}
    for row in csv.reader((ROOT/'tools/symbols.csv').read_text().splitlines()):
        if len(row)>1 and re.fullmatch(r'[0-9a-fA-F]{8}',row[0]): symbols[row[1]]=int(row[0],16)
    for name in re.findall(r'\b\w+\b',re.sub(r'AeGameFrameFn','',frame_names)):
        address=symbols.get(name,0)
        shim += [f'static int32_t test_frame_{name}(void *user, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {{',
                 '    (void)r;', f'    return trace_call(user, "{name}", 0x{address:08X}u, 4, a0, a1, a2, a3, 0, 0);', '}']
        wires += [f'    host->frame.{name} = test_frame_{name};']
    shim+=['static void integration_callbacks(AeGameHost *host) {']+wires+['}', '']
    (ROOT/'tools/mips/game_integration_callbacks.inc').write_text('\n'.join(shim))
    direct=['/* Generated typed hook invocations for the integration checker. */']
    for index,r in enumerate(records):
        if r['kind']=='hud':
            direct.append(f'case {index}: {{ AeHudDraw draw = {{0}}; draw.kind = (AeHudDrawKind)a0; draw.asset = a1; draw.x = (int32_t)a2; draw.y = (int32_t)a3; g->hud.{r["name"]}(g->hud.user, g->ram, &draw); return 0; }}')
            continue
        words=[]
        for i,param in enumerate(r['params'][2:]):
            typ=param.split()[0]
            words.append(f'({typ})a{i}')
        args=', '.join([f'g->{r["field"]}.user','g->ram']+words)
        call=f'g->{r["field"]}.{r["name"]}({args})'
        direct.append(f'case {index}: '+(f'{call}; return 0;' if r['ret']=='void' else f'return (int32_t){call};'))
    (ROOT/'tools/mips/game_integration_hooks.inc').write_text('\n'.join(direct)+'\n')


if __name__=='__main__': generate()
