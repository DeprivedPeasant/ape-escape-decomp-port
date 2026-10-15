"""Inventory auxiliary actor sources using proven loader relationships.

PlayerUpdate streams eight fixed 20-sector blocks of member 013. PlayerIdleModelSetup
rebases the secondary actor model/frames and player state tables from each block.
Other compact bundles are inventoried without guessing model-animation pairings.
"""
import hashlib
import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import export_model as em
from codex.channel_animation import layout as channel_layout

ROOT = em.ROOT
# Proven StageStartUi_RunA/RunB load lists and DrawWipeA/B actor counts.
# member -> (active actor slots, TIM member); model descriptors +4+i*4,
# animation descriptors +0x18+i*4, all word offsets from the bundle.
UI_BUNDLES = {0x025:(2,0x024),0x026:(2,0x024),0x027:(2,0x024),
              0x037:(1,0x036),0x039:(1,0x038),0x03B:(1,0x03A),
              0x03D:(1,0x03C),0x03F:(1,0x03E),0x041:(2,0x040),0x043:(1,0x042)}


def ui_clips():
    out = {}
    for member,(slots,tim) in UI_BUNDLES.items():
        mem = f'{member:03X}'
        b = (em.MEMBERS/f'{mem}.bin').read_bytes()
        descriptors = struct.unpack_from('<I',b)[0]
        if descriptors not in (12,13,14) or struct.unpack_from('<H',b,4)[0] != descriptors+1:
            raise ValueError(f'{mem}: unexpected UI descriptor count')
        section_offsets = sorted({struct.unpack_from('<H',b,4+i*4)[0]*4 for i in range(descriptors)})
        for slot in range(slots):
            model_word,model_kind = struct.unpack_from('<HH',b,4+slot*4)
            anim_word,anim_kind = struct.unpack_from('<HH',b,0x18+slot*4)
            if model_kind != 3:
                raise ValueError(f'{mem} slot {slot}: descriptor types differ from loader format')
            model,table = model_word*4,anim_word*4
            m = em.parse_actor(b,model)
            if not m or not m['ok']:
                if struct.unpack_from('<H',b,model+4)[0] == 1:
                    continue  # proven active slot, outside the unmodified shared scanner
                raise ValueError(f'{mem} slot {slot}: invalid actor')
            if anim_kind != 10:
                raise ValueError(f'{mem} slot {slot}: unexpected supported-actor animation descriptor')
            count = struct.unpack_from('<I',b,table)[0]
            if not 1 <= count <= 1024 or table+4+count*4 > len(b):
                raise ValueError(f'{mem} slot {slot}: invalid animation table')
            frames = table+4+count*4
            limit = next((p for p in section_offsets if p > table),len(b))
            for index in range(count):
                first,last = struct.unpack_from('<HH',b,table+4+index*4)
                if first > last:
                    raise ValueError(f'{mem} slot {slot} clip {index}: invalid frame range')
                channel_layout(b,frames,len(m['bones']),last,limit)
                out.setdefault((mem,model),[]).append((f'ui_slot{slot}_clip{index:02X}',frames,first,last,1,'channels'))
    return out


def ui_vram(member):
    if int(member,16) not in UI_BUNDLES:
        return None
    import vram
    tim = UI_BUNDLES[int(member,16)][1]
    vr = vram.global_vram()
    images = vram.dt.parse_tims((em.MEMBERS/f'{tim:03X}.bin').read_bytes())
    if not images:
        raise ValueError(f'UI TIM member {tim:03X} has no images')
    for image in images:
        vram.dt.upload(vr,image)
    return vr,tim


