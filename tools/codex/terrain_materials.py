"""Native terrain corner UV/color adapter, recovered from DrawTerrain packet setup.

Keeps the shared OBJ exporter untouched. Samples follow its NW,NE,SW,SE,center
vertex order. Original packet order is NW,SW,NE,SE; bit2 rotates both packet
geometry and attributes, leaving their association with world corners unchanged.
"""
import struct
import dump_areas as da
import export_area_obj as obj


def samples(member):
    blob=(da.MEMBERS/f'{member:03X}.bin').read_bytes()
    ah=struct.unpack_from('<I',blob,0x4D0)[0]
    flags,offset=struct.unpack_from('<II',blob,ah)
    if flags&3==3:return []
    grid=ah+offset*4
    width,depth,ncolors=struct.unpack_from('<HHI',blob,grid)
    cells=grid+8;colors=cells+width*depth*4;tiles=colors+ncolors*4
    table=obj.uv_table();out=[]
    for i in range(width*depth):
        word=struct.unpack_from('<I',blob,cells+i*4)[0]
        orientation=word>>7&15
        if orientation>=len(table):raise ValueError('terrain UV index outside original table')
        tile=word>>3&15;tu,tv=(tile%4)*64,(tile//4)*64
        uv=[table[orientation][j] for j in (0,2,1,3)]
        indices=[blob[tiles+i*8+j] for j in (0,6,1,7)]
        if word&1:
            if any(j>=ncolors for j in indices):raise ValueError('terrain color index out of range')
            rgb=[tuple(blob[colors+j*4:colors+j*4+3]) for j in indices]
        else:rgb=[(128,128,128)]*4
        rgb.append(tuple(sum(c[k] for c in rgb)//4 for k in range(3)))
        uv.append(tuple(sum(p[k] for p in uv)/4 for k in range(2)))
        out.append([((tu+u+(.5 if j<4 and u==0 else 0))/256,
                     1-(tv+v+(.5 if j<4 and v==0 else 0))/256,
                     *(c/255 for c in rgb[j])) for j,(u,v) in enumerate(uv)])
    return out
