"""Resolve area load chains, code overlays and dispatched enemy/boss handlers offline.

Generates notes/51-area-overlays.md and extract/codex/area-overlays.json.
--propose appends a reproducible rename batch to notes/codex-inbox.md.
"""
import argparse
import json
import struct
from collections import defaultdict
from pathlib import Path
from codex.area_tables import area_records
from codex.stage_catalog import catalog

ROOT = Path(__file__).resolve().parent.parent
BASE, HDR = 0x80010000, 0x800
PTRS = 0x800A81B0
CHAIN_LO, CHAIN_HI = 0x800A71B0, PTRS
BOSS = {0x44: (1,0x8013DC68,0x801402D8), 0x53: (2,0x8013D1AC,0x8013D57C),
        0x56: (3,0x8013DAE8,0x80143854), 0x46: (4,0x8013DF90,0x8013F1AC),
        0x57: (5,0x8013DA10,0x8013E4B4)}


def symbols():
    out = {}
    for line in (ROOT/'tools/symbols.csv').read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#'):
            address,name,*comment = line.split(',',2)
            out[address.lower()] = (name,','.join(comment))
    return out


def ignored(t):
    return t in (0x61,0x63,0x6D,0x6E,0x84) or 0x72 <= t <= 0xB5


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--propose',action='store_true')
    options = p.parse_args()
    exe = (ROOT/'extract/disc/SCUS_944.23').read_bytes()
    base,size = struct.unpack_from('<II',exe,0x18)
    if base != BASE or size+HDR > len(exe):
        raise ValueError('EXE load header mismatch')
    def read(addr,fmt):
        off = addr-base+HDR
        if off < HDR or off+struct.calcsize(fmt) > size+HDR:
            raise ValueError(f'EXE address out of range: {addr:08X}')
        return struct.unpack_from(fmt,exe,off)
    mapping = {int(o['index'],16):o for o in json.loads((ROOT/'tools/overlay_map.json').read_text())['overlays']}
    names = symbols()
    stages = {r['area']:r for r in catalog()}
    rows,failures,warnings = [],[],[]
    usage = defaultdict(list)
    counts = defaultdict(int)
    def describe(addr,overlay=None):
        key = f'ovl_{overlay:03x}::{addr:08x}' if overlay is not None else f'{addr:08x}'
        return f'0x{addr:08X} {names.get(key,("unlabelled",))[0]}'
    holes = []
    for record in area_records():
        area,ptr,level,kind = [record[k] for k in ('area','pointer','level','kind')]
        if not ptr:
            holes.append(area)
            continue
        chain = []
        cursor = ptr
        for _ in range(16):
            if not CHAIN_LO <= cursor < CHAIN_HI:
                raise ValueError(f'area {area:02X}: chain exceeds record region')
            member,post,dest = read(cursor,'<HhI')
            if post < 0:
                break
            chain.append((member,post,dest))
            cursor += 8
        else:
            raise ValueError(f'area {area:02X}: no chain terminator')
        overlays = [m for m,post,d in chain if m >= 0x180 and post == 0]
        blobs = [m for m,post,d in chain if m < 0x180 and post == 0 and d == 0x80154000]
        if len(overlays) > 1 or len(blobs) != 1:
            failures.append(f'area {area:02X}: overlay/blob count {len(overlays)}/{len(blobs)}')
            continue
        overlay = overlays[0] if overlays else None
        if overlay is not None:
            usage[overlay].append((area,level,kind))
            mapped = mapping[overlay]
            load = int(mapped['load_addr'],16)
            disk_size = (ROOT/f'extract/members/{overlay:03X}.bin').stat().st_size
            if any(d != load for m,_,d in chain if m == overlay):
                failures.append(f'area {area:02X}: mapped overlay destination mismatch')
            def overlay_check(addr):
                if not load <= addr < load+disk_size or addr % 4:
                    failures.append(f'area {area:02X} overlay {overlay:03X}: handler outside member: {addr:08X}')
        b = (ROOT/f'extract/members/{blobs[0]:03X}.bin').read_bytes()
        sec = struct.unpack_from('<I',b,0x4E0)[0]
        handlers,unspawned = [],[]
        if sec+16 <= len(b) and struct.unpack_from('<I',b,sec)[0] == 0x44455A2E:
            n,offset = struct.unpack_from('<H',b,sec+4)[0],struct.unpack_from('<I',b,sec+8)[0]
            if sec+offset+n*0x20 > len(b):
                raise ValueError(f'area {area:02X}: ZED entries exceed member')
            types = sorted({struct.unpack_from('<H',b,sec+offset+i*0x20)[0] for i in range(n)})
            counts['ZED entries'] += n
            for t in types:
                if t >= 0xB7 or ignored(t):
                    unspawned.append(t)
                    continue
                addr = read(0x800AF350+t*4,'<I')[0]
                counts['dispatched type/area pairs'] += 1
                if addr >= 0x8013D000:
                    if overlay is None:
                        failures.append(f'area {area:02X}: type {t:02X} needs overlay but none loads')
                    else:
                        overlay_check(addr)
                        handlers.append({'type':t,'address':addr,'space':f'ovl_{overlay:03X}'})
                        counts['overlay type/area pairs'] += 1
                elif BASE <= addr < base+size and addr % 4 == 0:
                    handlers.append({'type':t,'address':addr,'space':'resident'})
                else:
                    failures.append(f'area {area:02X}: type {t:02X} non-code table value {addr:08X}')
        boss = BOSS.get(area)
        if boss:
            if overlay is None:
                failures.append(f'boss area {area:02X} has no code overlay')
            else:
                overlay_check(boss[1])
                overlay_check(boss[2])
        name = stages[area]['name'] or ('Time Station (runtime notes/04)' if area == 0x58 else f'unnamed hub/special level 0x{level:02X}')
        rows.append({'area':area,'name':name,'level':level,'kind':kind,'pointer':ptr,
                     'member':blobs[0],'overlay':overlay,'chain':chain,'handlers':handlers,
                     'not_spawned_types':unspawned,'boss':boss})
    result = {'areas':rows,'counts':dict(counts),'failures':failures,'warnings':warnings}
    target = ROOT/'extract/codex'
    target.mkdir(parents=True,exist_ok=True)
    (target/'area-overlays.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    note = ['# Areas and code overlays (generated)','',
            'Generated by `uv run --offline python tools/dump_area_overlays.py`; `--propose` appends rename proposals.', '',
            'Sources: SCUS_944.23 area pointer table 0x800A81B0, 8-byte {loadChainPointer, level:u16, kind:u16}; '
            'area records {member:u16, postprocess:s16, destination:u32}, terminated by negative postprocess. '
            'The actual first chain is **0x800A71B0**, earlier than the 0x800A71E0 bound in the overview. '
            'The last chain ends at 0x800A81B0, not 0x800A8190; its final records extend through 0x800A81A8. '
            'The packed id is kind<<16 | level, not a unique world/stage pair (AreaGetLevel/AreaGetKind). '
            'Code overlays are postprocess 0 members >=0x180, cross-checked with overlay_map.json and member size.', '',
            'NULL chain pointers are holes, not a table terminator. The id 0xFFFFFFFF ends the table. '
            f'NULL entries: {", ".join(f"0x{a:02X}" for a in holes)}. '
            'The original dump_areas.area_members stopped at the first NULL (0x34), omitting all later areas; '
            'this tool uses a complete reader in tools/codex/area_tables.py without editing the shared module.', '',
            'ZakoInit reads ZED entries; ZakoTypeIsIgnored excludes 0x61/63/6D/6E and 0x72..B5. '
            'ZakoUpdateLoop calls the EXE handler table 0x800AF350[type] for spawned objects. '
            'Types excluded from that path are listed separately; they are NOT interpreted as function pointers into adjacent EXE data. '
            'A fixed RAM address is resolved in the overlay loaded by this particular area, not all overlays with that address.', '',
            'BossKindForArea (0x8005AB00), BossInit (0x8005AB90), and BossUpdate (0x8005AC28) independently supply five boss init/update pairs. '
            'Overlay map entries at the load base are import roots, not automatically loader callbacks or boss init routines.', '',
            'Stage names are resolved through level -> overlay 0x188 byte table 0x8013B450 -> packed glyph list 0x8013AED0 (notes/28, tools/codex/stage_catalog.py). '
            'All101 active areas validate;86 have stage names. Hub/special levels31..33 have no name in this table; Time Station area58 retains its separately established runtime identity. '
            'Suggested StageXX/AreaXX prefixes remove the arbitrary overlay id without guessing a boss character.', '',
            f'Validation: {len(rows)} areas, {len(usage)} distinct area code overlays, {dict(counts)}; failures={len(failures)}.', '',
            '| Area | Area name / identity | Blob | Code overlay | Boss init / update | Overlay enemy entry points (type: address) |',
            '|---|---|---|---|---|---|']
    for r in rows:
        boss_text = f'{describe(r["boss"][1],r["overlay"])} / {describe(r["boss"][2],r["overlay"])}' if r['boss'] else '-'
        htext = '; '.join(f'{h["type"]:02X}: {describe(h["address"],r["overlay"])}' for h in r['handlers'] if h['space'] != 'resident') or '-'
        note.append(f'| 0x{r["area"]:02X} | {r["name"]} | {r["member"]:03X} | {r["overlay"] and format(r["overlay"],"03X") or "resident only"} | {boss_text} | {htext} |')
    note += ['', '## Overlay reuse and proposed prefixes', '', '| Overlay | Areas | Levels | Prefix |', '|---|---|---|---|']
    prefixes = {}
    for ov,areas in sorted(usage.items()):
        levels = {lev for _,lev,_ in areas}
        prefix = f'Area{areas[0][0]:02X}_Boss' if len(areas) == 1 and areas[0][0] in BOSS else f'Stage{min(levels):02X}' if len(levels) == 1 else 'Areas'+'_'.join(f'{a:02X}' for a,_,_ in areas)
        prefixes[ov] = prefix
        note.append(f'| {ov:03X} | '+', '.join(f'{a:02X}' for a,_,_ in areas)+' | '+', '.join(f'{lev:02X}' for lev in sorted(levels))+f' | {prefix} |')
    note += ['', '## Full enemy dispatch (including resident functions)', '', '| Area | Type | Entry / name |', '|---|---|---|']
    for r in rows:
        for h in r['handlers']:
            ov = r['overlay'] if h['space'] != 'resident' else None
            note.append(f'| {r["area"]:02X} | {h["type"]:02X} | {h["space"]}::{describe(h["address"],ov)} |')
        if r['not_spawned_types']:
            note.append(f'| {r["area"]:02X} | '+','.join(f'{t:02X}' for t in r['not_spawned_types'])+' | excluded from Zako spawn/handler-table path |')
    note += ['', '## Failures', '']+(failures or ['None.'])
    (ROOT/'notes/51-area-overlays.md').write_text('\n'.join(note)+'\n',encoding='utf-8')
    if options.propose and not failures:
        proposals = []
        for addr,(old,comment) in sorted(names.items()):
            if '::' not in addr:
                continue
            space,raw = addr.split('::')
            ov = int(space.removeprefix('ovl_'),16)
            if ov not in prefixes:
                continue
            marker = f'Ovl{ov:03X}_'
            bossmarker = f'Boss{ov:03X}_'
            if old.startswith(marker) or old.startswith(bossmarker):
                suffix = old.split('_',1)[1]
                areas = ', '.join(f'0x{a:02X}' for a,_,_ in usage[ov])
                canonical = f'ovl_{ov:03X}::{raw}'
                proposals.append(f'{canonical},{prefixes[ov]}_{suffix},Rename {old}: overlay loaded by areas {areas}; behavior retained from existing symbol.')
        # Explicit boss entry proposals, except already correctly named functions.
        for r in rows:
            if r['boss']:
                for label,addr in zip(('Init','Update'),r['boss'][1:]):
                    key = f'ovl_{r["overlay"]:03x}::{addr:08x}'
                    if key not in names:
                        canonical = f'ovl_{r["overlay"]:03X}::{addr:08x}'
                        proposals.append(f'{canonical},Area{r["area"]:02X}_Boss{label},Direct dispatch from Boss{label} for BossKindForArea {r["boss"][0]}.')
        header = '## 2026-10-06 — area overlay mapping rename batch [status: open]'
        inbox = ROOT/'notes/codex-inbox.md'
        text = inbox.read_text(encoding='utf-8')
        section = header+'\n\nEvidence: notes/51-area-overlays.md, tools/dump_area_overlays.py; private overlay-dispatch.c. These are renames of existing symbols, with verified area/stage prefixes; retain current behavior suffixes.\n\nsymbols (paste into tools/symbols.csv as a new batch):\n```csv\n'+'\n'.join(proposals)+'\n```\n'
        if header in text:
            before,after = text.split(header,1)
            tail = '\n## '+after.split('\n## ',1)[1] if '\n## ' in after else ''
            text = before+section+tail
            inbox.write_text(text,encoding='utf-8')
        elif '— area overlay mapping rename batch [applied' not in text:
            text += '\n'+section
            inbox.write_text(text,encoding='utf-8')
        print(f'proposals={len(proposals)}')
    print(f'areas={len(rows)} overlays={len(usage)} failures={len(failures)}')
    if failures:
        print('\n'.join(failures))
        raise SystemExit(1)


if __name__ == '__main__':
    main()
