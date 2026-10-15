"""Compare native frame delta clock to original SeqReadDelta instructions."""
import ctypes as C,json,random,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from native_audio.check_player import build
from codex.mips_ground_reference import Reference,signed
import dump_areas as da,dump_sound as ds
ROOT=da.ROOT

def vlq(n):
    b=[n&127];n>>=7
    while n:b.insert(0,(n&127)|128);n>>=7
    return bytes(b)

def main():
    lib=build('seq_clock');lib.sp_delta_frames.argtypes=[C.c_uint32,C.c_uint16,C.c_uint16,C.c_uint16,C.POINTER(C.c_int32),C.POINTER(C.c_uint8)]
    lib.sp_delta_frames.restype=C.c_uint32
    ref=Reference((da.MEMBERS/'05B.bin').read_bytes());track=0x800F5AC0
    ref.write(0x800E1854,4,0x80120000);ref.write(0x800F75FE,2,60)
    rng=random.Random(94423);cases=0;real=0
    def compare(delta,tempo,ppq,rem,up):
        nonlocal cases
        encoded=vlq(delta);ref.ram[0x120000:0x120000+len(encoded)]=encoded
        for offset,width,value in [(0xC,4,0),(0x27,1,0),(0x104,2,tempo),(0x106,2,ppq),(0x108,1,up),(0x10C,4,rem),(0x114,4,0)]:ref.write(track+offset,width,value)
        ref.call(0x800A01BC,(0,),limit=2000)
        expected=(ref.read(track+0x114,4),signed(ref.read(track+0x10C,4)),ref.read(track+0x108,1))
        cr=C.c_int32(rem);cu=C.c_uint8(up)
        actual=(lib.sp_delta_frames(delta,tempo,ppq,60,C.byref(cr),C.byref(cu)),cr.value,cu.value)
        assert actual==expected,(delta,tempo,ppq,rem,up,actual,expected)
        cases+=1;return actual
    for _ in range(1500):
        tempo=rng.choice([60,88,120,170,240,280]);rate=tempo*480*10//3600
        compare(rng.choice([0,1,7,8,15,30,120,480,6720]),tempo,480,rng.randrange(-rate+1,rate),rng.randrange(2))
    # Traverse the entire real Time Station sequence, preserving carry across tempo changes.
    blob=(da.MEMBERS/'136.bin').read_bytes();off,n=ds.parse(blob)['ssq'];seq=blob[off:off+n]
    ppq,tempo=struct.unpack_from('<HH',seq,2);pos=0x110;rem=up=0
    lengths={8:2,9:2,10:2,11:2,12:1,14:1}
    while pos<len(seq):
        st=seq[pos]
        if st==255 and seq[pos+1]==0x2F:break
        if st==255:assert seq[pos+1]==0x51;tempo=struct.unpack_from('<H',seq,pos+2)[0];size=4
        else:size=1+lengths[st>>4]
        pos+=size;delta=0
        while True:
            b=seq[pos];pos+=1;delta=delta*128+(b&127)
            if b<128:break
        _,rem,up=compare(delta,tempo,ppq,rem,up);real+=1
    report=dict(comparisons=cases,time_station_events=real,failures=0,scope='Original SeqReadDelta frame count, remainder and round-up flag; scheduler starts frame0 and runs at735samples/tick. Other sequencer state/controller behavior not validated here.')
    # Audio stream callback chunk boundaries must not change quantized playback.
    import numpy as np
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();rendered=[]
    for block in (735,1024):
        player=lib.sp_create(blob,len(blob),exe,len(exe),1,8,4,10);assert player
        buffer=(C.c_float*(block*2))();pieces=[];left=stream_frames=3*44100
        try:
            while left:
                take=min(block,left);lib.sp_render(player,buffer,take)
                pieces.append(np.frombuffer(buffer,dtype=np.float32,count=take*2).copy());left-=take
        finally:lib.sp_destroy(player)
        rendered.append(np.concatenate(pieces))
    assert np.array_equal(*rendered),'Quantized stream depends on callback buffer size'
    report['stream_frames']=stream_frames;report['stream_chunk_sizes']=[735,1024]
    (ROOT/'extract/codex/audio-audit/seq-clock-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

if __name__=='__main__':main()
