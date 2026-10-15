"""Complete EXE area table reader: NULL load-chain holes are not table terminators."""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = (ROOT/'extract/disc/SCUS_944.23').read_bytes()
PTRS = 0x800A81B0


def area_records():
    records = []
    for area in range(1,256):
        ptr,identity = struct.unpack_from('<II',EXE,PTRS+area*8-0x80010000+0x800)
        if identity == 0xFFFFFFFF:
            return records
        if ptr and not 0x800A71B0 <= ptr < PTRS:
            raise ValueError(f'area {area:02X}: invalid load-chain pointer {ptr:08X}')
        chain = []
        if ptr:
            for i in range(16):
                address = ptr+i*8
                if address >= PTRS:
                    raise ValueError(f'area {area:02X}: load chain out of bounds')
                member,post,dest = struct.unpack_from('<HhI',EXE,address-0x80010000+0x800)
                if post < 0:
                    break
                chain.append((member,post,dest))
            else:
                raise ValueError(f'area {area:02X}: no terminator')
        records.append({'area':area,'pointer':ptr,'level':identity & 0xFFFF,'kind':identity >> 16,'chain':chain})
    raise ValueError('area table has no identity terminator')


def area_members():
    return {r['area']:m for r in area_records() for m,post,d in r['chain'] if post == 0 and d == 0x80154000 and m < 0x180}
