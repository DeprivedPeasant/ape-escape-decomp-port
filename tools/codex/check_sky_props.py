"""Actual sky metadata and original integer draw-policy / pre-GTE differential."""
import collections,ctypes as C,json,os,random,shutil,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from codex import sky_props
import dump_areas as da
from sky_reference import SkyReference
from mips_ground_reference import signed
OUT=ROOT/'extract/codex/native'
class Policy(C.Structure):
    _fields_=[(name,C.c_int) for name in ('placement','translation','fog','bucket')]

def main():
    dll=OUT/f'sky_{os.getpid()}.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    str(ROOT/'src/render/native_sky.c'),'-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));vec=C.c_int16*3
    lib.ae_sky_vertex.argtypes=[C.c_int,C.c_uint16,C.c_int16,C.POINTER(C.c_int16),C.POINTER(C.c_int16)]
    lib.ae_sky_policy.argtypes=[C.c_int,C.POINTER(Policy)]
    ref=SkyReference();rng=random.Random(9442345);counts=collections.Counter();records=[];areas=0;vertices=0;triangles=0
    for area,member in sorted(da.area_members().items()):
        blob=(da.MEMBERS/f'{member:03X}.bin').read_bytes();u=lambda o:struct.unpack_from('<I',blob,o)[0]
        ah=u(0x4D0);mt=ah+u(ah+8)*4;pt=ah+u(ah+12)*4;expected=[]
        for i in range(u(pt)):
            row=struct.unpack_from('<HHhhhH',blob,pt+4+i*12);mesh=row[0]
            if mesh>=u(mt):continue
            m=mt+struct.unpack_from('<H',blob,mt+4+mesh*4)[0]*4
            if blob[m] in (4,5):expected.append((i,mesh,m,row))
        found=sky_props.load(member);assert len(found)==len(expected)
        for r,(i,mesh,m,row) in zip(found,expected):
            assert r['prop_index']==i and r['mesh_index']==mesh and r['mesh_flags']==u(m)
            assert r['type']==blob[m] and r['rotation']==row[1] and r['placement']==row[2:5]
            assert r['placement_ignored'] and r['original_quarter_turn']==row[1]>>10&3
            vofs,vc=struct.unpack_from('<HH',blob,m+16)
            raw=[struct.unpack_from('<hhh',blob,m+vofs*4+j*8) for j in range(vc)]
            assert r['vertex_count']==vc and r['vertices']==raw
            # Independently walk primitive records/vertex index words, not metadata's parsed triangles.
            p=m+struct.unpack_from('<H',blob,m+20)[0]*4;tris=[]
            from export_model import STRIDE
            while blob[p+3]&0xFC in STRIDE:
                code=blob[p+3]&0xFC;n=4 if code in (0x28,0x2C,0x38,0x3C) else 3
                v=struct.unpack_from('<'+str(n)+'H',blob,p+4)
                tris.append(v[:3])
                if n==4:tris.append((v[2],v[1],v[3]))
                p+=STRIDE[code]
            assert r['triangles']==tris,(area,i)
            counts[r['type']]+=1;vertices+=vc;triangles+=len(tris);records.append(r)
        areas+=1
    policy_cases=0;calls=0
    for case in range(2000):
        types=[rng.choice((4,5)) for _ in range(rng.randrange(1,5))]
        y=rng.randrange(-32768,32768);fog=rng.randrange(65536)
        events,restored=ref.policy(types,y,fog);assert restored==fog
        wanted=[]
        for i in reversed(range(len(types))):
            p=Policy(9,8,7,6);assert lib.ae_sky_policy(types[i],C.byref(p))==1
            assert (p.placement,p.translation,p.fog,p.bucket)==(0,1,0,1)
            wanted += [('zero_translation',),('transform',i,0x800E7D00,(0,signed(-y,16) if types[i]==4 else 0,0),0),
                       ('emit',0x3FFC,0x800E7D00,i),('restore_translation',0x800E78C0)]
        assert events==wanted;policy_cases+=1;calls+=len(events)
    math_cases=0
    cases=[(t,rotation,y,tuple(rng.choice((-32768,-1,0,1,32767)) for _ in range(3)))
           for t in (4,5) for rotation in (0,1,1023,1024,2047,2048,3071,3072,4095,65535)
           for y in (-32768,-1,0,1,32767)]
    cases += [(rng.choice((4,5)),rng.randrange(65536),rng.randrange(-32768,32768),
               tuple(rng.randrange(-32768,32768) for _ in range(3))) for _ in range(8000)]
    cases += [(r['type'],r['rotation'],-1702,v) for r in records for v in r['vertices']]
    for kind,rot,y,v in cases:
        expected=ref.vertices(rot,(0,signed(-y,16) if kind==4 else 0,0),[v]*3)[0]
        inp,out=vec(*v),vec(19,23,29)
        assert lib.ae_sky_vertex(kind,rot,y,inp,out)==1 and tuple(out)==expected,(kind,rot,y,v,tuple(out),expected)
        assert lib.ae_sky_vertex(kind,rot,y,inp,inp)==1 and tuple(inp)==expected
        math_cases+=1
    guards=0
    for kind,inp,out in ((3,vec(1,2,3),vec(19,23,29)),(4,None,vec(19,23,29)),(5,vec(1,2,3),None)):
        assert lib.ae_sky_vertex(kind,0,0,inp,out)==0
        if out is not None:assert tuple(out)==(19,23,29)
        guards+=1
    p=Policy(9,8,7,6);assert lib.ae_sky_policy(3,C.byref(p))==0 and (p.placement,p.translation,p.fog,p.bucket)==(9,8,7,6)
    assert lib.ae_sky_policy(4,None)==0;guards+=2
    blob=(da.MEMBERS/'05B.bin').read_bytes()
    bad_header=bytearray(blob);struct.pack_into('<I',bad_header,0x4D0,len(blob))
    ah=struct.unpack_from('<I',blob,0x4D0)[0]
    bad_count=bytearray(blob);struct.pack_into('<I',bad_count,ah+12,0x7FFFFFFF)
    for damaged in (blob[:0],blob[:0x4D0],bad_header,bad_count):
        try:sky_props._load_blob(damaged)
        except ValueError:guards+=1
        else:raise AssertionError('truncated source accepted')
    report=dict(areas=areas,sky_placements=sum(counts.values()),types=dict(counts),raw_vertices=vertices,
                raw_triangles=triangles,original_draw_policy_cases=policy_cases,ordered_endpoint_calls=calls,
                original_pre_gte_vertex_cases=math_cases,inplace_cases=math_cases,guards=guards,failures=0)
    (OUT/'sky-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (ROOT/'notes/port/validation/sky_validation.md').write_text('# Sky prop validation\n\nGenerated by check_sky_props.py.\n\n'+
        '\n'.join(f'- {k}: {v}' for k,v in report.items())+'\n\nActual source placements, mesh vertices and independently read primitive index words match. Original type4/5 loop executes with only translation/transform/emit endpoints hooked; ordered calls, offsets, fog zero/restoration and fixed bucket match. Original quarter-turn instructions execute until the first GTE transform. Complete projection, GPU packets, visibility, ordering and runtime scene parity are not covered.\n')
    print(report)
if __name__=='__main__':main()
