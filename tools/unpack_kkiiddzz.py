"""Split KKIIDDZZ.DAT + KKIIDDZZ.BNS into archive members using KKIIDDZZ.HED.

HED is 512 little-endian u32 descriptors: high 12 bits = sector count,
low 20 bits = sector offset relative to HED's LBA + 1 (i.e. the start of DAT).
DAT and BNS are physically adjacent, so they form one logical stream.
Descriptors 0x1C0+ instead hold absolute disc LBAs of STR movie files; those
are resolved to a disc file via the dumpsxiso XML instead of being extracted
again. Zero descriptors are unused slots.

Usage: uv run python tools/unpack_kkiiddzz.py
"""
import json
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

SECTOR = 2048
ROOT = Path(__file__).resolve().parent.parent
DISC = ROOT / "extract" / "disc"
OUT = ROOT / "extract" / "members"
XML = ROOT / "extract" / "SCUS-94423.xml"


def disc_files() -> list[tuple[int, int, str]]:
    """(lba, sectors, path relative to extract/disc) for every file on disc."""
    files = []
    for el in ET.parse(XML).iter("file"):
        src = Path(el.get("source"))
        rel = src.relative_to(DISC).as_posix()
        lba = int(el.get("offs"))
        # XA/STR ("xa"/"mixed") files are stored as raw 2336-byte sectors by dumpsxiso
        per = 2336 if el.get("type") in ("xa", "mixed") else SECTOR
        files.append((lba, -(-src.stat().st_size // per), rel))
    return files


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    hed = (DISC / "KKIIDDZZ.HED").read_bytes()
    dat = (DISC / "KKIIDDZZ.DAT").read_bytes()
    stream = dat + (DISC / "KKIIDDZZ.BNS").read_bytes()
    dat_sectors, stream_sectors = len(dat) // SECTOR, len(stream) // SECTOR
    base_lba = next(lba for lba, _, rel in disc_files() if rel == "KKIIDDZZ.DAT")
    files = disc_files()

    index = []
    for i, (desc,) in enumerate(struct.iter_unpack("<I", hed)):
        if desc == 0:
            continue
        count, offset = desc >> 20, desc & 0xFFFFF
        entry = {"index": f"0x{i:03X}", "sector_offset": offset, "sectors": count,
                 "lba": base_lba + offset}
        if offset + count <= stream_sectors:
            data = stream[offset * SECTOR:(offset + count) * SECTOR]
            name = f"{i:03X}.bin"
            (OUT / name).write_bytes(data)
            entry |= {"source": "BNS" if offset >= dat_sectors else "DAT",
                      "file": name, "ps_x_exe": data[:8] == b"PS-X EXE"}
        else:
            # Out-of-archive descriptors hold absolute disc LBAs (STR movies)
            lba = offset
            entry["lba"] = lba
            hit = next(((rel, lba - f_lba) for f_lba, n, rel in files
                        if f_lba <= lba < f_lba + n), None)
            if hit is None:
                sys.exit(f"member 0x{i:03X} (LBA {lba}) matches no disc file")
            entry |= {"source": "disc", "disc_file": hit[0], "sector_in_file": hit[1]}
        index.append(entry)

    (OUT / "index.json").write_text(json.dumps(index, indent=1))
    by_src = {s: sum(e["source"] == s for e in index) for s in ("DAT", "BNS", "disc")}
    print(f"{len(index)} members {by_src} -> {OUT}")


if __name__ == "__main__":
    main()
