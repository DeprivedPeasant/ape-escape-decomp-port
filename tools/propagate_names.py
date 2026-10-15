"""Propagate names between duplicated functions (same code shape, e.g. enemy helpers copied into several overlays).

Needs notes/decomp/hashes.csv (Ghidra DumpFuncHashes.java) and tools/symbols.csv.
Prints symbols.csv lines for unnamed functions whose twin (same shape hash and size) is already named, with the
overlay prefix swapped (OvlXXX_ -> Ovl<this overlay>_). With --groups lists still-unnamed twin groups instead.

Usage: uv run python tools/propagate_names.py [--groups] [min_instructions]
"""
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def named_symbols() -> dict[str, tuple[str, str]]:
    out = {}
    for line in (ROOT / "tools/symbols.csv").read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split(",", 2)
        if len(parts) == 3:
            out[parts[0].lower()] = (parts[1], parts[2])
    return out


def main() -> None:
    groups_mode = "--groups" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    min_n = int(args[0]) if args else 6
    syms = named_symbols()
    groups = defaultdict(list)
    shape_groups = defaultdict(list)
    for r in csv.DictReader(open(ROOT / "notes/decomp/hashes.csv", encoding="utf-8")):
        if int(r["size"]) >= min_n:
            groups[r["hash"]].append(r)
        if int(r["size"]) >= 25 and r["block"].startswith("ovl_"):
            shape_groups[r["shape"]].append(r)  # looser: same opcode sequence, constants may differ
    seen = set()
    for rows in list(groups.values()) + list(shape_groups.values()):
        if len(rows) < 2:
            continue
        key = lambda r: r["entry"].lower()
        named = [(r, syms[key(r)]) for r in rows if key(r) in syms]
        unnamed = [r for r in rows if key(r) not in syms and r["name"].startswith("FUN_")]
        if groups_mode:
            if not named and len(unnamed) > 1:
                print(int(rows[0]["size"]), " ".join(r["entry"] for r in unnamed))
            continue
        if not named or not unnamed:
            continue
        src, (name, comment) = named[0]
        for r in unnamed:
            if r["entry"] in seen:
                continue
            seen.add(r["entry"])
            blk = r["block"]
            if not blk.startswith("ovl_"):
                continue  # main-EXE twins are named by hand
            nm = name
            if blk.startswith("ovl_"):
                tag = "Ovl" + blk[4:]
                nm = re.sub(r"^(Ovl[0-9A-F]{3}|Boss[0-9A-F]{3,4})_", tag + "_", name)
                if nm == name:
                    nm = f"{name}_{blk[4:]}"
            print(f"{r['entry']},{nm},Same code as {src['entry']} ({name}). {comment}")


if __name__ == "__main__":
    main()
