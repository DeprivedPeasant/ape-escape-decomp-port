"""Compare native terrain attributes with original EXE instruction fragments.

Original geometry-copy and UV/color setup run separately, stopping before GTE
projection/culling/draw calls. Only the original byte stream is the oracle.
"""
import collections
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
import dump_tims as dt
import export_area_obj as obj
import vram as vr
from codex.mips_ground_reference import Reference
from codex.pack_area import OUT,png_rgba,pack
from codex.terrain_materials import samples


class Stop(Exception):pass


def fragment(cpu,start,end,registers):
    def trace(pc,r):
        if pc==start:
            for register,value in registers.items():r[register]=value
        if pc==end:raise Stop
    cpu.trace=trace
    try:cpu.call(start,limit=1000)
    except Stop:return
    raise AssertionError('original terrain fragment missed stop address')


def original(cpu,word,tilebytes):
    s=0x1F800000;grid=0x80130000;tileptr=0x80130100
    cpu.write(s+0x2c8,4,word);cpu.write(s+0x2cc,4,word>>3&15)
    # Identifiable projected corners in a 3x3 block: row-major X then Z.
    for j in range(9):
        cpu.write(s+0x220+j*8,4,j);cpu.write(s+0x224+j*8,4,1)
    fragment(cpu,0x80044A30,0x80044B98,{16:0,17:s})
    geometry=[cpu.read(s+0x200+j*8,4) for j in range(4)]
    cpu.write(s+0x2a4,2,1);cpu.write(grid+0x10,4,tileptr)
    for j,value in enumerate(tilebytes):cpu.write(tileptr+j,1,value)
    fragment(cpu,0x80044CE0,0x80044E3C,{17:s,18:0,20:grid,21:0,22:0x800AF060,8:0})
    uv=[(cpu.read(s+0x298+j*2,1),cpu.read(s+0x299+j*2,1)) for j in range(4)]
    color=[cpu.read(s+0x2b4+j,1) for j in range(4)]
    bycorner={geometry[j]:(uv[j],color[j]) for j in range(4)}
    return [bycorner[k] for k in (0,1,3,4)]


def main():
    members=da.area_members();cpu=Reference((da.MEMBERS/f'{members[1]:03X}.bin').read_bytes())
    oracle={};table=obj.uv_table()
    for tile in range(16):
        for orientation in range(8):
            for mirror in (0,4):
                word=1|(tile<<3)|(orientation<<7)|mirror
                result=original(cpu,word,bytes(range(8)))
                expected=[((tile%4*64+table[orientation][j][0],tile//4*64+table[orientation][j][1]),k)
                          for j,k in zip((0,2,1,3),(0,6,1,7))]
                assert result==expected,(word,result,expected)
                oracle[(tile,orientation,bool(mirror))]=result
    rows=[];corners=0;pixel_checks=0;packed_vertices=0;orientations=collections.Counter();mirrors=0
    for area,member in sorted(members.items()):
        b=(da.MEMBERS/f'{member:03X}.bin').read_bytes();h=struct.unpack_from('<I',b,0x4D0)[0]
        flags,offset=struct.unpack_from('<II',b,h)
        if flags&3==3 or flags&1:continue
        g=h+offset*4;w,d,n=struct.unpack_from('<HHI',b,g);cells=g+8
        colors=cells+w*d*4;tiles=colors+n*4
        adapted=samples(member);drawn=0
        vram=vr.area_vram(area);atlas=png_rgba(obj.OUT/f'area_{area:02X}/terrain_atlas.png')
        # Check every atlas texel independently against packed VRAM nibbles/CLUTs.
        for v in range(256):
            for u in range(256):
                tile=(v//64)*4+u//64
                packed=struct.unpack_from('<H',vram,((256+v)*1024+512+u//4)*2)[0]
                index=packed>>(4*(u&3))&15
                color=struct.unpack_from('<H',vram,((240+tile)*1024+880+index)*2)[0]
                o=((255-v)*256+u)*4
                assert atlas[o:o+4]==bytes(dt.c16(color)),(area,u,v)
                pixel_checks+=1
        for i in range(w*d):
            word=struct.unpack_from('<I',b,cells+i*4)[0]
            if not word&1:continue
            tile=word>>3&15;orientation=word>>7&15;mirror=bool(word&4)
            result=oracle[(tile,orientation,mirror)];tilebytes=b[tiles+i*8:tiles+i*8+8]
            for j,((u,v),index) in enumerate(result):
                # Adapter retains existing corner-edge pixel-center convention.
                cu,cv=table[orientation][(0,2,1,3)[j]]
                expected=((u+(.5 if cu==0 else 0))/256,1-(v+(.5 if cv==0 else 0))/256)
                assert adapted[i][j][:2]==expected,(area,i,j)
                ci=tilebytes[index]
                assert adapted[i][j][2:]==tuple(c/255 for c in b[colors+ci*4:colors+ci*4+3])
                corners+=1
            drawn+=1;orientations[orientation]+=1;mirrors+=mirror
        # Verify serialized native attributes and their actual geometric corners,
        # rather than stopping at the intermediate Python adapter.
        packed=Path(pack(area)['pack']).read_bytes()
        nv,nb=struct.unpack_from('<2I',packed,20);vstart=64+nb*16
        actual=[]
        for batch in range(nb):
            first,count,texture,kind=struct.unpack_from('<4I',packed,64+batch*16)
            if kind==0:
                actual.extend(struct.unpack_from('<8f',packed,vstart+(first+j)*32) for j in range(count))
        assert len(actual)==drawn*12
        cursor=0
        for i in range(w*d):
            if not struct.unpack_from('<I',b,cells+i*4)[0]&1:continue
            x0=-w+i%w*2;z0=-d+i//w*2
            xz=((x0,z0),(x0+2,z0),(x0,z0+2),(x0+2,z0+2),(x0+1,z0+1))
            for j in (4,0,1,4,1,3,4,3,2,4,2,0):
                vertex=actual[cursor];cursor+=1
                assert (vertex[0],vertex[2])==xz[j],(area,i,j,'position')
                expected=(*adapted[i][j][:2],*(min(1,c*2) for c in adapted[i][j][2:]))
                assert all(abs(a-e)<1e-6 for a,e in zip(vertex[3:],expected)),(area,i,j,'attributes')
                packed_vertices+=1
        rows.append(dict(area=area,drawn_cells=drawn))
        print(f'area {area:02X}: {drawn} visible cells, atlas and original corner attributes pass',flush=True)
    report=dict(areas=rows,original_configurations=len(oracle),corner_checks=corners,
                atlas_texels=pixel_checks,packed_vertices=packed_vertices,orientations=dict(orientations),mirrored_cells=mirrors,failures=[])
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'terrain-material-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS {corners} corners, {pixel_checks} atlas texels, {packed_vertices} serialized vertices, {len(oracle)} original instruction configurations')


if __name__=='__main__':main()
