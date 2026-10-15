"""Fast consistency check of the verified-function ledger (tools/verified.csv) and the checker exclusion registry
(tools/mips/check_exclusions.csv).  Runs no MIPS code (a few seconds at most), so it is part of the normal check set.

  a) static: every `ae_*` function defined in src/core (ae_sdk.c, ae_pad.c), src/bodies, src/monkeys and the ported player-input
     files (src/player/native_gadget_slots.c, native_player_pad.c) has a ledger row and vice versa; every row's checker exists and
     mentions the function; every row of the exclusion registry names an existing checker (and, for `unreachable`, a function of it).
  b) results: the slow differential checkers write extract/checks/results/<checker>.json at the end of a default-scale run.  For
     every `diffed` row whose checker has such a file, the run must have passed and the reported cases must be >= min_cases.  A
     missing or stale file (another git HEAD) is a warning, not a failure; the slow checkers are not re-run here.

Ledger columns: address,name,c_function,source,checker,status,min_cases,notes.  status: `diffed` (ported, passes its checker) or `wip`
(ported, checker not finished).  min_cases: the per-function case count of the checker at its default scale.  Lowering a min_cases
value or adding an exclusion row needs the project owner's review (see notes/port/validation/ledger.md).

Usage: uv run python tools/check_verified.py [--results-only]
"""
import csv
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEDGER = ROOT / "tools/verified.csv"
EXCLUSIONS = ROOT / "tools/mips/check_exclusions.csv"
RESULTS = ROOT / "extract/checks/results"
INVENTORY = ROOT / "notes/inventory/functions.csv"
# files whose `ae_*` functions must all be in the ledger (the ported RAM-image code with a differential checker)
# every RAM-image port file (src/**/ae_*.c, except the RAM image itself and src/game, the host glue that wires the ports
# to each other: its functions are not original functions, tools/mips/check_game.py tests the wiring) plus the two
# player-input ports; a new module is picked up automatically (and parallel branches no longer conflict on this list)
SCAN = sorted(str(p.relative_to(ROOT)).replace("\\", "/") for p in (ROOT / "src").rglob("ae_*.c")
              if p.name != "ae_ram.c" and "game" not in p.relative_to(ROOT / "src").parts[:1]) + ["src/player/native_gadget_slots.c", "src/player/native_player_pad.c"]
# defined `ae_*` functions that are not ported original functions (adding one needs the owner's review)
NOT_ORIGINAL = {
    "ae_monkey_hook_missing": "sets the error flag for a NULL hook (support of the hook macros)",
    "ae_FUN_8013fc00": "dispatch to the area-overlay function at 8013FC00 (not in the EXE: overlay code); calls a hook",
    "ae_ModelEmitPrimsCycleColors": "renderer boundary wrapper (8001D530): the whole body is GTE colour setup + GPU packets, done by the native renderer hook",
    "ae_DrawRibbonBetween": "renderer boundary wrapper (8002020C): segment interpolation, projection and GPU packets belong to the native ribbon renderer hook",
    "ae_FUN_8013f660": "dispatch to the area-overlay function at 8013F660 (zako kind 0x3F; overlay code, not in the EXE); calls a hook",
    "ae_FUN_8014022c": "dispatch to the area-overlay function at 8014022C (overlay code, not in the EXE); calls a hook",
    "ae_FUN_8014263c": "dispatch to the area-overlay function at 8014263C (overlay code, not in the EXE); calls a hook",
    "ae_MeshTransform": "renderer boundary wrapper (800160C0): the whole body is the GTE RTPT mesh projection loop, done by the native renderer hook",
    "ae_MeshDraw": "renderer boundary wrapper (80017E5C): the whole body is GTE lighting and GPU packet emission, done by the native renderer hook",
    "ae_DebugTextDrawGlyph": "renderer boundary wrapper (8008C434): glyph projection and GPU packets belong to the native renderer hook",
    "ae_ScreenFlashDraw": "renderer boundary wrapper (80046990): the whole body builds ordering-table packets, done by the native renderer hook",
    "ae_CameraTransitionFrameDraw": "renderer boundary wrapper (8004CEB4): quad and ordering-table construction belongs to the native renderer hook",
    "ae_player_pad_init": "host glue around ae_pad_* (loads the EXE image, owns the AeRam)",
    "ae_player_pad_free": "host glue around ae_pad_*",
    "ae_player_pad_set_camera": "host glue around ae_pad_CameraRotMatrix",
    "ae_player_pad_read": "host glue: copies the pad state out of the RAM image",
    "ae_player_pad_update": "host glue around ae_pad_PadUpdate",
    "ae_player_pad_frame_end": "host glue around ae_pad_PadSampleStickHistory",
}
DEF = re.compile(r"^(?!static)[A-Za-z_][A-Za-z0-9_ \*]*?[ \*](ae_[A-Za-z0-9_]+)\(", re.M)
STATUSES = ("diffed", "wip")
KINDS = ("ram_mask", "shared_random", "stack_window", "unreachable", "dropped_mutation")
errors, warnings = [], []


