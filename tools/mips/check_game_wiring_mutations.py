"""Executable mutations of the central wrappers, module wires and stage order.

Uses the area integration oracle; build errors never count as detections.
Mutant sources and detailed results stay under ignored extract/mips.
"""
import itertools
import json
import re
import sys
from check_game_area import Harness, ROOT, mr, duplicate_overlay_fixture


def mutations():
    source='src/game/ae_game_hooks.c';text=(ROOT/source).read_text()
    for label,old,new in [
        ('missb missing typed adapter','g->missb.call = wire_missb_words;','g->missb.call = NULL;'),
        ('missb lost array word','name, address, count, args);\n}\n\nstatic','name, address, 0, args);\n}\n\nstatic'),
        ('missb renderer becomes missing','return ae_game_mode_call(user, ram, name, address, count, args);',
         'return wire_original_words(user, ram, name, address, count, args);'),
    ]:
        assert old in text,label
        yield label,source,text.replace(old,new,1),'missb'
    source='src/player/ae_player1.c';text=(ROOT/source).read_text()
    old='if (hooks && hooks->CdMgrQueueCommand)'
    assert old in text
    yield 'XA bypassed native queue',source,text.replace(old,'if (0)',1),'missb'
    source='src/game/ae_game_hooks.c';text=(ROOT/source).read_text()
    old='g->missa.PlayerAuxModelInit = wire_missa_PlayerAuxModelInit;'
    assert old in text
    yield 'missing missa player init callback',source,text.replace(old,'g->missa.PlayerAuxModelInit = NULL;',1),'load'
    old='g->missa.CallIndirect = wire_missa_CallIndirect;'
    assert old in text
    yield 'missing missa indirect',source,text.replace(old,'g->missa.CallIndirect = NULL;',1),'missa'
    old='"RenderTrailRingSegment", 0, 5, (const uint32_t[])'
    assert old in text
    yield 'missa renderer lost fifth word',source,text.replace(old,'"RenderTrailRingSegment", 0, 4, (const uint32_t[])',1),'missa'
    old='~(uint32_t)(ae_rd8(g->ram, AE_PAD_BUF1 + 3u) | (ae_rd8(g->ram, AE_PAD_BUF1 + 2u) << 8))'
    assert old in text
    yield 'missa pad lost buttons',source,text.replace(old,'0',1),'missa'
    source='src/game/ae_game_hooks.c';text=(ROOT/source).read_text()
    for name,address,old_address in [('ActorAnimDecodeEntry','80014EF0','80014E34'),
                                     ('ArcGetMemberPos','80012C0C','8005E000')]:
        prefix='player2' if name=='ActorAnimDecodeEntry' else 'stage'
        match=re.search(r'static (?:void|int32_t) wire_'+prefix+'_'+name+r'\(.*?\n\}',text,re.S)
        assert match
        body=match[0]
        assert '0x'+address+'u' in body
        layer='missa' if name=='ArcGetMemberPos' else 'net_catch'
        yield 'net catch wrong '+name,source,text.replace(body,body.replace('0x'+address+'u','0x'+old_address+'u'),1),layer
    for label,old,new in [
        ('overlay lost named EXE fifth word', 'words[4], words[5], words[6]);', '0, words[5], words[6]);'),
        ('overlay renderer dropped tail', 'ram, name, address, count, args) : 0;', 'ram, name, address, count > 4 ? 4 : count, args) : 0;'),
        ('overlay renderer routed as missing', 'g->host.render_words ? g->host.render_words', 'g->host.missing_words ? g->host.missing_words'),
        ('overlay original call dropped name', 'ae_game_missing_words(g, name, address, count, args)', 'ae_game_missing_words(g, "UnknownFunction", address, count, args)'),
    ]:
        assert old in text, label
        yield label,source,text.replace(old,new),'overlay'
    source='src/game/ae_calls.c';text=(ROOT/source).read_text()
    old='if (CALLS[index].overlay_member == g->loaded_overlay)'
    assert old in text
    yield 'overlay wrong loaded member',source,text.replace(old,'if (CALLS[index].overlay_member == 0x18Cu)').replace('call->overlay_member != g->loaded_overlay','0'),'overlay'
    duplicate=duplicate_overlay_fixture(text)
    old='index < ae_game_call_count() && CALLS[index].address == address'
    new='index < ae_game_call_count() && index == (size_t)(ae_game_find_call(address) - CALLS)'
    assert old in duplicate
    yield 'overlay duplicate stopped at first address',source,duplicate.replace(old,new),'overlay_duplicate'
    source='src/game/ae_modes.c'
    text=(ROOT/source).read_text()
    names=['BeginFrame','MenuWindowsRun','MessageBoxUpdateDraw','DrawTextSprites',
           'PlayerUpdate','DrawTerrain','DrawAreaProps','StageDrawScreenOverlay']
    start=text.index('static int stage_frame_body(')
    prefix,body=text[:start],text[start:]
    lines={name:re.search(r'CALL[0-4]\('+name+r',[^;]+;',body)[0] for name in names}
    for first,second in list(itertools.combinations(names,2))[:24]:
        changed=prefix+body.replace(lines[first],'MUTATION_SWAP',1).replace(lines[second],lines[first],1).replace('MUTATION_SWAP',lines[second],1)
        yield 'order '+first+'/'+second,source,changed,'frame'
    source='src/game/ae_calls.c';text=(ROOT/source).read_text()
    for label,old,new in [
        ('quadrant argument swap','ae_pad_QuadrantFromSigns((int32_t)a0, (int32_t)a1)','ae_pad_QuadrantFromSigns((int32_t)a1, (int32_t)a0)'),
        ('quadrant lost sign','ae_pad_QuadrantFromSigns((int32_t)a0, (int32_t)a1)','ae_pad_QuadrantFromSigns((uint16_t)a0, (int32_t)a1)'),
        ('length wrong axis','ae_player_vec_length3((int32_t)a0, (int32_t)a1, (int32_t)a2)','ae_player_vec_length3((int32_t)a0, (int32_t)a1, (int32_t)a1)'),
        ('length narrowed axis','ae_player_vec_length3((int32_t)a0, (int32_t)a1, (int32_t)a2)','ae_player_vec_length3((int8_t)a0, (int32_t)a1, (int32_t)a2)'),
    ]:
        assert old in text
        yield label,source,text.replace(old,new,1),'helpers'
    source='src/game/ae_calls_special.c';text=(ROOT/source).read_text()
    for label,old,new in [
        ('normalize output offset','ae_sdk_VectorNormal(g->ram, a0, a1)','ae_sdk_VectorNormal(g->ram, a0, a1 + 4u)'),
        ('square input offset','a0 + i * 4','a0 + (i + 1u) * 4'),
    ]:
        assert old in text
        yield label,source,text.replace(old,new,1),'helpers'
    source='src/game/ae_game_hooks.c';text=(ROOT/source).read_text()
    for name in ['AreaSetZoneBoxes','AreaSetVramAnimList','AreaSetFeature30','AreaBumpAlloc']:
        match=re.search(r'static int32_t wire_stage_'+name+r'\(.*?\n\}',text,re.S)
        assert match
        body=match[0]
        changed=re.sub(r'ae_game_call\(g, 0x[0-9A-F]+u','ae_game_call(g, 0x8005E000u',body)
        assert changed!=body
        yield 'wrong hook '+name,source,text.replace(body,changed,1),'load'
    old='g->stage.monkey_hooks = &g->monkey;'
    assert old in text
    yield 'missing stage monkey table',source,text.replace(old,'g->stage.monkey_hooks = NULL;',1),'load'
    old='g->body.move = g->body_move;'
    yield 'missing embedded body movement table',source,text.replace(old,'g->body.move = (AeBodyMoveHooks){0};',1),'frame'
    for name in ('RotMatrixX','RotMatrixY','RotMatrixZ'):
        match=re.search(r'static int32_t wire_pause_menu_'+name+r'\(.*?\n\}',text,re.S)
        assert match
        body=match[0]
        changed=re.sub(r'ae_game_call\(g, 0x[0-9A-F]+u', 'ae_game_call(g, 0x8005E000u',body)
        assert changed!=body
        yield 'lost pause SDK '+name,source,text.replace(body,changed,1),'pause_math'
    old='    if (g->host.hud_draw) g->host.hud_draw(g->host.user, g->ram, p2);'
    yield 'lost typed HUD request',source,text.replace(old,'    (void)g;',1),'hud'
    match=re.search(r'static int32_t wire_gadget_EffectSpawnFromDef\(.*?\n\}',text,re.S)
    assert match
    body=match[0]
    yield 'lost fifth hook argument',source,text.replace(body,body.replace('(uint32_t)p6','0'),1),'extended'
    source='src/game/ae_modes.c';text=(ROOT/source).read_text()
    old='else CALL0(PlayerUpdate, 0x80060780u);'
    assert old in text
    yield 'omit complete PlayerUpdate',source,text.replace(old,'else (void)r;',1),'frame'
    source='src/game/ae_calls_special.c';text=(ROOT/source).read_text()
    old='ae_PlayerAct_ScaleMoveVel(g->ram, &g->player_act, AE_PLAYER_ACT_PLAYER,'
    assert old in text
    yield 'scale action wrong implicit player',source,text.replace(old,old.replace('AE_PLAYER_ACT_PLAYER','a0'),1),'helpers'
    source='src/player/ae_player_act.c';text=(ROOT/source).read_text()
    old='ae_wr16(r, gadget4_state + 4u, 0);'
    assert old in text
    yield 'hoop jump retains controller mode',source,text.replace(old,old.replace(', 0);',', 2);'),1),'helpers'

    source='src/game/ae_game_hooks.c';text=(ROOT/source).read_text()
    match=re.search(r'static int32_t wire_zako_ai_FUN_8013DF54\(.*?\n\}',text,re.S)
    assert match
    body=match[0]
    for word in (6,7,8):
        yield f'lost zako stack word {word-2}',source,text.replace(body,body.replace(f'(uint32_t)p{word}','0'),1),'extended'
    source='src/game/ae_game_body.c';text=(ROOT/source).read_text()
    for label,old,new in [
        ('call7 lost third stack word','ae_wr32(g->ram, stack_argument, a6);','ae_wr32(g->ram, stack_argument, 0);'),
        ('call7 lost stack restoration','ae_wr32(g->ram, stack_argument, saved);','(void)saved;'),
    ]:
        # Limit restoration mutation to the seven-word helper.
        start=text.index('int32_t ae_game_call7(')
        prefix,body=text[:start],text[start:]
        assert old in body
        yield label,source,prefix+body.replace(old,new,1),'extended'


