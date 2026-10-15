"""Disassemble EffectScriptUpdate's u16 command stream using the EXE script pointer table.

Offline: generates notes/52-effect-scripts.md, extract/codex/effect-scripts.txt/.json.
Only opcodes proven by the interpreter are accepted. Not a particle VM emulator.
"""
import argparse
import json
import struct
from collections import Counter,defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE, END = 0x800B175C, 0x800B1828
# Direct SpawnEffectGroup callers in the private Ghidra project pass these tables.
# Each local table is immediately after its contiguous script pool. Counts stop
# before unrelated data/function pointers, not at an assumed NULL sentinel.
OVERLAY_TABLES = [('18E',0x801426C0,3),('198',0x801449D4,1),
                  ('19E',0x80143718,15),('1A0',0x80141640,8),
                  ('1A1',0x801409FC,4),('1A6',0x80143F00,6),('1A8',0x80141F88,10)]
# opcode: (name, number of operand halfwords, observed behavior)
OPS = {
    1: ('LOOP',1,'Push loop body pointer and u8 repeat count; maximum two active loop levels.'),
    2: ('LOOP_RANDOM',2,'Push loop count = base + (rand8*spread>>8), truncated to u8.'),
    3: ('END_LOOP',0,'Decrement current count; jump to body while nonzero, otherwise pop.'),
    4: ('END',0,'Clear particle type/active byte; stop interpreter.'),
    8: ('ADD_POSITION',3,'Add signed x,y,z to position at slot +0x14/+0x16/+0x18.'),
    9: ('ADD_POSITION_RANDOM',6,'Add rand8*spread>>8 - bias for x,y,z (spread,bias pairs).'),
    10: ('WAIT_FIXED_POSITION',1,'Set slot+1=1; wait operand-1 frames, yielding this update.'),
    11: ('WAIT_FIXED_POSITION_RANDOM',2,'Same, wait base + rand8*spread>>8 -1.'),
    12: ('YIELD_CLEAR_WAIT',0,'Set slot+1=0, timer low/high bytes=0; yield this update.'),
    13: ('WAIT_MOVING',1,'Set slot+1=0; wait operand-1 frames, yielding this update.'),
    14: ('WAIT_MOVING_RANDOM',2,'Same, wait base + rand8*spread>>8 -1.'),
    15: ('SET_VELOCITY',3,'Set signed x,y,z velocity at +0x1C/+0x1E/+0x20.'),
    16: ('SET_VELOCITY_X',1,'Set velocity x.'),
    17: ('SET_VELOCITY_Y',1,'Set velocity y.'),
    18: ('SET_VELOCITY_Z',1,'Set velocity z.'),
    19: ('SET_VELOCITY_RANDOM',6,'Set each component to rand8*spread>>8 - bias.'),
    20: ('SET_VELOCITY_X_RANDOM',2,'Set x to rand8*spread>>8 - bias.'),
    21: ('SET_VELOCITY_Y_RANDOM',2,'Set y to rand8*spread>>8 - bias.'),
    22: ('SET_VELOCITY_Z_RANDOM',2,'Set z to rand8*spread>>8 - bias.'),
    23: ('ADD_VELOCITY',3,'Add signed x,y,z to velocity.'),
    24: ('SCALE_VELOCITY',3,'Multiply each signed velocity by operand and arithmetic shift right 12.'),
    25: ('SET_ANCHOR_MODE_0',0,'Set slot+2=0 (world position).'),
    26: ('SET_ANCHOR_MODE_1',0,'Set slot+2=1 (position relative to player).'),
    27: ('SET_ANCHOR_MODE_2',0,'Set slot+2=2 (position relative to player y-0x80).'),
    30: ('SET_UV_RECT',2,'First operand packs u/v; second packs width/height increments for four UV corners.'),
    31: ('ADD_U',1,'Add operand low byte to all four u coordinates.'),
    32: ('ADD_V',1,'Add operand low byte to all four v coordinates.'),
    33: ('SET_SIZE',2,'Set slot+0x12/+0x13 byte width,height.'),
    34: ('SET_SIZE_RANDOM',4,'Set width and height from separate base,spread pairs.'),
    35: ('SET_SIZE_SQUARE_RANDOM',2,'Set width=height from one base,spread pair.'),
    36: ('SCALE_SIZE',2,'Scale unsigned byte width,height with signed 12-bit fixed-point factors.'),
    37: ('SET_RGB',3,'Replace low 24 bits of slot+0x30; retain command byte.'),
    38: ('SET_GREEN_RANDOM',2,'Replace green byte with base + rand8*spread>>8 (code does not choose all RGB channels).'),
    39: ('SET_COLOR_FADE',1,'Set signed byte fade at +0x1A, applied to all RGB channels each update.'),
    40: ('SET_COLOR_FADE_RANDOM',2,'Set fade byte to base + rand8*spread>>8.'),
    41: ('OPAQUE',0,'Clear command bit1 at +0x33.'),
    42: ('BLEND_HALF',0,'Set semi-transparent bit1; set TPAGE=0x1C at +0x2A.'),
    43: ('BLEND_ADD',0,'Set semi-transparent bit1; TPAGE=0x3C.'),
    44: ('BLEND_SUBTRACT',0,'Set semi-transparent bit1; TPAGE=0x5C.'),
    45: ('BLEND_ADD_QUARTER',0,'Set semi-transparent bit1; TPAGE=0x7C.'),
    46: ('COLOR_MODULATED',0,'Clear command bit0 at +0x33.'),
    47: ('COLOR_RAW',0,'Set command bit0 at +0x33.'),
    48: ('SET_CLUT_ROW',1,'CLUT = ((row+0x1F0)*0x40)|0x2C.'),
    49: ('SET_CLUT_ROW_RANDOM',2,'CLUT row from base + rand8*spread>>8.'),
    51: ('KILL_AT_GROUND_OR_WATER',0,'Conditional particle kill/burst below player groundY or water threshold.'),
    52: ('DRAW_RIBBON',1,'Draw velocity-aligned ribbon using 0x800B1B78+index*8 and byte width.'),
}