def read_csv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return [{k: (v or "").strip() for k, v in r.items()} for r in csv.DictReader(f)]


def git_head():
    try:
        return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    except Exception:  # noqa: BLE001
        return ""


def defined_functions():
    out = {}
    for rel in SCAN:
        for m in DEF.finditer((ROOT / rel).read_text(encoding="utf-8")):
            out.setdefault(m.group(1), rel)
    return out


def check_static(ledger, excl):
    defs = defined_functions()
    seen = set()
    for r in ledger:
        who = f"{r['c_function']} ({r['name']})"
        if r["c_function"] in seen:
            errors.append(f"ledger: {r['c_function']} listed twice")
        seen.add(r["c_function"])
        if r["status"] not in STATUSES:
            errors.append(f"ledger: {who}: status {r['status']!r} is not one of {STATUSES}")
        if not re.fullmatch(r"[0-9A-Fa-f]{8}", r["address"]):
            errors.append(f"ledger: {who}: bad address {r['address']!r}")
        try:
            if int(r["min_cases"]) < 0:
                raise ValueError
        except ValueError:
            errors.append(f"ledger: {who}: min_cases {r['min_cases']!r} is not a non-negative integer")
        src = ROOT / r["source"]
        if not src.is_file():
            errors.append(f"ledger: {who}: source {r['source']} does not exist")
        elif not re.search(r"\b%s\(" % re.escape(r["c_function"]), src.read_text(encoding="utf-8")):
            errors.append(f"ledger: {who}: {r['c_function']} is not defined in {r['source']}")
        chk = ROOT / r["checker"]
        if not chk.is_file():
            errors.append(f"ledger: {who}: checker {r['checker']} does not exist")
        elif r["min_cases"] != "0":                    # a helper diffed only through its callers (0 cases of its own) is not named
            text = chk.read_text(encoding="utf-8")
            base = r["name"].split(" (")[0]
            if base not in text and r["c_function"] not in text:
                errors.append(f"ledger: {who}: {r['checker']} does not mention {base} or {r['c_function']}")
    for name, rel in sorted(defs.items()):
        if name not in seen and name not in NOT_ORIGINAL:
            errors.append(f"{rel} defines {name} but tools/verified.csv has no row for it (add one, or list it in "
                          f"NOT_ORIGINAL of tools/check_verified.py with the reason)")
    for name in NOT_ORIGINAL:
        if name not in defs:
            warnings.append(f"NOT_ORIGINAL lists {name}, which is no longer defined in the scanned files")
        if name in seen:
            errors.append(f"{name} is both in the ledger and in NOT_ORIGINAL")
    # exclusion registry
    for r in excl:
        chk = ROOT / "tools/mips" / (r["checker"] + ".py")
        if not chk.is_file():
            errors.append(f"check_exclusions.csv: checker {r['checker']} does not exist ({r['kind']} {r['where']})")
            continue
        if r["kind"] not in KINDS:
            errors.append(f"check_exclusions.csv: unknown kind {r['kind']!r} ({r['checker']})")
        if not r["reason"]:
            errors.append(f"check_exclusions.csv: {r['checker']} {r['kind']} {r['where']}: the reason is empty")
        if r["kind"] == "unreachable":
            toks = r["where"].split()
            while toks and re.fullmatch(r"[0-9A-Fa-f]{8}(\.\.[0-9A-Fa-f]{8})?", toks[-1]):
                toks.pop()
            fname = " ".join(toks)
            if not fname or fname not in chk.read_text(encoding="utf-8"):
                errors.append(f"check_exclusions.csv: {r['checker']}: unreachable row names {fname!r}, which the checker does not mention")
    return defs