def main():
    # The upstream oracle builder uses one DLL filename per process. Cache
    # its library, while attach_gte_oracle still creates fresh state per CPU.
    sys.path.insert(0,str(ROOT/'tools/codex'))
    import gte_oracle
    oracle=gte_oracle.build()
    gte_oracle.build=lambda:oracle
    directory=ROOT/'extract/mips/game_wiring_mutants';directory.mkdir(parents=True,exist_ok=True)
    results=[]
    def frame_profile(h):
        h.prepare_player()
        h.cpu.write32(0x800A8A58,1);h.cpu.write32(0x800EC350,15)
        # A live menu emits the real DrawPrim boundary before closing on mode 7.
        # Its side effect makes menu/message ordering observable after menuwin porting.
        h.cpu.write16(0x800BEF60,1);h.cpu.write8(0x800F447C,7)
        h.cpu.write8(0x800F5A00,1)
        h.cpu.write8(0x801D2000,0)
        h.cpu.write32(0x800F5A04,0x801D2000);h.cpu.write32(0x800F5A08,0x801D2000)
        h.cpu.write16(0x800F5A10,2)
        h.models['_draw_text_close_message']=1
        h.models['_menu_message_animation']=8
        h.sync();h.frame(1,0)
    baseline=Harness();baseline.load(1);frame_profile(baseline)
    duplicate_path=directory/'duplicate_baseline.c'
    duplicate_path.write_text(duplicate_overlay_fixture((ROOT/'src/game/ae_calls.c').read_text()))
    Harness({'src/game/ae_calls.c':duplicate_path}).direct_duplicate_overlay_routes()
    for index,(label,source,text,layer) in enumerate(mutations()):
        path=directory/(str(index)+'_'+source.rsplit('/',1)[1]);path.write_text(text)
        try:
            h=Harness({source:path})
        except (SystemExit,Exception) as error:
            results.append(dict(label=label,status='build error',reason=str(error)))
            print(label,'BUILD ERROR',flush=True);continue
        try:
            if layer=='missb':h.direct_missb_routes()
            elif layer=='helpers':h.direct_helpers()
            elif layer=='missa':h.direct_missa_routes()
            elif layer=='hud':h.direct_hud_hooks()
            elif layer=='extended':h.direct_extended_hooks()
            elif layer=='pause_math':h.direct_pause_math_hooks()
            elif layer=='overlay':h.direct_overlay_routes()
            elif layer=='overlay_duplicate':h.direct_duplicate_overlay_routes()
            elif layer=='net_catch':h.net_catch()
            else:
                h.load(0x44 if layer=='boss' else 1)
                if layer=='frame':
                    frame_profile(h)
        except (AssertionError,mr.MipsError) as error:
            results.append(dict(label=label,status='caught',reason=str(error)))
            print(label,'caught',flush=True)
        else:
            results.append(dict(label=label,status='survived'))
            print(label,'SURVIVED',flush=True)
    (directory/'results.json').write_text(json.dumps(results,indent=2))
    caught=sum(row['status']=='caught' for row in results)
    print(f'{caught}/{len(results)} executable wiring mutations caught')
    return int(caught!=len(results) or caught<30)


if __name__=='__main__':sys.exit(main())
