"""Small test-only MIPS-I interpreter for original terrain height instructions.

This is not the port runtime. It executes the original extracted GetGroundHeight with
GetSolidHeight replaced by a no-solid-surface hook, to isolate terrain integer behavior.
An optional gte_backend callback permits external COP2 execution for SDK tests;
without it the original limited SQR/GPF/leading-zero backend stays unchanged.
Supports only the integer instructions encountered in the tested routines, including branch
and load delay slots. Original bytes remain exclusively in extract/.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dump_areas as da


def signed(v, bits=32):
    v &= (1 << bits)-1
    return v-(1 << bits) if v & (1 << (bits-1)) else v


class Reference:
    def __init__(self, blob):
        self.ram = bytearray(0x200000)
        self.scratch = bytearray(0x400)
        self.ram[0x10000:0x10000+len(da.EXE)-0x800] = da.EXE[0x800:]
        if len(blob) > 0xAC000:
            raise ValueError('area member exceeds emulated load region')
        self.ram[0x154000:0x154000+len(blob)] = blob
        ah = struct.unpack_from('<I', blob, 0x4D0)[0]
        flags, offset = struct.unpack_from('<II', blob, ah)
        self.write(0x800F44E4, 4, 0x80154000+ah)
        self.write(0x800EBFA0, 2, 0)
        self.write(0x800EBFA2, 2, 0)
        self.disabled = bool(flags & 2)
        if not self.disabled:
            grid = ah+offset*4
            width, depth, colors = struct.unpack_from('<HHI', blob, grid)
            self.width, self.depth = width, depth
            cells = grid+8
            heights = cells+width*depth*12+colors*4
            self.write(0x800EBFA0, 2, width)
            self.write(0x800EBFA2, 2, depth)
            self.write(0x800EBFA8, 4, 0x80154000+cells)
            self.write(0x800EBFB4, 4, 0x80154000+heights)
        else:
            self.width = self.depth = 0
        self.opcodes = set()

    def memory(self, address, size):
        if 0x1F800000 <= address < 0x1F800400:
            blob, offset = self.scratch, address-0x1F800000
        elif 0x80000000 <= address < 0x80200000:
            blob, offset = self.ram, address-0x80000000
        else:
            raise ValueError(f'unmapped MIPS address {address:08X}')
        if offset+size > len(blob) or offset % size:
            raise ValueError('MIPS memory alignment/bounds')
        return blob, offset

    def read(self, address, size):
        blob, offset = self.memory(address, size)
        return int.from_bytes(blob[offset:offset+size], 'little')

    def write(self, address, size, value):
        blob, offset = self.memory(address, size)
        blob[offset:offset+size] = (value & ((1 << (size*8))-1)).to_bytes(size, 'little')

    def query(self, x, y, z):
        r = self.call(0x80043400, (x,y,z), hooks={0x8004D8C0: lambda cpu, regs: 32767})
        return dict(height=signed(r[2]), slope_x=signed(self.read(0x1F8003FC, 2),16),
                    slope_z=signed(self.read(0x1F8003FE,2),16), surface=self.read(0x1F8003F8,2))

    def call(self, root, args=(), hooks=None, limit=20000):
        r = [0]*32
        r[4:4+len(args)] = [v & 0xFFFFFFFF for v in args]
        r[29], r[31] = 0x80140000, 0x800000FC
        # Fresh stack for deterministic comparison of uninitialized disabled-grid slopes.
        self.ram[0x13FF80:0x140000] = b'\0'*128
        pc, branch, pending, lo, hi = root, None, None, 0, 0
        cop = [0]*32
        hooks = hooks or {}
        for _ in range(limit):
            if getattr(self,'trace',None): self.trace(pc,r)
            if pc == 0x800000FC:
                return r
            if pc in hooks:
                r[2] = hooks[pc](self,r) & 0xFFFFFFFF
                pc = r[31]
                continue
            ins = self.read(pc, 4)
            op, rs, rt, rd, sh, fn = ins >> 26, ins >> 21 & 31, ins >> 16 & 31, ins >> 11 & 31, ins >> 6 & 31, ins & 63
            imm = ins & 65535
            a, b, simm = r[rs], r[rt], signed(imm, 16)
            previous_branch, previous_load = branch, pending
            branch = pending = None
            written = None

            def put(index, value):
                nonlocal written
                if index:
                    r[index] = value & 0xFFFFFFFF
                    written = index

            if op == 0:
                self.opcodes.add(('special', fn))
                if fn == 0: put(rd, b << sh)
                elif fn == 2: put(rd, b >> sh)
                elif fn == 3: put(rd, signed(b) >> sh)
                elif fn == 4: put(rd, b << (a&31))
                elif fn == 6: put(rd, b >> (a&31))
                elif fn == 7: put(rd, signed(b) >> (a&31))
                elif fn == 8: branch = a
                elif fn == 9: branch = a; put(rd,pc+8)
                elif fn == 16: put(rd,hi)
                elif fn == 18: put(rd, lo)
                elif fn in (24,25):
                    product = signed(a)*signed(b) if fn==24 else a*b
                    lo,hi = product&0xFFFFFFFF,(product>>32)&0xFFFFFFFF
                elif fn in (26,27):
                    aa,bb = (signed(a),signed(b)) if fn==26 else (a,b)
                    if not bb: raise ValueError('MIPS divide by zero')
                    quotient = abs(aa)//abs(bb) * (-1 if (aa<0)!=(bb<0) else 1)
                    lo,hi = quotient&0xFFFFFFFF,(aa-quotient*bb)&0xFFFFFFFF
                elif fn in (32,33): put(rd, a+b)
                elif fn in (34,35): put(rd, a-b)
                elif fn == 36: put(rd, a&b)
                elif fn == 37: put(rd, a | b)
                elif fn == 38: put(rd,a^b)
                elif fn == 42: put(rd, int(signed(a) < signed(b)))
                elif fn == 43: put(rd,int(a<b))
                elif fn == 13: raise ValueError('MIPS break trap')
                else: raise ValueError(f'unsupported SPECIAL {fn:X} at {pc:X}')
            else:
                self.opcodes.add(('op', op))
                if op == 1:
                    if rt not in (0, 1): raise ValueError('unsupported REGIMM')
                    take = signed(a) < 0 if rt == 0 else signed(a) >= 0
                    if take: branch = (pc+4+simm*4) & 0xFFFFFFFF
                elif op in (2, 3):
                    branch = ((pc+4) & 0xF0000000) | ((ins & 0x3FFFFFF) << 2)
                    if op == 3: put(31, pc+8)
                elif op in (4, 5):
                    if (a == b) == (op == 4): branch = (pc+4+simm*4) & 0xFFFFFFFF
                elif op in (6,7):
                    if (signed(a)<=0 if op==6 else signed(a)>0): branch=(pc+4+simm*4)&0xFFFFFFFF
                elif op in (8,9): put(rt, a+simm)
                elif op == 10: put(rt,int(signed(a)<simm))
                elif op == 11: put(rt, int(a < (simm & 0xFFFFFFFF)))
                elif op == 12: put(rt, a & imm)
                elif op == 13: put(rt, a | imm)
                elif op == 15: put(rt, imm << 16)
                elif op == 18:
                    backend = getattr(self, 'gte_backend', None)
                    if backend is not None:
                        result = backend(ins, b)
                        if rs in (0, 2): pending = (rt, result)
                    elif rs==4:
                        cop[rd]=b
                        if rd==30:
                            value=b if b<0x80000000 else (~b&0xFFFFFFFF)
                            cop[31]=32-value.bit_length()
                    elif rs==0: pending=(rt,cop[rd])
                    elif ins&0x3F==0x28:
                        for index in (1,2,3): cop[24+index]=(signed(cop[8+index],16)**2)&0xFFFFFFFF
                    elif ins&0x3F==0x3D:
                        for index in (1,2,3): cop[24+index]=(signed(cop[8],16)*signed(cop[8+index],16))&0xFFFFFFFF
                    else: raise ValueError(f'unsupported GTE {ins:08X} at {pc:X}')
                elif op == 50 and getattr(self, 'gte_backend', None) is not None:
                    value = self.read((a+simm) & 0xFFFFFFFF, 4)
                    self.gte_backend(0x48800000 | rt << 11, value)
                elif op == 58 and getattr(self, 'gte_backend', None) is not None:
                    value = self.gte_backend(0x48000000 | rt << 11, 0)
                    self.write((a+simm) & 0xFFFFFFFF, 4, value)
                elif op in (32,33,35,36,37):
                    size = 4 if op == 35 else (1 if op in (32,36) else 2)
                    value = self.read((a+simm) & 0xFFFFFFFF, size)
                    if op in (32,33): value = signed(value,size*8)
                    pending = (rt, value & 0xFFFFFFFF)
                elif op in (34,38):
                    address=(a+simm)&0xFFFFFFFF; shift=(address&3)*8
                    word=self.read(address&~3,4)
                    old=previous_load[1] if previous_load and previous_load[0]==rt else b
                    value=((old&((1<<(24-shift))-1))|(word<<(24-shift))) if op==34 else ((old&(~((1<<(32-shift))-1)&0xFFFFFFFF))|(word>>shift))
                    pending=(rt,value&0xFFFFFFFF)
                elif op in (40,41,43): self.write((a+simm)&0xFFFFFFFF, {40:1,41:2,43:4}[op],b)
                elif op in (42,46):
                    address=(a+simm)&0xFFFFFFFF; offset=address&3;aligned=address&~3
                    if op==42:
                        for i in range(offset+1):self.write(aligned+i,1,b>>((3-offset+i)*8))
                    else:
                        for i in range(offset,4):self.write(aligned+i,1,b>>((i-offset)*8))
                else: raise ValueError(f'unsupported opcode {op:X} at {pc:X}')
            if previous_load is not None and previous_load[0] != written:
                r[previous_load[0]] = previous_load[1]
            r[0] = 0
            pc = previous_branch if previous_branch is not None else (pc+4) & 0xFFFFFFFF
        raise ValueError('original terrain function exceeded instruction budget')