def disassemble(blob,offset,limit=None):
    """Returns instructions/end; validates bounded operands, END and structured loops."""
    limit = len(blob) if limit is None else min(limit,len(blob))
    records,stack = [],[]
    cursor = offset
    while cursor+2 <= limit:
        opcode = struct.unpack_from('<H',blob,cursor)[0]
        if opcode not in OPS:
            raise ValueError(f'unknown opcode {opcode} at +0x{cursor:X}')
        name,n,_ = OPS[opcode]
        end = cursor+2*(n+1)
        if end > limit:
            raise ValueError(f'truncated {name} at +0x{cursor:X}')
        operands = struct.unpack_from('<'+'h'*n,blob,cursor+2) if n else ()
        record = {'offset':cursor,'opcode':opcode,'name':name,'operands':operands,'bytes':end-cursor}
        if opcode in (1,2):
            if len(stack) >= 2:
                raise ValueError(f'loop nesting exceeds two at +0x{cursor:X}')
            stack.append(cursor)
        elif opcode == 3:
            if not stack:
                raise ValueError(f'END_LOOP without LOOP at +0x{cursor:X}')
            record['loop_start'] = stack.pop()
        records.append(record)
        cursor = end
        if opcode == 4:
            if stack:
                # END kills the slot regardless of depth, but report it separately.
                record['open_loops_on_end'] = len(stack)
            return records,cursor
        if len(records) > 4096:
            raise ValueError('instruction limit exceeded (missing END)')
    raise ValueError(f'no END terminator before bound +0x{limit:X}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--propose',action='store_true')
    options = parser.parse_args()
    exe = (ROOT/'extract/disc/SCUS_944.23').read_bytes()
    base,size = struct.unpack_from('<II',exe,0x18)
    addr_to_off = lambda a:a-base+0x800
    u32 = lambda a:struct.unpack_from('<I',exe,addr_to_off(a))[0]
    starts = defaultdict(list)
    null_types = []
    for t in range((END-TABLE)//4):
        address = u32(TABLE+t*4)
        if address:
            if not base <= address < base+size or address % 2:
                raise ValueError(f'type {t}: invalid script pointer {address:08X}')
            starts[address].append(t)
        else:
            null_types.append(t)
    sources = [('EXE',exe,base-0x800,starts,TABLE)]
    table_inventory = [{'source':'EXE','address':TABLE,'entries':(END-TABLE)//4,'nonnull':sum(map(len,starts.values()))}]
    for mem,table,n in OVERLAY_TABLES:
        blob = (ROOT/f'extract/members/{mem}.bin').read_bytes()
        load = 0x8013D000
        local_starts = defaultdict(list)
        for i in range(n):
            address = struct.unpack_from('<I',blob,table-load+i*4)[0]
            if not load <= address < table or address % 2:
                raise ValueError(f'overlay {mem} table entry {i}: invalid pointer {address:08X}')
            local_starts[address].append(200+i)
        sources.append((mem,blob,load,local_starts,table))
        table_inventory.append({'source':mem,'address':table,'entries':n,'nonnull':n})
    scripts,failures = [],[]
    counts = Counter()
    listing = []
    for source,blob,origin,pointers,table in sources:
        addresses = sorted(pointers)
        for i,address in enumerate(addresses):
            bound = addresses[i+1] if i+1 < len(addresses) else table
            try:
                commands,end = disassemble(blob,address-origin,bound-origin)
                for c in commands:
                    c['address'] = c['offset']+origin
                    counts[c['opcode']] += 1
                end_address = end+origin
                padding = b'' if source == 'EXE' and i+1 == len(addresses) else blob[end:bound-origin]
                scripts.append({'source':source,'address':address,'types':pointers[address],'end':end_address,
                                'commands':commands,'padding_bytes':len(padding),'padding_nonzero':sum(bool(x) for x in padding)})
                listing.append(f'\nscript {source}::0x{address:08X}; types '+','.join(f'0x{t:02X}' for t in pointers[address]))
                for c in commands:
                    listing.append(f'  {c["address"]:08X}: {c["name"]:<30} '+', '.join(str(x) for x in c['operands']))
            except ValueError as error:
                failures.append(f'{source}::{address:08X}: {error}')
    # Validate raw pool contiguity: only zero alignment padding is allowed between known scripts.
    for s in scripts:
        if s['padding_nonzero']:
            failures.append(f'{s["source"]}::{s["address"]:08X}: nonzero bytes after END before next table script')
    # Exhaustively enumerate pointers to discovered scripts in EXE and all archive members.
    # These are raw pointer candidates, not claims that every matching word is executed.
    references = []
    datasets = [('EXE',exe,base-0x800)]
    datasets += [(f.stem,f.read_bytes(),0) for f in sorted((ROOT/'extract/members').glob('*.bin'))]
    for source,blob,origin in datasets:
        known = starts if source == 'EXE' or source not in {s[0] for s in sources[1:]} else next(s[3] for s in sources if s[0] == source)
        for o in range(0,len(blob)-3,4):
            value = struct.unpack_from('<I',blob,o)[0]
            if value in known:
                references.append({'source':source,'offset':o,'address':origin+o if source == 'EXE' else None,'script':value})
    output = ROOT/'extract/codex'
    output.mkdir(parents=True,exist_ok=True)
    result = {'tables':table_inventory,'scripts':scripts,'null_types':null_types,'opcodes':dict(counts),'references':references,'failures':failures}
    (output/'effect-scripts.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    (output/'effect-scripts.txt').write_text('\n'.join(listing)+'\n',encoding='utf-8')
    note = ['# Effect particle script bytecode (generated)','',
            'Generated by `uv run --offline python tools/dump_effect_scripts.py`. Full game-data disassembly is kept gitignored at extract/codex/effect-scripts.txt/.json.', '',
            'Evidence: private headless exports of EffectScriptUpdate (0x80069BEC), SpawnOneParticle (0x8006962C), '
            'ParticleAlloc (0x80069548), ParticlesDraw (0x80069920), RibbonDraw (0x8006F3D0), and spawn wrappers. '
            'No extracted/decompiled code is committed. Opcode/operand facts are transcribed from switch(*pc-1) and actual pc increments, then validated on the EXE streams.', '',
            'ExportCallers.java (tools/codex/ghidra_scripts) exported 33 roots/direct callers, including named overlay functions. '
            'Every direct SpawnEffectGroup call uses either the default table or one of the seven overlay tables below. '
            'Overlay-local types start at 200 (0xC8); table pointers are resolved in their own address space. '
            'The decompiler auto-label PTR_DAT_<script>_<table> contains BOTH a pointed-to address and the table address; the final address is the table location.', '',
            'Encoding: little-endian u16 opcode; operands are 16-bit words (signed where arithmetic requires it). '
            'There is no byte opcode, length header, pointer jump or padding within an instruction. '
            'Opcodes absent from the interpreter switch are illegal: 0,5,6,7,28,29,50 and >52. '
            'END=4 terminates the slot; END_LOOP=3 closes a structured loop. WAIT opcodes yield; END is not a wait.', '',
            'SpawnParticle passes script table 0x800B175C to SpawnEffectGroup, which invokes SpawnOneParticle for types <0x100. '
            'SpawnOneParticle sets slot+4 to table[type] for type<200, or table[type-200] otherwise. '
            'The default table runs through 0x800B1824 (null sentinel), before the group table at 0x800B1828. '
            'Types 0/1 are null, and type 8/9 share a stream. Additional type values are not assumed valid unless the table contains them.', '',
            'Slot stride is 0x34, 100 slots at 0x800B9900. +0 active/type, +1 movement freeze flag, +2 anchor mode, '
            '+3 signed loop depth initially -1, +4 pc, +8/+0xC loop pc stack, +0x10/+0x11 loop counts, '
            '+0x12/+0x13 dimensions, +0x14 position (s16[3]), +0x1A fade, +0x1C velocity (s16[3]), '
            '+0x22 wait timer, +0x24/+0x28/+0x2C/+0x2E UV pairs, +0x26 CLUT, +0x2A TPAGE, +0x30 RGB/GPU command. '
            'Update draws particles before executing scripts. It then walks pc until a yield or kill, saves pc, integrates velocity unless +1 is set, and applies clamped RGB fading.', '',
            'Random helper: unsigned 16-bit seed = seed*5+1, rand8=seed>>8. Spread/bias operands are signed; '
            'loop counts and sizes truncate to bytes. Freeze waits suppress automatic position integration for their duration. '
            'The ribbon command draws through velocity*3; conditional ground/water kill can terminate before the explicit END.', '',
            f'Validation: {len(scripts)} distinct table streams across {len(sources)} tables, {sum(t["nonnull"] for t in table_inventory)} nonnull type entries; '
            f'{sum(counts.values())} instructions, {len(counts)} observed opcodes / {len(OPS)} legal opcodes; failures={len(failures)}.',
            'Every discovered stream decodes to explicit END within the next stream boundary, with valid operand widths and <=2 nested loops. '
            'Raw-pointer cross-references are searched over the full EXE and all members (including overlays); '
            'they do not establish that byte patterns in arbitrary area data are scripts.', '',
            '| Opcode | Name | Operand words | Total bytes | Observed | Behavior |',
            '|---|---|---|---|---|---|']
    for opcode,(name,n,behavior) in OPS.items():
        note.append(f'| {opcode} / 0x{opcode:02X} | {name} | {n} | {2*(n+1)} | {counts[opcode]} | {behavior} |')
    note += ['', '## Script tables','', '| Source | Table address | Entries | Nonnull |','|---|---|---|---|']
    for table in table_inventory:
        note.append(f'| {table["source"]} | 0x{table["address"]:08X} | {table["entries"]} | {table["nonnull"]} |')
    note += ['', '## Script inventory', '', '| Address space / address | Particle types | Bytes through END | Instructions | Padding before next stream |', '|---|---|---|---|---|']
    for s in scripts:
        note.append(f'| {s["source"]}::0x{s["address"]:08X} | '+','.join(f'0x{t:02X}' for t in s['types'])+f' | {s["end"]-s["address"]} | {len(s["commands"])} | {s["padding_bytes"]} |')
    note += ['', '## Pointer locations', '', '| Source | Pointer offset/address | Script |', '|---|---|---|']
    for ref in references:
        location = f'0x{ref["address"]:08X}' if ref['address'] is not None else f'+0x{ref["offset"]:X}'
        note.append(f'| {ref["source"]} | {location} | 0x{ref["script"]:08X} |')
    note += ['', '## Coverage limits','',
             'All streams in the default particle table and seven tables used by every known direct caller are accounted for. Group lists (kinds 0x32/0x33) and effect-object definitions '
             'are different formats and are not scripts for this interpreter. Raw bytecode without an absolute pointer to a known stream '
             'cannot be reliably distinguished from geometry. The direct caller audit finds no scripts in area blobs; '
             'unknown indirect calls or currently unused tables are not ruled out. No speculative scans are counted as validated scripts.', '',
             '## Failures','']+(failures or ['None.'])
    validation = output/'effect-validation.json'
    if validation.exists():
        checks = json.loads(validation.read_text())
        note += ['', '## Decoder rejection checks','',
                 'Run `uv run --offline python tools/codex/check_effect_scripts.py` before regenerating this note.',
                 f'{checks["real_stream_boundaries_verified"]} real stream boundaries independently re-decoded; '
                 f'all {checks["real_streams_missing_end_rejected"]} streams fail when the END is removed. '
                 f'{checks["synthetic_corruptions_rejected"]} malformed unknown-opcode, operand-width, missing-END and loop cases rejected.']
    (ROOT/'notes/52-effect-scripts.md').write_text('\n'.join(note)+'\n',encoding='utf-8')
    if options.propose and not failures:
        header = '## 2026-10-06 — particle bytecode data labels [status: open]'
        inbox = ROOT/'notes/codex-inbox.md'
        text = inbox.read_text(encoding='utf-8')
        existing = {line.split(',',1)[0].lower() for line in (ROOT/'tools/symbols.csv').read_text().splitlines() if line and not line.startswith('#')}
        proposals = []
        for address,name,comment in [(TABLE,'g_ParticleScriptTable','Particle type -> u16 script stream pointer; default table passed by SpawnParticle'),
                                     (0x800B9900,'g_Particles','100 slots of 0x34 bytes; EffectScriptUpdate bytecode particle instances')]:
            if f'{address:08x}' not in existing:
                proposals.append(f'{address:08x},{name},{comment}')
        for s in scripts:
            address = s['address']
            canonical = f'{address:08x}' if s['source'] == 'EXE' else f'ovl_{s["source"]}::{address:08x}'
            if canonical.lower() not in existing:
                suffix = '' if s['source'] == 'EXE' else f'_{s["source"]}'
                proposals.append(f'{canonical},g_ParticleScript{suffix}_{s["types"][0]:02X},Validated u16 effect stream for particle types '+ '/'.join(f'{t:02X}' for t in s['types']))
        for mem,table,n in OVERLAY_TABLES:
            canonical = f'ovl_{mem}::{table:08x}'
            if canonical.lower() not in existing:
                proposals.append(f'{canonical},g_ParticleScriptTable_{mem},Local particle script pointers for types 200..{199+n}')
        section = header+'\n\nEvidence: notes/52-effect-scripts.md, tools/dump_effect_scripts.py; private effects-callees.c / lane-roots.c / effect-callers.c.\n\nsymbols (paste into tools/symbols.csv as a new batch):\n```csv\n'+'\n'.join(proposals)+'\n```\n'
        if header in text:
            before,after = text.split(header,1)
            tail = '\n## '+after.split('\n## ',1)[1] if '\n## ' in after else ''
            text = before+section+tail
            inbox.write_text(text,encoding='utf-8')
        elif '— particle bytecode data labels [applied' not in text:
            text += '\n'+section
            inbox.write_text(text,encoding='utf-8')
        print(f'proposals={len(proposals)}')
    print(f'scripts={len(scripts)} instructions={sum(counts.values())} opcodes={len(counts)} failures={len(failures)} refs={len(references)}')
    if failures:
        print('\n'.join(failures))
        raise SystemExit(1)


if __name__ == '__main__':
    main()
