"""Rank still-unnamed (FUN_) functions by number of distinct callers: the helper sweep.

Needs notes/inventory/{functions,calls}.csv (Ghidra DumpCallGraph.java). Prints the top N
(default 30) with size and a few callers; use it to pick the next functions to name.

Usage: uv run python tools/unnamed_by_callers.py [N] [min_size]
"""
import csv
import sys
from collections import defaultdict
from pathlib import Path

INV = Path(__file__).resolve().parent.parent / "notes/inventory"


def main() -> None:
    top = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    min_size = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    funcs = {}
    for r in csv.DictReader(open(INV / "functions.csv", encoding="utf-8")):
        funcs[r["entry"]] = r
    callers = defaultdict(set)
    for r in csv.DictReader(open(INV / "calls.csv", encoding="utf-8")):
        if r["caller"] != r["callee"]:
            callers[r["callee"]].add(r["caller"])
    rows = []
    for addr, f in funcs.items():
        if f["name"].startswith("FUN_") and int(f["size"]) >= min_size:
            rows.append((len(callers[addr]), int(f["size"]), addr, f["name"], sorted(callers[addr])))
    rows.sort(reverse=True)
    for n, size, addr, name, cs in rows[:top]:
        names = ", ".join(funcs[c]["name"] if c in funcs else c for c in cs[:4])
        print(f"{n:4d} callers  {size:5d} B  {name:34s} {names}")


if __name__ == "__main__":
    main()
