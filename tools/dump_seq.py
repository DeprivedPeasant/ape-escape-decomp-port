"""Decode the SSsq sequence chunk of a sound bank member (notes/23-sequencer.md).

Layout (validated on all banks): chunk header 12 bytes + 'SSsq'; +0x10..+0x110 = 16 channel records of 0x10 bytes
[0]=0, [1]=channel, [2]=tone program (0xFF = none), [3]=volume (0x64), [4]=pan (0x40), [0xC]=0x10, [0xE]=0x64; the event
stream starts at +0x110 (the restart position used by SeqCmd_MetaEndOfTrack).
Events: status byte (high nibble type, low nibble channel) + data, then a MIDI style variable length delta:
  0x8n/0x9n/0xAn/0xBn: 2 data bytes   0xCn/0xEn: 1 data byte   0xF0: meta (2F end, 51 tempo + 2 bytes)

Chunk header (validated): u16 master volume (100), u16 ticks per quarter note (480), u16 initial tempo in BPM, u16 0.
Tempo meta event: FF 51 lo hi = u16 BPM. Controller 0x63 value 20 / 30 = loop start / end, with controller 6 = 127 right
after it (infinite loop count), as in the standard PS1 SEQ format. Controllers 1, 2, 7 (volume), 10 (pan), 0x40 also occur.
Pitch bend carries a single 7-bit data byte (0x40 = centre).

Usage: uv run python tools/dump_seq.py            -> decode every bank, print a summary table
       uv run python tools/dump_seq.py 01B [n]    -> print the first n events of one bank
       uv run python tools/dump_seq.py midi       -> extract/midi/<member>.mid for every bank (standard MIDI file, format 0)
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_sound as ds  # noqa: E402

DATA = {0x8: 2, 0x9: 2, 0xA: 2, 0xB: 2, 0xC: 1, 0xE: 1}


def decode(q: bytes, start: int = 0x110):
    """Yield (pos, status, data bytes, delta) until the end-of-track meta or a parse failure; returns (events, ok)."""
    ev, p = [], start
    while p < len(q):
        st = q[p]
        if st < 0x80:
            return ev, f"bad status 0x{st:02X} at 0x{p:X}"
        n = DATA.get(st >> 4)
        if st >> 4 == 0xF:
            if q[p + 1] == 0x2F:
                ev.append((p, st, bytes(q[p + 1:p + 2]), 0))
                return ev, "ok"
            if q[p + 1] == 0x51:
                n = 3
            else:
                return ev, f"unknown meta 0x{q[p+1]:02X} at 0x{p:X}"
        data = q[p + 1:p + 1 + n]
        p += 1 + n
        d = 0
        for _ in range(4):  # variable length delta (7 bits per byte, bit 7 = more)
            b = q[p]
            p += 1
            d = d << 7 | b & 0x7F
            if not b & 0x80:
                break
        ev.append((p - 1, st, bytes(data), d))
    return ev, "ran off the end"


def vlq(n: int) -> bytes:
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(n & 0x7F | 0x80)
        n >>= 7
    return bytes(reversed(out))


def to_midi(q: bytes) -> bytes:
    """Standard MIDI file (format 0) of one SSsq chunk. Loop markers stay as controller 0x63 / 6 events."""
    _, res, tempo, _ = struct.unpack_from("<4H", q, 0)
    ev, _why = decode(q)
    meta_tempo = bytes([0xFF, 0x51, 0x03])
    trk = bytearray(bytes([0]) + meta_tempo + (60_000_000 // max(tempo, 1)).to_bytes(3, "big"))
    pend = 0  # the delta of an event is the gap AFTER it, MIDI wants the gap BEFORE the next one
    for _pos, st, data, d in ev:
        if st == 0xFF and data[0] == 0x2F:
            trk += vlq(pend) + bytes([0xFF, 0x2F, 0])
            break
        if st == 0xFF:
            bpm = struct.unpack_from("<H", data, 1)[0]
            trk += vlq(pend) + meta_tempo + (60_000_000 // max(bpm, 1)).to_bytes(3, "big")
        elif st >> 4 == 0xE:
            trk += vlq(pend) + bytes([st, 0, data[0]])  # 7 bit bend -> 14 bit (LSB 0, MSB = value)
        else:
            trk += vlq(pend) + bytes([st]) + data
        pend = d
    return b"MThd" + struct.pack(">IHHH", 6, 0, 1, res) + b"MTrk" + struct.pack(">I", len(trk)) + bytes(trk)


def main() -> None:
    if len(sys.argv) > 1 and sys.argv[1] == "midi":
        out = ds.ROOT / "extract/midi"
        out.mkdir(parents=True, exist_ok=True)
        n = 0
        for f in sorted(ds.MEMBERS.glob("*.bin")):
            b = f.read_bytes()
            c = ds.parse(b)
            if c and c["ssq"]:
                off, size = c["ssq"]
                (out / f"{f.stem}.mid").write_bytes(to_midi(b[off:off + size]))
                n += 1
        print(n, "files ->", out)
        return
    if len(sys.argv) > 1:
        m = sys.argv[1].upper()
        b = (ds.MEMBERS / f"{m}.bin").read_bytes()
        off, size = ds.parse(b)["ssq"]
        ev, why = decode(b[off:off + size])
        print(m, why, len(ev), "events")
        for pos, st, data, d in ev[:int(sys.argv[2]) if len(sys.argv) > 2 else 40]:
            print(f"{pos:06X} {st:02X} {data.hex(' '):8s} +{d}")
        return
    print("member  size   events  result")
    for f in sorted(ds.MEMBERS.glob("*.bin")):
        b = f.read_bytes()
        c = ds.parse(b)
        if not c or not c["ssq"]:
            continue
        off, size = c["ssq"]
        ev, why = decode(b[off:off + size])
        print(f"{f.stem}  {size:6d} {len(ev):6d}  {why}")


if __name__ == "__main__":
    main()
