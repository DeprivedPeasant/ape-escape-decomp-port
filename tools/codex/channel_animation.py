"""ModelPoseBones channel stream decoder, independent of the fixed player frame format.

Header: u32 channel count; sorted four-byte {matrix,unused,kind,unused} descriptors;
then channelCount packed words per frame. Kinds: rotation=1, translation=2, scale=3.
"""
import struct


def signed(value,bits):
    value &= (1<<bits)-1
    return value-(1<<bits) if value & (1<<(bits-1)) else value


def vector(word,multiplier=1):
    return tuple(signed(word>>shift,bits)*multiplier for shift,bits in ((0,10),(10,12),(22,10)))


def layout(b,offset,nb,last,limit=None):
    limit = len(b) if limit is None else min(limit,len(b))
    if offset < 0 or offset+4 > limit:
        raise ValueError('channel header outside section')
    count = struct.unpack_from('<I',b,offset)[0]
    if count > nb*3 or offset+4+count*4 > limit:
        raise ValueError(f'channel count/header invalid: {count} for {nb} bones')
    descriptors = []
    for index in range(count):
        bone,unused,kind,pad = struct.unpack_from('<4B',b,offset+4+index*4)
        if not 1 <= bone <= nb or kind not in (1,2,3):
            raise ValueError(f'invalid channel descriptor {index}: {(bone,unused,kind,pad)}')
        descriptors.append((bone-1,kind))
    if descriptors != sorted(set(descriptors)):
        raise ValueError('channel descriptors duplicate or out of runtime traversal order')
    start = offset+4+count*4
    if last < 0 or start+(last+1)*count*4 > limit:
        raise ValueError(f'channel frames cross section: last={last} stride={count*4}')
    return descriptors,start


def decode(b,m,offset,first,last,limit=None):
    if not 0 <= first <= last:
        raise ValueError('invalid channel frame range')
    nb = len(m['bones'])
    descriptors,start = layout(b,offset,nb,last,limit)
    stride = len(descriptors)*4
    frames = []
    for frame in range(first,last+1):
        translations = [tuple(bone[2]) for bone in m['bones']]
        rotations = [(0,0,0)]*nb
        scales = [(1,1,1)]*nb
        for index,(bone,kind) in enumerate(descriptors):
            word = struct.unpack_from('<I',b,start+frame*stride+index*4)[0]
            if kind == 1:
                rotations[bone] = vector(word,4)
            elif kind == 2:
                translations[bone] = vector(word)
            else:
                scales[bone] = tuple(v/256 for v in vector(word))
        frames.append((translations,rotations,scales))
    return frames
