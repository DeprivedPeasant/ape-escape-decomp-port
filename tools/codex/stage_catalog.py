"""Read resolved stage names and sound banks from original overlay/EXE tables."""
import json, struct, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_text, dump_sound
from codex.area_tables import area_records
ROOT=Path(__file__).resolve().parents[2]

def catalog():
    blob=(ROOT/'extract/members/188.bin').read_bytes()
    start=0x8013AED0-0x80136000
    codes=[]
    for _ in range(22):
        end=blob.index(0,start);codes.append(blob[start:end]);start=end+1
    titles=[dump_text.decode(s).strip() for s in codes]
    assert len(titles)==22 and all('<' not in s for s in titles),titles
    mapping=blob[0x5450:0x5450+31]
    rows=[]
    for record in area_records():
        if not record['pointer']:continue
        level=record['level'];assert level<=33
        index=mapping[level] if level<len(mapping) else 255
        assert index==255 or index<len(titles)
        banks=[m for m,p,d in record['chain'] if p==2]
        assert len(banks)<=1
        if banks:
            chunks=dump_sound.parse((ROOT/f'extract/members/{banks[0]:03X}.bin').read_bytes())
            assert chunks['shd'] and chunks['ssq']
        rows.append(dict(area=record['area'],level=level,kind=record['kind'],
                         name=titles[index] if index!=255 else None,
                         codes=codes[index].hex() if index!=255 else '',sound_bank=banks[0] if banks else None))
    assert len(rows)==101
    return rows

if __name__=='__main__':
    rows=catalog();target=ROOT/'extract/codex/stage-catalog.json'
    target.write_text(json.dumps(rows,indent=2)+'\n')
    print(f'{len(rows)} areas, {sum(r["name"] is not None for r in rows)} named, {sum(r["sound_bank"] is not None for r in rows)} sound banks; failures=0')
