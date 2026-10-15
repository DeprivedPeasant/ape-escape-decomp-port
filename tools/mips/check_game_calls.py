"""Run existing MIPS differential cases through the central table's wrappers.

Builds the same hand ports with private symbols. Public checker entry points
marshall the checker inputs into original ABI words, then invoke ae_calls.c.
Each checker still supplies its original Python hooks on both sides. Native
array helpers and documented partial ports remain delegated, not counted as
complete original-entry proofs.
"""
import argparse
import ctypes
import importlib
import inspect
import hashlib
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tools/mips')]
from game_call_inventory import declarations,ledger
from generate_game_calls import SPECIAL
import check_game as cg
import exclusion_registry as reg

# Their original entries never return. The owning checker proves one worker
# iteration through its explicit test adapter, not a complete synchronous call.
ITERATION_ONLY={'ae_SaveUi_CardStateThread','ae_SaveUi_CardResultThread'}


def proof_identity(checker):
    digest=hashlib.sha256()
    paths=sorted((ROOT/'src').rglob('*'))
    paths += [ROOT/checker, Path(__file__), ROOT/'tools/verified.csv',
              ROOT/'tools/mips/ovl187_test_api.c', ROOT/'tools/mips/ovl187_test_stubs.c',
              ROOT/'tools/mips/ovl186b_test_api.c',ROOT/'tools/mips/ovl186a_shim.c',
              ROOT/'tools/mips/ovl188_test_api.c',
              ROOT/'tools/mips/boot_overlay_test_api.c',ROOT/'tools/mips/ovl_game_stub.c']
    paths += [ROOT/'tools/mips/ovl188_test_api.c',ROOT/'tools/mips/ovl186b_test_api.c',
              ROOT/'tools/mips/overlay_small_harness.py']
    for path in paths:
        if path.is_file():
            digest.update(path.relative_to(ROOT).as_posix().encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()


def build_proxy():
    identities={checker:proof_identity(checker) for checker in {r['checker'] for r in ledger()}}
    rows=ledger(); protos=declarations()
    sources={r['source'] for r in rows}
    names=sorted({r['c_function'] for r in rows})
    renames='\n'.join(f'#define {n} private_{n}' for n in names)+'\n'
    out=ROOT/'extract/mips/call_proxy';out.mkdir(parents=True,exist_ok=True)
    overrides={}
    # The 187 unit suite models its EXE dependencies explicitly. Keep those
    # models inside the proxy too, while its public entries use real adapters.
    model_source=(ROOT/'tools/mips/ovl187_test_stubs.c').read_text()
    model_names=re.findall(r'^(?:void|int32_t|uint32_t) (ae_\w+)\(',model_source,re.M)
    model_renames=''.join(f'#undef {name}\n#define {name} game_o187_{name}\n' for name in model_names)
    models=out/'ovl187_proxy_models.c'
    model_path=(ROOT/'tools/mips/ovl187_test_stubs.c').as_posix()
    model_digest=hashlib.sha256(model_source.encode()).hexdigest()
    models.write_text(model_renames+'#include "'+model_path+'"\n/* '+model_digest+' */\n')
    for source in sources|{'src/game/ae_calls.c','src/game/ae_calls_special.c','src/game/ae_calls_legacy.c','src/game/ae_game_body.c'}:
        target=out/(Path(source).stem+'.c')
        # Preserve the effects checker's explicit dependency models and its
        # event recorder in the same DLL as the central adapters.
        test_define = '#define AE_EFFECTS_TEST\n' if source.startswith('src/effects/ae_effects_') else ''
        source_text=(ROOT/source).read_text()
        if source=='src/ui/ae_hud.c':
            anchor='static void hud_play_sfx(AeRam *r, const AeHudHooks *h, int32_t id) {'
            assert anchor in source_text
            source_text=source_text.replace(anchor,anchor+'\n    extern void game_proxy_capture_sfx(int32_t);\n    game_proxy_capture_sfx(id);',1)
        unit_models=model_renames if source=='src/modes/ae_ovl187.c' else ''
        target.write_text(test_define+renames+unit_models+source_text)
        overrides[source]=target
    # Special declarations omitted from public headers by their modules.
    protos['ae_CollProbe_Slope']=('','int32_t',['AeRam *r','const AeCollProbeHooks *h','uint32_t shape','uint32_t probe','uint32_t out'])
    protos['ae_GadgetSlotsSave']=('','void',['AeRam *r'])
    protos['ae_GadgetSlotsRestore']=('','void',['AeRam *r'])
    protos['ae_GadgetSlotsInitForArea']=('','int32_t',['AeRam *r','int32_t gadget'])
    protos['ae_GadgetSlotsInitForExit']=('','void',['AeRam *r','const AeGadgetHooks *h'])
    # Order matches the checked-in table, including duplicate addresses.
    sorted_rows=sorted(rows,key=lambda r:(int(r['address'],16),r['c_function'] in SPECIAL,'native' in r['source'],r['c_function']))
    indices={r['c_function']:i for i,r in enumerate(sorted_rows)}
    text=['#include "../../../src/game/ae_calls.h"','#include "../../../src/player/native_player.h"','#include <string.h>',
          f'static uint32_t hits[{len(rows)}];', 'uint32_t game_proxy_hits(uint32_t index) { return hits[index]; }']
    covered=[]; delegated=[]
    for name in names:
        if name not in protos: raise AssertionError(name)
        _,ret,params=protos[name]
        variables=[re.search(r'(\w+)\s*(?:\[.*\])?$',p)[1] for p in params]
        signature=', '.join(params)
        text += [f'extern {ret} private_{name}({signature});',f'{ret} {name}({signature}) {{']
        if name in ITERATION_ONLY or name in SPECIAL and name!='ae_GadgetSlotsInitForExit' or not any('AeRam' in p for p in params):
            text += [('    ' if ret=='void' else '    return ')+f'private_{name}('+', '.join(variables)+');','}']
            delegated.append(name);continue
        nullable=[v for p,v in zip(params,variables) if 'Hooks' in p or 'AeSoundHw' in p]
        # AreaKindVisitedGet uses only the area's data query, whose hook table
        # is optional. A null checker table must still exercise its adapter.
        if nullable and name not in {'ae_PlayerApplyGadgetStance','ae_AreaKindVisitedGet'}:
            delegate=f'private_{name}('+', '.join(variables)+')'
            text += ['    if ('+' && '.join('!'+v for v in nullable)+') {',
                     ('        '+delegate+'; return;' if ret=='void' else '        return '+delegate+';'),'}']
        text += ['    AeGame game;', '    ae_game_init(&game, '+variables[next(i for i,p in enumerate(params) if 'AeRam' in p)]+', NULL);']
        text += [f'    ++hits[{indices[name]}];']
        words=[]
        for param,var in zip(params,variables):
            if 'AeRam' in param: continue
            if 'AeSoundHw' in param: text += [f'    if ({var}) game.host.sound_hw = *{var};']
            elif 'Hooks' in param:
                typ=re.search(r'\b(Ae\w+Hooks)\b',param)[1]
                field=re.sub(r'(?<!^)(?=[A-Z])','_',typ[2:-5]).lower()
                field={'monkey':'monkey','body':'body','world':'world','solid':'solid','spawn':'spawn','gimmick':'gimmick','anim_event':'anim'}.get(field,field)
                if name=='ae_GadgetSlotsInitForExit': text += [f'    if ({var}) memcpy(&game.legacy_gadget, {var}, sizeof(game.legacy_gadget));']
                else: text += [f'    if ({var}) game.{field} = *{var}; else memset(&game.{field}, 0, sizeof(game.{field}));']
            else: words.append(f'(uint32_t){var}')
        for i,word in enumerate(words[4:],4): text += [f'    ae_wr32(game.ram, game.call_stack_pointer + {i*4}u, {word});']
        words=(words+['0']*4)[:4]
        call=f'ae_game_call_at({indices[name]})->invoke(&game, '+', '.join(words)+')'
        text += [('    (void)'+call+';' if ret=='void' else f'    return ({ret})'+call+';'),'}']
        covered.append(name)
    # The HUD checker calls a test dispatcher rather than individual C exports.
    # Reuse that declaration-only dispatcher with the public wrapper exports.
    hud_api=(ROOT/'tools/mips/hud_test_api.c').read_text()
    dispatcher=hud_api[hud_api.index('AE_HUD_TEST_EXPORT int32_t ae_hud_test_call'):]
    text += [dispatcher.replace('AE_HUD_TEST_EXPORT int32_t ae_hud_test_call','int32_t game_proxy_hud_call',1),
             'static int32_t hud_sfx[32]; static uint32_t hud_sfx_count;',
             'void game_proxy_capture_sfx(int32_t id) { if (hud_sfx_count < 32) hud_sfx[hud_sfx_count++] = id; }',
             'void game_proxy_hud_clear_sfx(void) { hud_sfx_count = 0; }',
             'uint32_t game_proxy_hud_sfx_count(void) { return hud_sfx_count; }',
             'int32_t game_proxy_hud_sfx_id(uint32_t index) { return index < hud_sfx_count ? hud_sfx[index] : 0; }']
    proxy=out/'game_calls_proxy.c';proxy.write_text('\n'.join(text)+'\n')
    # Save-menu indices are local to its own shim, independent of boot indices.
    boot=(ROOT/'tools/mips/boot_overlay_test_api.c').read_text()
    prefix=boot[:boot.index('int32_t game_proxy_overlay_call')]
    prefix=prefix.replace('ae_ovl181.h','ae_ovl186a.h').replace('ae_ovl182.h','ae_ovl186a.h')
    prefix=prefix.replace('"../../src/', '"'+(ROOT/'src').as_posix()+'/')
    save=(ROOT/'tools/mips/ovl186a_shim.c').read_text()
    dispatch=save[save.index('int32_t integration_overlay_call'):]
    dispatch=dispatch[:dispatch.index('    case 30:')]
    dispatch+='    default: (void)a4; (void)a5; (void)a6; return 0;\n    }\n}\n'
    save_api=out/'save_overlay_test_api.c'
    save_api.write_text(prefix+dispatch.replace('integration_overlay_call','game_proxy_save_overlay_call',1))
    # Explicit dllexport in the unit API disables MinGW's automatic exports
    # for the remaining proxy symbols. Keep the proxy's normal export policy.
    start_api=out/'ovl188_proxy_api.c'
    start_api.write_text((ROOT/'tools/mips/ovl188_test_api.c').read_text()
        .replace('__declspec(dllexport)','')
        .replace('"../../src/', '"'+(ROOT/'src').as_posix()+'/'))
    additions=[str(proxy.relative_to(ROOT)).replace('\\','/'), 'tools/mips/effects_test_api.c',
               'tools/mips/ovl187_test_api.c','tools/mips/ovl186b_test_api.c','tools/mips/boot_overlay_test_api.c',models.relative_to(ROOT).as_posix(),save_api.relative_to(ROOT).as_posix(),start_api.relative_to(ROOT).as_posix()]
    cg.SOURCES.extend(additions)
    try: library=cg.build_dll(overrides,'game_calls_proxy')
    finally:
        for source in additions: cg.SOURCES.remove(source)
    exports=ctypes.CDLL(str(library))
    for name in covered+['ovl188_call','o186_call','o187_call','game_proxy_hud_call']:
        try: getattr(exports,name)
        except AttributeError as error:
            raise RuntimeError(f'central proxy is missing DLL export {name}') from error
    Path(str(library)+'.identities.json').write_text(json.dumps(identities))
    (out/'coverage.json').write_text(json.dumps(dict(wrappers=covered,delegated=delegated),indent=2))
    return library,covered,delegated


def run_checker(checker,library):
    module=importlib.import_module(Path(checker).stem)
    original_cdll=ctypes.CDLL
    original_run=subprocess.run
    # Checker builds and custom test APIs must still run. Replace only their
    # exported port functions with central-table proxy functions.
    proxy=original_cdll(str(library))
    # The loaded proxy DLL has a fixed ledger inventory. Re-reading the CSV
    # for every RAM-helper lookup made large suites spend minutes in Python.
    port_names={row['c_function'] for row in ledger()}
    # Member 189 exports thin named unit wrappers rather than ae_* symbols.
    # Redirect those public entries too; keep its dependency bridge private.
    overlay_exports = ({spec[2]: 'ae_' + name for name,spec in module.FUNCS.items()}
                       if Path(checker).stem == 'check_ovl189' else {})
    class Combined:
        def __init__(self,primary): self.primary=primary
        def __getattr__(self,name):
            if name in overlay_exports: return getattr(proxy,overlay_exports[name])
            hud_exports={'ae_hud_test_call':'game_proxy_hud_call',
                         'ae_hud_test_clear_sfx':'game_proxy_hud_clear_sfx',
                         'ae_hud_test_sfx_count':'game_proxy_hud_sfx_count',
                         'ae_hud_test_sfx_id':'game_proxy_hud_sfx_id'}
            if name in hud_exports: return getattr(proxy,hud_exports[name])
            if name.startswith('effects_test_'): return getattr(proxy,name)
            if name=='o187_call': return proxy.o187_call
            if name=='o186_call': return proxy.o186_call
            if name=='ovl188_call': return proxy.ovl188_call
            if name=='integration_overlay_call':
                if Path(checker).stem!='check_ovl186a':return proxy.game_proxy_overlay_call
                primary=self.primary.integration_overlay_call
                adapter=proxy.game_proxy_save_overlay_call
                for fn in (primary,adapter):
                    fn.argtypes=[ctypes.c_void_p]+[ctypes.c_uint32]*8
                    fn.restype=ctypes.c_int32
                def save_call(shim,index,*words):
                    return (primary if index>=30 else adapter)(shim,index,*words)
                return save_call
            if name.startswith('stagefade_call_'):
                return getattr(proxy,'ae_'+name[len('stagefade_call_'):])
            if name.startswith('ae_player_'):
                try:return getattr(proxy,name)
                except AttributeError:pass
            if name.startswith('ae_') and name in port_names:
                return getattr(proxy,name)
            return getattr(self.primary,name)
    def load(path,*args,**kwargs):
        dll=original_cdll(path,*args,**kwargs)
        p=Path(str(path))
        if p.suffix=='.dll' and p.parent in {ROOT/'extract/mips',ROOT/'extract/checks'} and not p.name.startswith(('mips','oracle')) and '_mut' not in p.name: return Combined(dll)
        return dll
    ctypes.CDLL=load
    def build_with_dependencies(command, *args, **kwargs):
        # Merged modules add real dependency edges to older standalone builds.
        # Resolve those against the same central proxy, without gameplay stubs.
        if isinstance(command, (list, tuple)) and '-shared' in command and 'gcc' in Path(str(command[0])).name.lower():
            # Overlay builders combine the ledger closure with explicit maths
            # leaves. A leaf already in check_game.SOURCES must compile once.
            unique=[]; seen_sources=set()
            for argument in command:
                if str(argument).endswith('.c'):
                    identity=str(Path(argument).resolve()).casefold()
                    if identity in seen_sources:continue
                    seen_sources.add(identity)
                unique.append(argument)
            command = unique + [str(library)]
        return original_run(command, *args, **kwargs)
    subprocess.run=build_with_dependencies
    old_argv=sys.argv
    sys.argv=[str(ROOT/checker)]
    old_stem=reg._stem
    reg._stem=Path(checker).stem
    try:
        if hasattr(module,'main'):
            result=module.main()
        else:
            # Small overlay suites expose their member to a shared runner.
            # Keep the same function-table guard as their script entry point.
            assert set(module.FUNCTIONS)==set(module.shared.CONFIGS[module.MEMBER]['functions'])
            result=module.shared.main(module.MEMBER)
        return result if isinstance(result,int) else 0
    finally:
        ctypes.CDLL=original_cdll; subprocess.run=original_run; sys.argv=old_argv; reg._stem=old_stem


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--checkers',nargs='*')
    parser.add_argument('--worker')
    parser.add_argument('--library',type=Path)
    parser.add_argument('--jobs',type=int,default=1)
    parser.add_argument('--remaining',action='store_true',help='run only suites without a completed proxy report')
    parser.add_argument('--summarize',action='store_true',help='aggregate existing reports without running or declaring them current')
    args=parser.parse_args()
    if args.worker:
        result=run_checker(args.worker,args.library)
        lib=ctypes.CDLL(str(args.library));lib.game_proxy_hits.argtypes=[ctypes.c_uint32];lib.game_proxy_hits.restype=ctypes.c_uint32
        if not result and Path(args.worker).stem=='check_playerupdate':
            # Whole-function dependency doubles isolate the port. Also exercise
            # its central adapter with the real wired game services; count the
            # actual proxy invocation rather than inferring wrapper coverage.
            from check_game_area import Harness
            import gte_oracle
            import os
            oracle=ctypes.CDLL(str(gte_oracle.OUT/f'oracle_{os.getpid()}.dll'))
            oracle.oracle_gte_step.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.c_uint32,ctypes.c_uint32]
            oracle.oracle_gte_step.restype=ctypes.c_uint32
            original_build=gte_oracle.build
            gte_oracle.build=lambda:oracle
            try:
                h=Harness();h.load(1);h.prepare_player()
                h.lib.integration_playerupdate_hooks.argtypes=[ctypes.c_void_p]
                h.lib.integration_playerupdate_hooks.restype=ctypes.c_void_p
                lib.ae_PlayerUpdate.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
                lib.ae_PlayerUpdate.restype=None
                hooks=h.lib.integration_playerupdate_hooks(h.shim)
                for case in range(8):
                    h.clog=[];h.mlog=[];h.callback_error=None
                    lib.ae_PlayerUpdate(h.ram,hooks)
                    h.cpu.call(0x80060780,hooks=h.hooks(0x80060780))
                    h.compare(f'central PlayerUpdate adapter #{case}')
                print('Central PlayerUpdate adapter: 8 real-service differentials')
            finally:gte_oracle.build=original_build
        rows=sorted(ledger(),key=lambda r:(int(r['address'],16),r['c_function'] in SPECIAL,'native' in r['source'],r['c_function']))
        counts={row['c_function']:lib.game_proxy_hits(i) for i,row in enumerate(rows)}
        manifest=Path(str(args.library)+'.identities.json')
        identity=json.loads(manifest.read_text()).get(args.worker) if manifest.exists() else None
        (ROOT/'extract/mips/call_proxy'/f'{Path(args.worker).stem}.json').write_text(json.dumps(dict(result=result,cases=counts,identity=identity)))
        return result
    if args.summarize:
        coverage=json.loads((ROOT/'extract/mips/call_proxy/coverage.json').read_text())
        library=None;covered=coverage['wrappers'];delegated=coverage['delegated']
    else:library,covered,delegated=build_proxy()
    print(f'Central adapters: {len(covered)} testable; delegated helpers/partial interfaces: {delegated}',flush=True)
    checkers=args.checkers or sorted({r['checker'] for r in ledger()})
    failures=[]
    def execute(checker):
        directory=ROOT/'extract/mips/call_proxy'
        report=directory/f'{Path(checker).stem}.json'
        if args.summarize:
            return checker,json.loads(report.read_text())['result'] if report.exists() else 1
        if args.remaining and report.exists():
            previous=json.loads(report.read_text())
            if previous['result']==0 and previous.get('identity')==proof_identity(checker):return checker,0
        print('CHECK',checker,flush=True)
        with (directory/f'{Path(checker).stem}.log').open('w') as output:
            result=subprocess.run([sys.executable,__file__,'--worker',checker,'--library',str(library)],stdout=output,stderr=subprocess.STDOUT).returncode
        return checker,result
    with ThreadPoolExecutor(max_workers=max(1,args.jobs)) as pool:
        for future in as_completed([pool.submit(execute,checker) for checker in checkers]):
            checker,result=future.result()
            print('DONE',checker,'PASS' if not result else f'FAIL ({result})',flush=True)
            if result:failures.append(checker)
    counts={row['c_function']:0 for row in ledger()}
    for checker in checkers:
        path=ROOT/'extract/mips/call_proxy'/f'{Path(checker).stem}.json'
        if path.exists():
            previous=json.loads(path.read_text())
            if previous['result']:
                if checker not in failures:failures.append(checker)
            else:
                for name,count in previous['cases'].items():counts[name]+=count
    report=dict(failures=failures,cases=counts,delegated=delegated)
    (ROOT/'extract/mips/game_calls_results.json').write_text(json.dumps(report,indent=2))
    print('Wrappers exercised:',sum(v>0 for v in counts.values()),'cases:',sum(counts.values()),flush=True)
    print('Failed checkers:',failures,flush=True)
    return bool(failures)


if __name__=='__main__':sys.exit(main())