def idle_clips():
    out = {}
    b = (em.MEMBERS/'013.bin').read_bytes()
    if len(b) != 8*0xA000:
        raise ValueError('idle member is not eight 20-sector blocks')
    for slot in range(8):
        start,end = slot*0xA000,(slot+1)*0xA000
        pointers = [start+struct.unpack_from('<H',b,start+k)[0]*4 for k in (4,8,12,16,20,24)]
        if struct.unpack_from('<I',b,start)[0] != 6 or not all(start+28 <= p < end for p in pointers):
            raise ValueError(f'idle {slot}: header or block-local pointers invalid')
        states,transitions,modeloff,anim = [start+struct.unpack_from('<H',b,start+k)[0]*4 for k in (8,12,20,24)]
        if not start <= states <= transitions < end or (transitions-states)%8:
            raise ValueError(f'idle {slot}: bad state bounds')
        if not start <= modeloff < anim < end:
            raise ValueError(f'idle {slot}: bad model/frame pointers')
        m = em.parse_actor(b,modeloff)
        nb = struct.unpack_from('<H',b,modeloff+4)[0]
        clips = []
        for index in range((transitions-states)//8):
            first,last,_ = struct.unpack_from('<HHI',b,states+index*8)
            if first > last:
                raise ValueError(f'idle {slot} state {index}: invalid range')
            channel_layout(b,anim,nb,last,end)
            if pointers[0]+(last+1)*72 > states:
                raise ValueError(f'idle {slot} state {index}: player frames cross state table')
            clips.append((f'idle{slot}_state{index:02X}',anim,first,last,1,'channels'))
        if m and m['ok']:
            out[('013',modeloff)] = clips
        elif nb != 1:
            raise ValueError(f'idle {slot}: model outside scanner limits for unexpected reason')
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--export-ui',action='store_true',help='refresh all scanner actors in the proven UI bundles')
    options = parser.parse_args()
    import export_gltf as gl
    found = list(em.scan_members())
    known = gl.clip_metadata()
    if options.export_ui:
        exported = 0
        for member,offset,m in found:
            if int(member,16) in UI_BUNDLES:
                gl.export(member,offset,m,known.get((member,offset),[]),30)
                exported += 1
        print(f'UI exports refreshed={exported}')
    idle = idle_clips()
    ui = ui_clips()
    b = (em.MEMBERS/'013.bin').read_bytes()
    slots = []
    for slot in range(8):
        base = slot*0xA000
        model = base+struct.unpack_from('<H',b,base+20)[0]*4
        nb,vc = struct.unpack_from('<HH',b,model+4)
        states = base+struct.unpack_from('<H',b,base+8)[0]*4
        end = base+struct.unpack_from('<H',b,base+12)[0]*4
        slots.append({'slot':slot,'model':model,'joints':nb,'vertices':vc,'states':(end-states)//8,
                      'scanner_model':('013',model) in idle})
    hashes = Counter()
    rows = []
    for member,offset,m in found:
        source = (em.MEMBERS/f'{member}.bin').read_bytes()
        digest = hashlib.sha256(source[offset:m['end']]).hexdigest()
        hashes[digest] += 1
        rows.append({'member':member,'offset':offset,'joints':len(m['bones']),'vertices':m['vc'],
                     'clips':len(known.get((member,offset),[])),'model_sha256':digest})
    unresolved = [r for r in rows if not r['clips']]
    animated_hashes = {r['model_sha256'] for r in rows if r['clips']}
    duplicate_candidates = sum(r['model_sha256'] in animated_hashes for r in unresolved)
    unresolved_unique = len({r['model_sha256'] for r in unresolved})
    result = {'models':rows,'idle_slots':slots,'unresolved_models':len(unresolved),
              'unresolved_members':dict(Counter(r['member'] for r in unresolved)),
              'unique_model_payloads':len(hashes),'unique_unresolved_payloads':unresolved_unique,
              'unresolved_duplicates_of_animated_models':duplicate_candidates,'failures':[]}
    (ROOT/'extract/codex/actor-sources.json').write_text(json.dumps(result,indent=2)+'\n')
    note = ['# Auxiliary actor sources (generated)','',
            'Generated by `uv run --offline python tools/codex/actor_sources.py`.', '',
            'PlayerUpdate (0x80060780) chooses slot frameCounter&7 and reads 20 sectors at '
            'member 013 start + slot*20 sectors. Thus this member is eight independent 0xA000-byte blocks, '
            'not one member-wide pointer table. PlayerIdleModelSetup (0x80032ADC) uses block-relative '
            'u16 word offsets: +4 player frames, +8 state table, +0xC transitions, +0x10 events, '
            '+0x14 secondary actor model, +0x18 secondary actor channel stream. PlayerUpdateIdleAnim (0x800687D8) '
            'uses the shared player animation frame index for the secondary actor.', '',
            'All eight block-local pointers, state ranges and channel stream extents validate. The secondary actors use ModelPoseBones channels (notes/61), distinct from the player frames at +4. '
            'Six secondary actors meet the existing scanner limits and now receive automatic clips. '
            'Two have one joint and remain outside the shared scanner requirement boneCount>=2; '
            'the original scanner is not modified. This is a supported boundary, not malformed data.', '',
            '| Idle slot | Model offset | Joints | Vertices | State clips | Scanner supported |',
            '|---|---|---|---|---|---|']
    note += [f'| {r["slot"]} | 0x{r["model"]:X} | {r["joints"]} | {r["vertices"]} | {r["states"]} | {r["scanner_model"]} |' for r in slots]
    note += ['', f'Full inventory: {len(rows)} scanner models / {len(hashes)} distinct model byte payloads; '
             f'{len(known)} actors have loader-backed animation associations; {len(unresolved)} remain unresolved.', '',
             f'The unresolved instances reduce to {unresolved_unique} unique model byte payloads. '
             f'{duplicate_candidates} are exact duplicates of animated models, but animation pointers '
             'are not transferred across members solely from that hash match. These are useful investigation candidates.', '',
             'Member 008+0x82F8 is initialized by the RC-car helper (0x80098FB0) through ModelInit, '
             'but that initializer establishes no animation set. Compact UI/scene bundles contain typed '
             'model/animation pointer entries, yet their pairing depends on the particular mode loader. '
             'These entries are not automatically paired merely because they are adjacent. '
             'Unresolved sources retain exported skeletons; explicit --clip accepts raw frame ranges and --channel-clip accepts channel stream ranges. Source format must be established first.', '',
             'Private evidence exports: model-callers.c, idle-update.c, rc-model-init.c under notes/decomp/codex '
             '(ignored). No main-EXE function renaming is proposed.', '',
             '## Remaining sources','', '| Member | Model offset | Joints | Vertices |','|---|---|---|---|']
    note += [f'| {r["member"]} | 0x{r["offset"]:X} | {r["joints"]} | {r["vertices"]} |' for r in unresolved]
    (ROOT/'notes/56-actor-sources.md').write_text('\n'.join(note)+'\n',encoding='utf-8')
    ui_note = ['# Compact stage UI actor bundles (generated)','',
               'Generated by tools/codex/actor_sources.py. Evidence: private stage-title-rendering.c '
               'and ui-area-init-callers.c exports; existing imported model/VRAM parsers.', '',
               'StageStartUi_RunB loads data member 025 by default, 026 for special areas 13/24, '
               'and 027 for selected uncleared destination variants. All use TIM member 024. '
               'DrawWipeB calls StageStartUi_AreaInit(2), proving two active slots. '
               'RunA selects data 037 + 2*k and TIM 036 + 2*k for area kinds '
               'default/2/4/5/7/10/11 (k=0..6). DrawWipeA initializes one actor, except '
               'kind 10 initializes two. Additional descriptors in these files are not '
               'automatically associated because this loader does not initialize their slots.', '',
               'Model descriptors are at +4+slot*4 (u16 word offset, u16 kind=3); animation '
               'descriptors are +0x18+slot*4 (kind=10). The animation header is a u32 clip count '
               'followed by count {first:u16,last:u16} records, then a ModelPoseBones channel stream (notes/61). '
               'The runtime computes frames = table+(count+1)*4. All active descriptor types, '
               'scanner-supported models, clip bounds and frame extents validate. Member 041 slot 1 '
               'is another one-joint model outside the shared scanner; its descriptor kind differs '
               'and no animation association is inferred for it.', '',
               f'Added {len(ui)} distinct actor associations / {sum(map(len,ui.values()))} clips. '
               'Each bundle uses its proven TIM list over global VRAM. UI texture files use '
               'a separate tex_<TIM-member> directory so earlier global-only images are not reused.', '',
               '| Data member | Active slots | TIM member | Distinct models | Clips |',
               '|---|---|---|---|---|']
    for member,(slots,tim) in UI_BUNDLES.items():
        matches = [clips for (mem,_),clips in ui.items() if int(mem,16) == member]
        ui_note.append(f'| {member:03X} | {slots} | {tim:03X} | {len(matches)} | {sum(map(len,matches))} |')
    ui_note += ['', 'These are base actor/state clips; UI script transforms, camera motion, '
                'placement interpolation and runtime swaps are not baked. No actor character '
                'identities are inferred from geometry. Auxiliary bundles used by other modes '
                'remain unresolved until their own loader establishes pairing.']
    (ROOT/'notes/60-ui-actor-bundles.md').write_text('\n'.join(ui_note)+'\n',encoding='utf-8')
    print(f'models={len(rows)} unique={len(hashes)} idle={len(idle)} unresolved={len(unresolved)} failures=0')


if __name__ == '__main__':
    main()