def check_results(ledger):
    """Compares the results of the slow checkers (extract/checks/results/*.json) with the ledger.  Returns the number of
    checkers whose results were found."""
    head = git_head()
    by_checker = {}
    for r in ledger:
        by_checker.setdefault(Path(r["checker"]).stem, []).append(r)
    found = 0
    gaps, stale_heads = [], []
    for stem, rows in sorted(by_checker.items()):
        path = RESULTS / f"{stem}.json"
        if not path.is_file():
            warnings.append(f"no results for {stem} (run it once at the default scale: uv run python tools/mips/{stem}.py)")
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            warnings.append(f"results of {stem} unreadable ({e}); run it again")
            continue
        found += 1
        if data.get("head") != head:
            stale_heads.append(stem)
        diffed = [r for r in rows if r["status"] == "diffed"]
        if not data.get("passed"):
            if diffed:
                errors.append(f"{stem}: the last default-scale run FAILED ({len(diffed)} diffed ledger rows depend on it)")
            continue
        funcs = data.get("functions", {})
        for r in diffed:
            ent = funcs.get(r["name"])
            if ent is None:
                if r["min_cases"] != "0":
                    warnings.append(f"{stem}: no result entry for {r['name']}")
                continue
            if ent["cases"] < int(r["min_cases"]):
                errors.append(f"{stem}: {r['name']} ran {ent['cases']} cases, the ledger requires >= {r['min_cases']}")
            if ent.get("uncovered"):
                gaps.append(f"{r['name']} ({ent['uncovered']})")
            if ent.get("stale_declared"):
                warnings.append(f"{stem}: {r['name']}: declared unreachable but executed: {ent['stale_declared']} (remove the row)")
    if stale_heads:
        warnings.append(f"results of {len(stale_heads)} checkers are from another git HEAD than {head[:8]} (stale; rerun to refresh): "
                        + ", ".join(stale_heads))
    for g in gaps:
        warnings.append(f"coverage gap at the default scale, not declared unreachable: {g}")
    return found


def progress(ledger):
    inv = {r["entry"].lower() for r in read_csv(INVENTORY)}
    diffed = {r["address"].lower() for r in ledger if r["status"] == "diffed"}
    wip = {r["address"].lower() for r in ledger if r["status"] == "wip"} - diffed
    for a in sorted(diffed | wip):
        if a not in inv:
            warnings.append(f"ledger address {a} is not in notes/inventory/functions.csv")
    print(f"ledger: {len(diffed)} functions diffed, {len(wip)} wip, of {len(inv)} functions in notes/inventory/functions.csv "
          f"({len(ledger)} rows)")


def report():
    for w in warnings:
        print("warning:", w)
    for e in errors:
        print("ERROR:", e)


def results_report():
    """The results part only (tools/run_checks.py calls it after the checks ran, when the results files are fresh).  Prints the
    progress line, the warnings and the errors; returns the number of errors (ledger regressions)."""
    del errors[:], warnings[:]
    ledger = read_csv(LEDGER)
    check_results(ledger)
    progress(ledger)
    report()
    return len(errors)


def main():
    results_only = "--results-only" in sys.argv[1:]
    ledger, excl = read_csv(LEDGER), read_csv(EXCLUSIONS)
    if not results_only:
        check_static(ledger, excl)
    found = check_results(ledger)
    progress(ledger)
    report()
    print(f"results found for {found} checkers; {len(errors)} errors, {len(warnings)} warnings")
    if errors:
        return 1
    print("OK: ledger and exclusion registry are consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
