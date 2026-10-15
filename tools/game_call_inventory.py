"""Audit the ledger's C interfaces before generating host call adapters.

This reads C declarations, never instructions. Special ABI adapters are explicit.
"""
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def declarations():
    result = {}
    for path in sorted((ROOT / "src").rglob("*.h")):
        if path.name == "native_gadget_slots.h":
            continue  # Its historical AeGadgetHooks tag conflicts with ae_gadget.h.
        text = re.sub(r"/\*.*?\*/|//[^\n]*", "", path.read_text(), flags=re.S)
        for match in re.finditer(r"\b(void|int32_t|uint32_t|int)\s+(ae_\w+)\s*\(([^;{}]*)\)\s*;", text):
            result[match[2]] = (path.relative_to(ROOT).as_posix(), match[1],
                                [a.strip() for a in match[3].split(",")])
    return result


def ledger():
    with (ROOT / "tools/verified.csv").open(newline="") as stream:
        return [row for row in csv.DictReader(stream) if row["status"] == "diffed"]


if __name__ == "__main__":
    prototypes = declarations()
    rows = ledger()
    print(f"{len(rows)} diffed rows; {len(set(r['address'] for r in rows))} addresses")
    for row in rows:
        proto = prototypes.get(row["c_function"])
        if proto is None or any("[" in arg for arg in proto[2]):
            print(row["address"], row["c_function"], proto)
