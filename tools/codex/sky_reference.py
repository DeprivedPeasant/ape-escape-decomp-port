"""Test-only original sky policy / integer pre-GTE paths; projection is excluded."""
import struct
from pathlib import Path
from mips_ground_reference import Reference,signed
ROOT=Path(__file__).resolve().parents[2]

class SkyReference:
    def __init__(self):
        self.cpu=Reference((ROOT/'extract/members/05B.bin').read_bytes())

    def policy(self,types,player_y,fog):
        """Execute actual DrawAreaProps type4/5 loop; hook GTE/emit endpoints only."""
        assert types and all(t in (4,5) for t in types)
        m=self.cpu;inst,mesh,ot=0x80110000,0x80111000,0x80120000
        m.write(0x800B4D64,4,inst);m.write(0x800F45AA,2,player_y)
        m.write(0x800EBFC4,2,fog)
        for i,kind in enumerate(types):
            m.write(inst+i*32,4,mesh+i*32);m.write(mesh+i*32,4,kind)
            for j in range(3):m.write(inst+i*32+16+j*2,2,111+i+j)
        events=[]
        def init(pc,r):
            if pc!=0x80017094:return
            r[18]=types[-1];r[19]=0x1F800000;r[20]=len(types)-1
            r[21]=0x800B0000;r[22]=1234
            m.write(r[29]+0x10,4,ot)
        m.trace=init
        def trans(m,r):
            events.append(('zero_translation',))
            r[31]=0x800170C8
            return 0
        def transform(m,r):
            offset=tuple(signed(m.read(0x1F8003C8+j*2,2),16) for j in range(3))
            events.append(('transform',(r[4]-inst)//32,r[5],offset,m.read(0x1F8003C0,2)))
            return 0
        def emit(m,r):events.append(('emit',r[4]-ot,r[5],(r[6]-inst)//32));return 0
        def restore(m,r):events.append(('restore_translation',r[4]));return 0
        def stop(m,r):r[31]=0x800000FC;return 0
        m.call(0x80017094,hooks={0x800170B0:trans,0x80016294:transform,
               0x8001A414:emit,0x8002FB30:restore,0x80017C9C:stop},limit=2000)
        return events,m.read(0x1F8003C0,2)

    def vertices(self,rotation,offset,vertices):
        """Execute first three original quarter-turn vertices, stop before GTE RTPT."""
        assert len(vertices)==3
        m=self.cpu;inst,mesh,verts=0x80110000,0x80111000,0x80112000
        m.write(inst,4,mesh);m.write(inst+12,4,verts);m.write(inst+28,4,rotation)
        m.write(mesh+18,2,3)
        for i,v in enumerate(vertices):
            for j,x in enumerate(v):m.write(verts+i*8+j*2,2,x)
        for j,x in enumerate(offset):m.write(0x1F8003C8+j*2,2,x)
        m.trace=None
        m.call(0x80016294,(inst,0x80113000),hooks={end:lambda m,r:0 for end in
               (0x800167E8,0x80016370,0x800164F0,0x8001666C)},limit=200)
        return [tuple(signed(m.read(0x1F8003D0+i*8+j*2,2),16) for j in range(3)) for i in range(3)]

if __name__=='__main__':
    ref=SkyReference()
    print(ref.policy([4,5,4],-1702,123))
    for rotation in (0,1024,2048,3072):
        print(rotation,ref.vertices(rotation,(0,1702,0),[(1,2,3),(4,5,6),(7,8,9)]))
