"""Prepare original glyph/icon pages and hint sectors for native_text integration."""
import json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import vram
from codex.stage_catalog import catalog,ROOT
OUT=ROOT/'extract/codex/native'

def pack_ui(area):
    row=next(r for r in catalog() if r['area']==area)
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes()
    def read(addr,size):return exe[addr-0x80010000+0x800:addr-0x80010000+0x800+size]
    pages={(0x257,(496<<6)|28),(0x257,(497<<6)|28)}
    for addr,count in [(0x800B3738,16),(0x800B3798,1),(0x800B37A0,1),(0x800B37C0,1)]:
        for i in range(count):pages.add((0x217,struct.unpack_from('<H',read(addr+i*8,8))[0]))
    hints=(ROOT/'extract/members/014.bin').read_bytes()
    assert len(hints)%2048==0
    texts=0
    for i in range(len(hints)//2048):
        sector=hints[i*2048:(i+1)*2048]
        assert 0 in sector
        texts+=bool(any(sector))
    name=(row['name'] or ('Time Station' if area==0x58 else f'Area {area:02X}')).encode()+b'\0'
    codes=bytes.fromhex(row['codes'])+b'\0'
    data=bytearray(struct.pack('<8s5I',b'AEUI1\0\0\0',area,len(name),len(codes),len(hints),len(pages)))
    data+=name+codes+hints
    image=vram.global_vram()
    for page,clut in sorted(pages):
        w,h,rgba=vram.texture(image,page,clut);assert (w,h)==(256,256)
        data+=struct.pack('<II',page,clut)+rgba
    OUT.mkdir(parents=True,exist_ok=True);target=OUT/f'ui_{area:02X}.aeu';target.write_bytes(data)
    report=dict(area=area,name=name[:-1].decode(),pack=str(target),sound_bank=row['sound_bank'],
                pages=len(pages),hint_sectors=len(hints)//2048,nonempty_hints=texts,bytes=len(data),failures=0)
    target.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':print(pack_ui(int(sys.argv[1],0) if len(sys.argv)>1 else 1))
