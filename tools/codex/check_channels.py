"""Independent runtime-order channel decoding and TRS matrix reference."""
import json
import math
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import export_gltf as gl
import export_model as em
from codex.channel_animation import decode,layout


def reference(b,m,clip,frame):
    offset=clip[1]
    count=struct.unpack_from('<I',b,offset)[0]
    descriptors=b[offset+4:offset+4+count*4]
    cursor=0
    translations=[];rotations=[];scales=[]
    for joint,bone in enumerate(m['bones'],1):
        values=[tuple(bone[2]),(0,0,0),(1,1,1)]
        for kind,target in ((1,1),(2,0),(3,2)):
            if cursor<count and descriptors[cursor*4]==joint and descriptors[cursor*4+2]==kind:
                word=struct.unpack_from('<I',b,offset+4+count*4+(frame*count+cursor)*4)[0]
                # Sign extension by subtracting the modulus, not the decoder helper.
                components=[word&1023,(word//1024)&4095,word//4194304]
                components=[v-mod if v>=mod//2 else v for v,mod in zip(components,(1024,4096,1024))]
                factor=4 if kind==1 else 1/256 if kind==3 else 1
                values[target]=tuple(v*factor for v in components)
                cursor+=1
        translations.append(values[0]);rotations.append(values[1]);scales.append(values[2])
    assert cursor==count,'runtime leaves unconsumed descriptors'
    return translations,rotations,scales


def positions(m,pose):
    translations,rotations,scales=pose
    matrices=[(((1,0,0),(0,1,0),(0,0,1)),(0,0,0))]
    result=[None]*m['vc']
    for i,(count,first,offset,parent) in enumerate(m['bones']):
        # Direct expanded Rz*Ry*Rx, independently of quaternion construction.
        x,y,z=[v*math.pi/2048 for v in rotations[i]]
        cx,cy,cz=math.cos(x),math.cos(y),math.cos(z)
        sx,sy,sz=math.sin(x),math.sin(y),math.sin(z)
        r=((cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx),
           (sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx),(-sy,cy*sx,cy*cx))
        r=tuple(tuple(r[a][c]*scales[i][c] for c in range(3)) for a in range(3))
        pr,pt=matrices[parent]
        world=tuple(tuple(sum(pr[a][k]*r[k][c] for k in range(3)) for c in range(3)) for a in range(3))
        origin=tuple(pt[a]+sum(pr[a][k]*translations[i][k] for k in range(3)) for a in range(3))
        matrices.append((world,origin))
        for vertex in range(first,first+count):
            result[vertex]=tuple(origin[a]+sum(world[a][k]*m['verts'][vertex][k] for k in range(3)) for a in range(3))
    return result


def main():
    metadata=gl.clip_metadata();seen=set();frames=associations=channels=scale_channels=0
    reserved=set()
    for member,offset,m in em.scan_members():
        b=(em.MEMBERS/f'{member}.bin').read_bytes()
        for clip in metadata.get((member,offset),[]):
            if len(clip)!=6 or clip[5]!='channels':continue
            associations+=1
            key=(member,clip[1],len(m['bones']),clip[2],clip[3])
            if key in seen:continue
            seen.add(key)
            descriptors,start=layout(b,clip[1],len(m['bones']),clip[3])
            channels+=len(descriptors);scale_channels+=sum(kind==3 for _,kind in descriptors)
            reserved.update((b[clip[1]+5+i*4],b[clip[1]+7+i*4]) for i in range(len(descriptors)))
            for frame,actual in enumerate(decode(b,m,clip[1],clip[2],clip[3]),clip[2]):
                assert actual==reference(b,m,clip,frame),(member,offset,frame)
                frames+=1
    # Synthetic descriptors deliberately contain ignored nonzero bytes.
    model={'bones':[(1,0,(7,8,9),0),(1,1,(10,0,0),1)],'vc':2,'verts':[(0,0,0),(4,0,0)]}
    pack=lambda x,y,z:(x&1023)|((y&4095)<<10)|((z&1023)<<22)
    blob=struct.pack('<I',3)+bytes((1,5,1,9,1,6,2,8,2,7,3,4))+struct.pack('<3I',pack(0,0,256),pack(1,-2,3),pack(128,256,256))
    clip=('fixture',0,0,0,1,'channels')
    pose=decode(blob,model,0,0,0)[0]
    assert pose==reference(blob,model,clip,0)
    expected=positions(model,pose)
    assert max(abs(a-b) for a,b in zip(expected[1],(1,10,3)))<1e-10,expected
    rejected=0
    for damaged in (blob[:-1],blob[:6]+bytes((4,))+blob[7:],blob[:4]+blob[8:12]+blob[4:8]+blob[12:]):
        try:layout(damaged,0,2,0)
        except ValueError:rejected+=1
        else:raise AssertionError('invalid stream accepted')
    for first,last in ((-1,0),(1,0)):
        try:decode(blob,model,0,first,last)
        except ValueError:rejected+=1
        else:raise AssertionError('invalid channel frame range accepted')
    result={'channel_clip_associations':associations,'unique_clip_sources':len(seen),'decoded_frames':frames,
            'channel_descriptors':channels,'scale_descriptors':scale_channels,'ignored_byte_pairs':sorted(reserved),
            'synthetic_transform_checks':1,'malformed_streams_rejected':rejected,'failures':[]}
    (gl.OUT/'channel-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    note=['# Channel animation format (generated)','',
          'Generated by tools/codex/check_channels.py. Private evidence: ui-pose.c, ModelPoseBones (0x80015780).','',
          'Enemies, secondary idle actors and stage UI actors use a channel stream, distinct from the player/monkey fixed pose format. '
          'A u32 channel count is followed by count four-byte descriptors and then count packed words per frame. '
          'Descriptor byte 0 is the one-based bone matrix index; byte 2 is kind 1 rotation, 2 translation or 3 scale. '
          'Bytes 1/3 are ignored by this routine and may be nonzero; their broader meaning is unresolved. '
          'Descriptors occur in bone order and rotation/translation/scale order within each bone.','',
          'All words carry signed 10/12/10-bit components. Rotation multiplies by four PSX angle units. '
          'Translation is unscaled. Scale multiplies by 16 in PSX 12-bit fixed point, hence component/256 in glTF. '
          'Missing channels use identity rotation/scale and the stored bone offset, including bone 0. '
          'Scale multiplies rotation columns before parent accumulation. Frame addressing is header+4+count*4+frame*count*4.','',
          'Correction: earlier enemy/idle glTF exports used the fixed player stride. Structural and skin tests alone did not expose '
          'that source-decoder error. Those exports and checks are rebuilt using this format and an independent runtime-order decoder. '
          'For example UI member 025 has 17 channels (68 bytes/frame), rather than its 16 bones plus two raw words (72 bytes/frame).','',
          f'Validated {associations} associations / {len(seen)} distinct clip sources / {frames} source frames without decoder disagreement. '
          f'Observed {scale_channels} scale descriptors across distinct sources. Synthetic checks cover ignored descriptor bytes, '
          f'parented rotation/translation/nonuniform scale; {rejected} malformed streams are rejected.','',
          'Actual emitted skin positions are checked separately by check_gltf.py and check_preview.py against independent TRS matrices. '
          'Runtime actor placement and external animation blends are not part of these base clips.']
    (gl.ROOT/'notes/61-channel-animation.md').write_text('\n'.join(note)+'\n')
    print(result)

if __name__=='__main__':main()
