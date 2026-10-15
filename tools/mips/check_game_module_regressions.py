"""Strict integrated reproducer for reported module defects; no source patches.

Returns failure while the original and ported RAM differ. This is deliberately
separate from the completed wrapper ABI proofs, whose hook models cannot expose
arguments discarded before the wrapper is reached.
"""
import json
import sys
from check_game_area import Harness, ROOT, mr


def main():
    sys.path.insert(0,str(ROOT/'tools/codex'))
    import gte_oracle
    oracle=gte_oracle.build()
    gte_oracle.build=lambda:oracle
    h=Harness()
    h.direct_missb_routes()
    source='src/player/ae_player1.c'
    text=(ROOT/source).read_text()
    old='if (hooks && hooks->CdMgrQueueCommand)'
    assert old in text
    path=ROOT/'extract/codex-task/old_xa_queue_boundary.c'
    path.write_text(text.replace(old,'if (0)',1))
    try:Harness({source:path}).direct_missb_routes()
    except AssertionError as error:
        assert 'RAM bytes differ' in str(error) or 'calls differ' in str(error),str(error)
        print('Old source rejected: XA bypasses native CD queue',flush=True)
    else:raise AssertionError('old XA queue boundary survived')
    # Empty player headers caused the original load failure. Keep the module
    # aliasing behavior checked separately from the valid boot fixture.
    import check_missa as missa
    import exclusion_registry as registry
    previous_checker=registry._stem
    registry._stem='check_missa'
    fixed=missa.Harness(h.lib._name,missa.SEED)
    fixed.run('PlayerInit',missa.DEFAULT_CASES-1)
    source=(ROOT/'src/player/ae_missa_player.c').read_text()
    old=source.replace('index < ae_rd16(r, stage + AE_MISSA_PLAYER_STAGE_MODEL_AUX_COUNT)',
                       'index < first_model_count')
    assert old!=source
    path=ROOT/'extract/codex-task/old_player_init_count.c';path.write_text(old)
    old_host=Harness({'src/player/ae_missa_player.c':path})
    old=missa.Harness(old_host.lib._name,missa.SEED)
    try:old.run('PlayerInit',missa.DEFAULT_CASES-1)
    except SystemExit as error:
        assert 'RAM differs' in str(error),str(error)
    else:raise AssertionError('old PlayerInit cached count survived')
    registry._stem=previous_checker
    print('Old source rejected: PlayerInit cached auxiliary model count',flush=True)
    # The former host bypassed Mode_Stage's setup and returned immediately
    # after its fade loop. Reject both independently against real instructions.
    h.load(1)
    host_source=(ROOT/'src/game/ae_game_stage.c').read_text()
    setup_start=host_source.index('    ae_wr16(g->ram, 0x800F4476u')
    setup_end=host_source.index('\n}',setup_start)
    old_setup=host_source[:setup_start]+'    return ae_game_call(g, 0x80031B60u, (uint32_t)area, (uint32_t)entry, 0, 0);'+host_source[setup_end:]
    path=ROOT/'extract/codex-task/old_stage_setup.c';path.write_text(old_setup)
    try:Harness({'src/game/ae_game_stage.c':path}).load(1)
    except AssertionError as error:
        assert 'RAM bytes differ' in str(error)
        print('Old source rejected: direct AreaLoad bypasses stage entry',flush=True)
    else:raise AssertionError('old stage setup survived')
    h.direct_frame_branches()
    mode_source=(ROOT/'src/game/ae_modes.c').read_text()
    old='    if (!ae_ram_failed(r)) stage_exit(r, h);'
    assert old in mode_source
    path=ROOT/'extract/codex-task/old_stage_exit.c'
    # Retain the function reference so strict compiler warnings do not turn
    # this behavioral rejection into an unused-function build failure.
    path.write_text(mode_source.replace(old,'    (void)stage_exit;',1))
    try:Harness({'src/game/ae_modes.c':path}).direct_frame_branches()
    except AssertionError as error:
        assert 'RAM bytes differ' in str(error)
        print('Old source rejected: fade return omits stage exit handling',flush=True)
    else:raise AssertionError('old stage exit survived')
    h.flash_regression()
    source=(ROOT/'src/player/ae_gadget.c').read_text()
    start=source.index('void ae_GadgetFlashUpdate(');end=source.index('/* 8006CDE0',start)
    old=source[:start]+source[start:end].replace('AE_G_GADGET_FLASH_LOCAL_POSITION','AE_G_GADGET_TEMP_VECTOR')+source[end:]
    path=ROOT/'extract/codex-task/old_flash_position.c';path.write_text(old)
    try:Harness({'src/player/ae_gadget.c':path}).flash_regression()
    except AssertionError as error:
        assert 'RAM bytes differ' in str(error), 'old flash position must corrupt effect/particle positions'
        print('Old source rejected: flash position overlaps MatrixLookDir',flush=True)
    else:raise AssertionError('old flash position survived')
    failures=[]
    for kind in (59,89):
        h.cpu.restore(h.base)
        shape,probe,out,position=0x801F1000,0x801F1100,0x801F1200,0x801F1300
        h.cpu.write_bytes(shape,bytes(0x38))
        for offset,value in [(6,128),(8,160),(10,128),(16,kind)]:
            h.cpu.write16(shape+offset,value)
        h.cpu.write_bytes(probe,bytes(16))
        h.cpu.write32(probe,64);h.cpu.write8(probe+4,kind);h.cpu.write32(probe+8,128)
        h.cpu.write_bytes(out,bytes(32))
        h.cpu.write_bytes(position,bytes(0x10));h.cpu.write_bytes(position+0x100,bytes(0x10))
        h.cpu.write32(0x800B20B4,position);h.cpu.write32(0x800B20B8,position+0x100)
        h.cpu.write16(0x800B20CC,1);h.cpu.write16(0x800F44A0,0)
        h.cpu.write16(0x800B20C6,3);h.cpu.write16(0x800B20CA,3)
        h.sync()
        result=h.lib.integration_call(h.shim,0x80074BBC,shape,probe,out,0)
        registers=h.cpu.call(0x80074BBC,[shape,probe,out],hooks=h.hooks(0x80074BBC))
        try:
            h.compare(f'CollProbe_Radial type {kind}')
            if result & 0xFFFFFFFF != registers[2]:raise AssertionError('return differs')
        except (AssertionError,mr.MipsError) as error:
            failures.append(str(error));print('FAIL',error)
    # Indicator_Register must return the index allocated before incrementing count.
    # The proximity-trigger initializer stores this result in gimmick +0x14.
    for count in (0, 1, 3):
        h.cpu.restore(h.base)
        h.cpu.write32(0x800BBC50,count)
        h.cpu.write_bytes(0x801F1000,bytes(range(8)))
        h.sync()
        result=h.lib.integration_call(h.shim,0x800716EC,2,0x801F1000,0,0)
        original=h.cpu.call(0x800716EC,[2,0x801F1000,0],hooks=h.hooks(0x800716EC))
        try:
            h.compare(f'Indicator_Register allocated index {count}')
            if result!=count or original[2]!=count:raise AssertionError('allocated index lost')
        except (AssertionError,mr.MipsError) as error:
            failures.append(str(error));print('FAIL',error)
    # The real HalfPipeRangeTest samples y-96 first. These values keep that
    # sample strictly inside [-128,0], while y-height is above the top, so
    # Box reaches its original vertical reject returning contact=1.
    for y,height in [(32,16),(95,94)]:
        h.cpu.restore(h.base)
        shape,probe,out,position=0x801F1000,0x801F1100,0x801F1200,0x801F1300
        h.cpu.write_bytes(shape,bytes(0x38));h.cpu.write16(shape+6,128);h.cpu.write16(shape+8,128);h.cpu.write16(shape+10,64)
        h.cpu.write_bytes(probe,bytes(16));h.cpu.write16(probe,32);h.cpu.write8(probe+4,59);h.cpu.write32(probe+8,height)
        h.cpu.write_bytes(position,bytes(16));h.cpu.write32(position+4,y)
        h.cpu.write32(0x800B20B4,position);h.cpu.write32(0x800B20B8,position)
        h.sync()
        result=h.lib.integration_call(h.shim,0x80073BC8,shape,probe,out,0)
        original=h.cpu.call(0x80073BC8,[shape,probe,out],hooks=h.hooks(0x80073BC8))
        try:
            h.compare(f'CollProbe_Box vertical reject y={y}')
            assert result==1 and original[2]==1, 'vertical contact return lost'
        except (AssertionError,mr.MipsError) as error:
            failures.append(str(error));print('FAIL',error)
    h.cpu.restore(h.base)
    h.cpu.write32(0x800EC354,0x801E8000);h.cpu.write32(0x800EC35C,1)
    h.cpu.write8(0x800F5A00,1);h.cpu.write16(0x800F5A10,4)
    h.sync();h.lib.integration_call(h.shim,0x8008E23C,0,0,0,0)
    h.cpu.call(0x8008E23C,[],hooks=h.hooks(0x8008E23C))
    try:
        h.compare('MessageBoxDrawWindow ordering entry')
        assert len(h.hud_draws)==1 and h.hud_draws[0].ordering_entry==0x800EC3D0+0x4078, 'message ordering entry lost'
    except (AssertionError,mr.MipsError) as error:
        failures.append(str(error));print('FAIL',error)
    # Integration found that the first projected ribbon point also compares
    # against zero. The boundary deliberately projects every point to (0,0).
    from types import SimpleNamespace
    import check_fxscript as fx
    h.cpu.restore(h.base)
    points=0x801D0000
    h.cpu.write_bytes(points,bytes(32));h.sync()
    h.lib.integration_call(h.shim,0x8006F3D0,points,2,0x800B1B78,24)
    projection=SimpleNamespace(last_projection_depth=0,
        call=lambda name,words:h.model(name,0,words,True))
    h.cpu.call(0x8006F3D0,[points,2,0x800B1B78,24],hooks=h.hooks(0x8006F3D0),
               patches=fx.Harness.mips_patches(None,projection))
    try:h.compare('RibbonDraw first projection at origin')
    except (AssertionError,mr.MipsError) as error:
        failures.append(str(error));print('FAIL',error)
    # The standalone ledge case poisons native local padding and requires the
    # explicit stale-input service, while comparing every resulting player byte.
    import check_bodymove as body
    ledge=body.Harness(h.lib._name,with_coverage=False,mips=h.cpu)
    ledge.base=h.base
    if not ledge.run('PlayerTryLedgeGrab',body.LEDGE_SUCCESS_SEED,5,()):
        failures.append('PlayerTryLedgeGrab explicit padding service')
    # Splash-start publishes surface height even without an emitted wake.
    h.cpu.restore(h.base)
    h.cpu.write8(0x800BAD78,0)
    h.cpu.write16(0x1F8003DE,0xFAD6);h.cpu.write16(0x1F8003E4,0)
    h.cpu.write16(0x800B4D90,0x1357)
    for offset,value in [(0x24,100),(0x28,400),(0x2C,-200),(0x34,0)]:
        h.cpu.write32(0x800EC1E0+offset,value&0xFFFFFFFF)
    h.sync()
    h.lib.integration_call(h.shim,0x8006B848,0,0,0,0)
    h.cpu.call(0x8006B848,hooks=h.hooks(0x8006B848))
    try:
        h.compare('PlayerSplashStart shared water height')
        assert h.cpu.read16(0x800B4D90)!=0x1357, 'surface fixture must change height'
    except (AssertionError,mr.MipsError) as error:
        failures.append(str(error));print('FAIL',error)
    # Slow water entry still publishes the height in the branch delay slot.
    h.cpu.restore(h.base)
    h.cpu.write16(0x800EC318,0x1000)
    h.cpu.write8(0x800BAD7B,0);h.cpu.write8(0x800E6902,1)
    h.cpu.write16(0x1F8003DE,0xFAD6);h.cpu.write16(0x1F8003E4,0)
    h.cpu.write16(0x800B4D90,0x1357);h.cpu.write32(0x800EC214,16)
    h.sync()
    h.lib.integration_call(h.shim,0x8006BA2C,0,0,0,0)
    h.cpu.call(0x8006BA2C,hooks=h.hooks(0x8006BA2C))
    try:
        h.compare('PlayerWaterEffectsUpdate slow-entry shared height')
        assert h.cpu.read16(0x800B4D90)!=0x1357, 'slow-entry fixture must change height'
    except (AssertionError,mr.MipsError) as error:
        failures.append(str(error));print('FAIL',error)
    # Entry depth uses +12C, while later wake checks use the halfword +13C.
    h.cpu.restore(h.base)
    h.cpu.write16(0x800EC318,0x1000)
    h.cpu.write8(0x800BAD7B,0);h.cpu.write8(0x800E6902,1)
    h.cpu.write32(0x800EC208,1000);h.cpu.write32(0x800EC214,20)
    h.cpu.write32(0x800EC30C,1096);h.cpu.write16(0x800EC31C,32767)
    for slot in range(12):h.cpu.write16(0x800BF670+slot*2,65535)
    h.sync()
    h.lib.integration_call(h.shim,0x8006BA2C,0,0,0,0)
    h.cpu.call(0x8006BA2C,hooks=h.hooks(0x8006BA2C))
    try:h.compare('PlayerWaterEffectsUpdate distinct depth and wake heights')
    except (AssertionError,mr.MipsError) as error:
        failures.append(str(error));print('FAIL',error)
    import check_ovl18c as ovl
    import exclusion_registry as registry
    previous_checker=registry._stem
    registry._stem='check_ovl18c'
    overlay=ovl.Harness(ovl.build())
    overlay_cases=0
    for name, scenarios in ovl.REGRESSIONS.items():
        for scenario in scenarios:
            if not overlay.run(name,scenario): failures.append(f'{name} regression {scenario}')
            overlay_cases+=1
    rejected=ovl.old_source_regressions()
    registry._stem=previous_checker
    # Ambient position padding appears in the actual area's frame 24. Require
    # an unchanged original baseline, then reject the former forced-zero input.
    integrated=Harness();integrated.load(2);integrated.prepare_player()
    for frame in range(25): integrated.frame(2,frame)
    source=(ROOT/'src/world/ae_ovl18c.c').read_text()
    old=('uint16_t padding = h && h->spawn_hooks && h->spawn_hooks->position_padding\n'
         '        ? h->spawn_hooks->position_padding(h->spawn_hooks->user, r, 0x8013D000u) : 0;')
    assert old in source
    path=ROOT/'extract/mips/ovl18c_old_sources/old_ambient_padding.c'
    path.write_text(source.replace(old,'uint16_t padding = 0;',1))
    broken=Harness({'src/world/ae_ovl18c.c':path})
    broken.load(2);broken.prepare_player()
    try:
        for frame in range(25): broken.frame(2,frame)
    except AssertionError as error:
        assert '0x800bb002' in str(error), 'old padding must fail at the copied effect position'
        rejected+=1
        print('Old source rejected: ambient position padding',flush=True)
    else: raise AssertionError('old ambient padding survived')
    # 187's former one-word SetRotMatrix hook discarded ApplyMatrixLV's
    # in-place translation. The strengthened module checker executes its
    # original three-word SDK instructions with the GTE oracle.
    import check_ovl187 as stage_clear
    previous_checker=registry._stem
    registry._stem='check_ovl187'
    fixed=stage_clear.Harness()
    for case in range(8): fixed.run('StageClearUi_LookAt',case=case)
    assert fixed.failures==0,'stage-clear real ApplyMatrixLV baseline failed'
    source=(ROOT/'src/modes/ae_ovl187.c').read_text()
    repaired = ('    (void)ae_sdk_apply_matrix_lv(ae_ptr(r, view_matrix, 32u),\n'
                '        ae_ptr(r, view_matrix + O187_MATRIX_TRANSLATION_X, 12u),\n'
                '        ae_ptr(r, view_matrix + O187_MATRIX_TRANSLATION_X, 12u));')
    assert repaired in source
    path=ROOT/'extract/codex-task/old_stage_clear_translation.c'
    path.write_text(source.replace(repaired,
        '    (void)ovl187_call1(r, h, "SetRotMatrix", O187_ADDR_GTE_SET_MATRIX, view_matrix);'))
    old=stage_clear.Harness(source=path,tag='old_stage_clear_translation')
    try: old.run('StageClearUi_LookAt',case=0)
    except AssertionError as error:
        assert 'RAM differs' in str(error),str(error)
    else: raise AssertionError('old stage-clear translation survived')
    registry._stem=previous_checker
    print('Old source rejected: stage-clear ApplyMatrixLV translation',flush=True)
    cases=h.cases+2+overlay_cases+integrated.cases+8
    print(f'Module regressions: {cases} comparisons; {rejected}/9 old overlay sources rejected; one old flash, two old stage, one old stage-clear one old PlayerInit and one old XA queue sources rejected; {len(failures)} failures')
    (ROOT/'extract/mips/game_module_regressions.json').write_text(json.dumps(dict(cases=cases,old_overlay_sources_rejected=rejected,old_flash_source_rejected=True,old_stage_sources_rejected=2,old_stage_clear_source_rejected=True,old_player_init_source_rejected=True,old_xa_queue_source_rejected=True,failures=failures),indent=2))
    return bool(failures)


if __name__=='__main__':sys.exit(main())
