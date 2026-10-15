"""Differential original24voice music/effect allocation, including all state writes."""
import ctypes as C,json,random,shutil,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dump_areas as da
from codex.mips_ground_reference import Reference
ROOT=da.ROOT;OUT=ROOT/'extract/codex/audio-audit'
class Voice(C.Structure):_fields_=[(n,C.c_uint16) for n in ['active','freeable','age','effect','priority','effect_id']]
class Pool(C.Structure):_fields_=[('voice',Voice*24)]+[(n,C.c_uint8) for n in ['cursor','count','effects','effect_limit']]

def main():
    probe=OUT/'voice_pool_probe.c'
    probe.write_text('#include "../../../src/audio/sp_voice_pool.h"\nint music(SpVoicePool *p){return sp_pool_music(p);}\nint effect(SpVoicePool *p,unsigned id,unsigned priority){return sp_pool_effect(p,id,priority);}\n')
    dll=OUT/'voice_pool_probe.dll'
    subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',str(probe),'-o',str(dll),'-static-libgcc'],check=True)
    lib=C.CDLL(str(dll));lib.music.argtypes=[C.POINTER(Pool)];lib.effect.argtypes=[C.POINTER(Pool),C.c_uint,C.c_uint]
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());rng=random.Random(94423);cases=[0,0]
    for mode in (0,1):
        for _ in range(3000):
            p=Pool();ages=list(range(24));rng.shuffle(ages)
            for i in range(24):
                p.voice[i]=Voice(rng.choice([1,1,1,0]),rng.randrange(2),rng.choice([ages[i],64,65535]),rng.randrange(2),rng.randrange(256),rng.randrange(8))
                v=p.voice[i]
                for off,val in [(0,v.active),(8,v.freeable),(10,v.age),(26,v.effect),(30,v.priority),(32,v.effect_id)]:ref.write(0x800E07F0+i*0x66+off,2,val)
            p.cursor=rng.choice([0,1,23,255]);p.count=rng.randrange(25);p.effects=rng.randrange(10);p.effect_limit=8
            for address,name in [(0x800F75CF,'cursor'),(0x800F75C0,'count'),(0x800F75CD,'effects'),(0x800F75C3,'effect_limit')]:ref.write(address,1,getattr(p,name))
            ident,priority=rng.randrange(8),rng.randrange(256)
            expected=ref.call(0x8009DAB4 if mode==0 else 0x8009DCA0,() if mode==0 else (ident,priority),limit=30000)[2]
            actual=lib.music(C.byref(p)) if mode==0 else lib.effect(C.byref(p),ident,priority)
            assert actual==expected,(mode,ident,priority,actual,expected)
            for address,name in [(0x800F75CF,'cursor'),(0x800F75C0,'count'),(0x800F75CD,'effects')]:assert getattr(p,name)==ref.read(address,1),(mode,name,getattr(p,name),ref.read(address,1))
            for i in range(24):assert p.voice[i].age==ref.read(0x800E07F0+i*0x66+10,2),(mode,i,p.voice[i].age,ref.read(0x800E07F0+i*0x66+10,2))
            cases[mode]+=1
    report=dict(music=cases[0],effects=cases[1],failures=0,scope='Seeded selection, age and global-count writes of original allocators; oscillator lifetime/integration not covered.')
    (OUT/'voice-pool-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
