"""Decode the SShd tone tables of a sound bank (notes/23-sequencer.md, notes/24-tone-tables.md).

SShd chunk (offsets relative to the start of the 12 byte chunk header; base pointer of the driver = chunk start):
  +0x10 u32 music tone table offset (u16 table), -1 when the chunk is the SFX bank
  +0x14 u32 volume curve (0x80 bytes after a 2 byte prefix; identity in the shipped banks)
  +0x18 u32 LFO / modulation wave tables (u16 count, then u16 offsets of 0x40 byte curves)
  +0x20 u32 SFX table (channel records + sequences), -1 when the chunk is the music bank
Music tone table: u16 count-1 (highest program number), then u16 offsets (0xFFFF = unused) of the program records,
relative to the table start. Program record: 8 byte header
  [0] last layer index in the low 7 bits (layers = ([0] & 0x7F) + 1); bit 7 set = layers play together, clear = key split (first matching layer only); 0xFF = special, [1] program volume, [2] program pan (0x40 centre),
  [4] pitch bend range in semitones, [5] modulation speed, [6] key offset for 0xFF programs
followed by 0x10 byte layer (tone) records:
  [0] low key  [1] high key  [2] root key  [3] fine tune (signed)  [4:6] SPU start address (units of 8 bytes, relative to
  the bank's sample area)  [6:8] ADSR1  [8:10] ADSR2  [10] SPU volume mode bits  [11] layer volume  [12] layer pan
  [13] bend range  [14] modulation speed  [15] flags: 0x01 / 0x10 bend range from the program / 0x20 modulation on /
  0x40 modulation speed from the program / 0x80 reverb on

Usage: uv run python tools/dump_tones.py 01B     -> print programs and layers of one bank
       uv run python tools/dump_tones.py check   -> validate every bank against its SSsq song and sample data
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_seq as sq  # noqa: E402
import dump_sound as ds  # noqa: E402


EMPTY = bytes([255]) * 16  # unused slot of a key indexed kit


def parse_programs(h: bytes):
    """Return {program: {'hdr': 8 bytes, 'layers': [16 byte records]}} of the music tone table (empty for the SFX chunk)."""
    base = struct.unpack_from("<I", h, 0x10)[0]
    if base == 0xFFFFFFFF:
        return {}
    last = struct.unpack_from("<H", h, base)[0]
    progs = {}
    for p in range(last + 1):
        off = struct.unpack_from("<H", h, base + 2 + p * 2)[0]
        if off == 0xFFFF:
            continue
        r = base + off
        hdr = h[r:r + 8]
        n = hdr[7] - hdr[6] + 1 if hdr[0] == 0xFF else (hdr[0] & 0x7F) + 1
        progs[p] = {"hdr": hdr, "layers": [h[r + 8 + i * 16:r + 24 + i * 16] for i in range(n)]}
    return progs


def main() -> None:
    if len(sys.argv) > 1 and sys.argv[1] != "check":
        m = sys.argv[1].upper()
        b = (ds.MEMBERS / f"{m}.bin").read_bytes()
        c = ds.parse(b)
        for so, ss in c["shd"]:
            progs = parse_programs(b[so:so + ss])
            for p, d in progs.items():
                print(f"program {p}: hdr {d['hdr'].hex(' ')}")
                for L in d["layers"]:
                    print("   keys %3d-%3d root %3d fine %4d addr %05X adsr %04X %04X vol %3d pan %3d bend %2d mod %2d flags %02X"
                          % (L[0], L[1], L[2], struct.unpack_from("b", L, 3)[0], struct.unpack_from("<H", L, 4)[0] * 8,
                             struct.unpack_from("<H", L, 6)[0], struct.unpack_from("<H", L, 8)[0], L[11], L[12], L[13], L[14], L[15]))
        return


def check() -> None:
    """Cross-check tone tables with the SSsq song (every note must hit a layer) and with the ADPCM sample starts."""
    tot = {"banks": 0, "layers": 0, "bad_addr": 0, "notes": 0, "no_prog": 0, "no_layer": 0}
    for f in sorted(ds.MEMBERS.glob("*.bin")):
        b = f.read_bytes()
        c = ds.parse(b)
        if not c or not c["ssq"] or not c["shd"]:
            continue
        so, ss = c["shd"][0]
        progs = parse_programs(b[so:so + ss])
        if not progs:
            continue
        tot["banks"] += 1
        vo, vs = c["vb"][0]
        starts = {a for a, _e in ds.split_samples(b[vo:vo + vs])} | {0}
        for d in progs.values():
            for L in d["layers"]:
                if L == EMPTY:
                    tot["empty_kit_slots"] = tot.get("empty_kit_slots", 0) + 1
                    continue
                tot["layers"] += 1
                if struct.unpack_from("<H", L, 4)[0] * 8 not in starts:
                    tot["bad_addr"] += 1
        qo, qs = c["ssq"]
        q = b[qo:qo + qs]
        cur = {i: q[0x10 + i * 16 + 2] for i in range(16)}
        ev, _ = sq.decode(q)
        for _pos, st, data, _d in ev:
            if st >> 4 == 0xC:
                cur[st & 15] = data[0]
            elif st >> 4 == 9:
                tot["notes"] += 1
                pr = progs.get(cur[st & 15])
                if pr is None:
                    tot["no_prog"] += 1
                elif pr["hdr"][0] == 0xFF:
                    i = data[0] - pr["hdr"][6]
                    if not 0 <= i < len(pr["layers"]) or pr["layers"][i] == EMPTY:
                        tot["no_layer"] += 1
                elif not any(L[0] <= data[0] <= L[1] for L in pr["layers"]):
                    tot["no_layer"] += 1
    print(tot)




# ---- SFX side (SShd chunks with a +0x20 table): sequences addressed by (program, note) ----
SFX_CTL_LEN = {0x01: 5, 0x02: 5, 0x06: 3, 0x07: 6, 0x0A: 6, 0x40: 3, 0x41: 6, 0x60: 5, 0x62: 3, 0x63: 3}


def u16(h: bytes, o: int) -> int:
    return struct.unpack_from("<H", h, o)[0]


def sfx_sequences(h: bytes):
    """{(program, note): absolute offset of the sequence} from the table at +0x1C:
    u16 last program, u16 offsets (0xFFFF = none) of per program sub tables; sub table = u16 last note + u16 sequence offsets,
    all offsets relative to the start of the +0x1C table."""
    base = struct.unpack_from("<I", h, 0x1C)[0]
    if base == 0xFFFFFFFF:
        return {}
    out = {}
    for p in range(u16(h, base) + 1):
        o = u16(h, base + 2 + p * 2)
        if o == 0xFFFF:
            continue
        sub = base + (o & 0xFFFE)
        for n in range(u16(h, sub) + 1):
            out[(p, n)] = base + u16(h, sub + 2 + n * 2)
    return out


def sfx_events(h: bytes, off: int):
    """Decode one SFX sequence: [(status, data bytes, delta ticks)] until FF 2F; returns (events, error or None)."""
    ev, p = [], off
    while True:
        st = h[p]
        if st == 0xFF and h[p + 1] == 0x2F:
            ev.append((0xFF, b"\x2f", 0))
            return ev, None
        if st == 0xA0:
            n = 3
        elif st == 0xB0 and h[p + 1] in SFX_CTL_LEN:
            n = SFX_CTL_LEN[h[p + 1]] - 1
        else:
            return ev, f"unknown event {h[p]:02X} {h[p+1]:02X} at 0x{p:X}"
        data = h[p + 1:p + 1 + n]
        p += 1 + n
        d = 0
        for _ in range(4):
            b = h[p]
            p += 1
            d = d << 7 | b & 0x7F
            if not b & 0x80:
                break
        ev.append((st, bytes(data), d))


def sfx_chunks():
    """Yield (member, chunk bytes) for every SShd chunk that has the SFX table (+0x20 != -1)."""
    for f in sorted(ds.MEMBERS.glob("*.bin")):
        b = f.read_bytes()
        c = ds.parse(b)
        if c:
            for so, ss in c["shd"]:
                if struct.unpack_from("<I", b, so + 0x20)[0] != 0xFFFFFFFF:
                    yield f.stem, b[so:so + ss]
        elif b[0x0C:0x10] == b"SShd" and struct.unpack_from("<I", b, 0x20)[0] != 0xFFFFFFFF:
            yield f.stem, b


def check_sfx() -> None:
    tot = bad = 0
    layers_bad = 0
    for m, h in sfx_chunks():
        # SFX program records: u16 table at +0x20 + 0x190 (count-1, offsets relative to the table), layers as in the music table
        sfxbase = struct.unpack_from("<I", h, 0x20)[0]
        for k, off in sfx_sequences(h).items():
            ev, err = sfx_events(h, off)
            tot += 1
            if err:
                bad += 1
                print(m, k, err)
                continue
            for st, data, _d in ev:
                if st == 0xA0:
                    tbl = sfxbase + 0x190
                    progs = u16(h, tbl)
                    if data[2] > progs or u16(h, tbl + 2 + data[2] * 2) == 0xFFFF:
                        layers_bad += 1
    print({"sequences": tot, "bad": bad, "bad_program_refs": layers_bad})


def sfx_programs(h: bytes):
    """SFX tone programs: table at (+0x20 offset) + 0x190 with the same shape as the music table; programs with header
    byte 0xFF hold one layer per key from [6] (first key) to [7] (last key) selected by the note of the A0 event."""
    base = struct.unpack_from("<I", h, 0x20)[0] + 0x190
    progs = {}
    for p in range(u16(h, base) + 1):
        off = u16(h, base + 2 + p * 2)
        if off == 0xFFFF:
            continue
        r = base + off
        hdr = h[r:r + 8]
        n = hdr[7] - hdr[6] + 1 if hdr[0] == 0xFF else (hdr[0] & 0x7F) + 1
        progs[p] = {"hdr": hdr, "layers": [h[r + 8 + i * 16:r + 24 + i * 16] for i in range(n)]}
    return progs


def check_sfx_layers() -> None:
    """Every A0 event of every SFX sequence must select an existing layer whose sample start is a real ADPCM sample start."""
    tot = miss = badaddr = 0
    for f in sorted(ds.MEMBERS.glob("*.bin")):
        b = f.read_bytes()
        c = ds.parse(b)
        if not c:
            continue
        for k, (so, ss) in enumerate(c["shd"]):
            h = b[so:so + ss]
            if struct.unpack_from("<I", h, 0x20)[0] == 0xFFFFFFFF:
                continue
            progs = sfx_programs(h)
            vo, vs = c["vb"][min(k, len(c["vb"]) - 1)]
            starts = {a for a, _e in ds.split_samples(b[vo:vo + vs])} | {0}
            for sq_off in sfx_sequences(h).values():
                for st, data, _d in sfx_events(h, sq_off)[0]:
                    if st != 0xA0:
                        continue
                    tot += 1
                    pr = progs.get(data[2])
                    idx = data[0] - pr["hdr"][6] if pr and pr["hdr"][0] == 0xFF else None
                    if pr is None or idx is None or not 0 <= idx < len(pr["layers"]):
                        miss += 1
                        continue
                    if u16(pr["layers"][idx], 4) * 8 not in starts:
                        badaddr += 1
    print({"sfx_notes": tot, "no_layer": miss, "bad_addr": badaddr})


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "check":
        check()
        check_sfx()
        check_sfx_layers()
    else:
        main()
