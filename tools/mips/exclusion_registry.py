"""Exclusion registry and result summaries of the RAM-image differential checkers (tools/mips/check_*.py).  Not a check itself.

Every deliberate blind spot of a checker (an address range left out of the RAM comparison, a value that both sides receive
from the same seeded generator because the original reads uninitialised memory, the stack window, instructions excluded from
the coverage requirement, a mutation that was dropped) is declared in ONE reviewed file, tools/mips/check_exclusions.csv
(`checker,kind,where,reason`).  The harness code paths that create such a blind spot look the entry up here and stop the
check (exit code 3, a clear message) when the running checker uses one that is not declared.  Kinds and the `where` syntax:

  ram_mask         an address range excluded from the RAM / scratchpad comparison: `800B4D00:8` (hex address : length) or
                   `monkey+0xC8:4` (the same offset in all 8 monkey slots) or `800B9900+0x34x100+0x1B:1` (the same offset in
                   COUNT records of STRIDE bytes from BASE: BASE+0xSTRIDExCOUNT+0xOFFSET:LENGTH); also a free-form description
                   for a partial comparison that a checker declares itself (declare())
  shared_random    an uninitialised read (stack word, caller register, leftover $v0) for which both sides receive the same
                   value: `AE_STK_ENTER_1D+16` (scratch block name + offset, set_stale / set_stale_chain) or a free-form
                   description (`MonkeyAi_GoToFlaggedNode $s2`) that the checker declares with declare()
  stack_window     the stack range excluded from the comparison: `801FF000..80200000`
  unreachable      instructions excluded from the coverage requirement: `<function> LO..HI [LO..HI | ADDR ...]` (hex, inclusive);
                   the reason is the proof
  dropped_mutation a mutation removed from the mutation test as dead / equivalent code (documentation only)

tools/check_verified.py reads the same file (every row must name an existing checker) and the results written by
write_results() (extract/checks/results/<checker>.json).  Lowering a min_cases value in tools/verified.csv or adding a row here
needs the project owner's review (see notes/port/validation/ledger.md).
"""
import csv
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "tools/mips/check_exclusions.csv"
RESULTS_DIR = ROOT / "extract/checks/results"
KINDS = ("ram_mask", "shared_random", "stack_window", "unreachable", "dropped_mutation")
# slot patterns of `monkey+0xOFF:LEN` (mirrors monkey_harness.MONKEYS / MONKEY_SIZE, which asserts the equality)
PATTERNS = {"monkey": (0x800E5500, 0x250, 8)}
_RANGE = re.compile(r"^([0-9A-Fa-f]{8})(?:\.\.([0-9A-Fa-f]{8}))?$")

_rows = None
_stem = False
_used = set()
_ok_masks = {}


def checker_stem():
    """The running checker (the script started from the command line), or None when it is not a check_*.py script
    (diagnostics such as monkey_stack_probe.py are not checkers: nothing is enforced for them)."""
    global _stem
    if _stem is False:
        main = sys.modules.get("__main__")
        stem = Path(getattr(main, "__file__", "") or "").stem
        _stem = stem if stem.startswith("check_") else None
    return _stem


def rows():
    global _rows
    if _rows is None:
        with open(REGISTRY, newline="", encoding="utf-8") as f:
            _rows = [{k: (v or "").strip() for k, v in r.items()} for r in csv.DictReader(f)]
        for r in _rows:
            if r["kind"] not in KINDS:
                raise SystemExit(f"{REGISTRY}: unknown kind {r['kind']!r} (row {r})")
    return _rows


def _fail(msg):
    print(f"EXCLUSION REGISTRY: {msg}\n  declare it (with the reason) in {REGISTRY.relative_to(ROOT)}; adding a row needs the "
          f"project owner's review (notes/port/validation/ledger.md)", flush=True)
    raise SystemExit(3)


def declared(kind, checker=None):
    checker = checker or checker_stem()
    return [r for r in rows() if r["checker"] == checker and r["kind"] == kind]


def declare(kind, where, checker=None):
    """Requires the registry row (checker, kind, where); `where` is compared as written."""
    checker = checker or checker_stem()
    if checker is None or (checker, kind, where) in _used:
        return
    for r in rows():
        if r["checker"] == checker and r["kind"] == kind and r["where"] == where:
            _used.add((checker, kind, where))
            return
    _fail(f"{checker} uses an undeclared {kind}: {where}")


def _expand_mask(where):
    m = re.match(r"^([0-9A-Fa-f]{8})\+0x([0-9A-Fa-f]+)x(\d+)\+0x([0-9A-Fa-f]+):(\d+)$", where)
    if m:                       # BASE+0xSTRIDExCOUNT+0xOFFSET:LENGTH
        base, stride, count, off, ln = int(m.group(1), 16), int(m.group(2), 16), int(m.group(3)), int(m.group(4), 16), int(m.group(5))
        return {(base - 0x80000000 + stride * k + off, ln) for k in range(count)}
    m = re.match(r"^(?:monkey\+0x([0-9A-Fa-f]+)|([0-9A-Fa-f]{8})):(\d+)$", where)
    if not m:
        return None
    ln = int(m.group(3))
    if m.group(1):
        base, stride, count = PATTERNS["monkey"]
        return {(base - 0x80000000 + stride * k + int(m.group(1), 16), ln) for k in range(count)}
    return {(int(m.group(2), 16) - 0x80000000, ln)}


def check_ram_mask(entries, checker=None):
    """entries: [(RAM offset, length)] -- every one must be covered by a `ram_mask` row of the running checker."""
    checker = checker or checker_stem()
    if checker is None:
        return
    if checker not in _ok_masks:
        allowed = set()
        for r in declared("ram_mask", checker):
            exp = _expand_mask(r["where"])
            if exp:
                allowed |= exp
                _used.add((checker, "ram_mask", r["where"]))
        _ok_masks[checker] = allowed
    for off, ln in entries:
        if (off, ln) not in _ok_masks[checker]:
            base, stride, count = PATTERNS["monkey"]
            rel = 0x80000000 + off - base
            hint = f" (= monkey+0x{rel % stride:X}:{ln} in slot {rel // stride})" if 0 <= rel < stride * count else ""
            _fail(f"{checker} masks RAM {0x80000000 + off:08X}:{ln}{hint} out of the comparison without a ram_mask row")


def parse_unreachable(where):
    """'MonkeyFoo 8003B3B0..8003B3FC 8003B400' -> ('MonkeyFoo', [(lo, hi), ...]) (hi inclusive)"""
    toks = where.split()
    spans = []
    while toks and _RANGE.match(toks[-1]):
        a, b = _RANGE.match(toks.pop()).groups()
        spans.append((int(a, 16), int(b or a, 16)))
    return " ".join(toks), spans[::-1]


def unreachable_for(checker=None):
    """{function name: [(lo, hi)]} declared unreachable for the checker"""
    out = {}
    for r in declared("unreachable", checker):
        name, spans = parse_unreachable(r["where"])
        out.setdefault(name, []).extend(spans)
    return out


def ranges_text(pcs):
    out, i = [], 0
    while i < len(pcs):
        j = i
        while j + 1 < len(pcs) and pcs[j + 1] == pcs[j] + 4:
            j += 1
        out.append(f"{pcs[i]:08X}" if i == j else f"{pcs[i]:08X}..{pcs[j]:08X}")
        i = j + 1
    return " ".join(out)


def coverage_summary(items, cov, patched=None):
    """items: [(name, address, size, own)].  Returns {name: {...}}: instructions executed at least once, declared unreachable
    (registry), the rest 'uncovered' (not failing: reported), plus 'stale' = declared unreachable instructions that did execute.
    `patched`: instructions the checker replaces by a hook patch (they never execute as original code); each must be declared
    in the registry as unreachable too (they are simply not counted)."""
    checker = checker_stem()
    decl = unreachable_for(checker) if checker else {}
    out = {}
    for name, addr, size, own in items:
        pcs = list(range(addr, addr + size, 4))
        spans = decl.get(name, [])
        if own:
            _used.update((checker, "unreachable", r["where"]) for r in declared("unreachable", checker)
                         if parse_unreachable(r["where"])[0] == name)
        isdecl = lambda p: any(lo <= p <= hi for lo, hi in spans)  # noqa: E731
        executed = [p for p in pcs if p in cov]
        dec = [p for p in pcs if p not in cov and isdecl(p)]
        unc = [p for p in pcs if p not in cov and not isdecl(p)]
        stale = [p for p in pcs if p in cov and isdecl(p)]
        out[name] = dict(own=own, total=len(pcs), executed=len(executed), declared_unreachable=len(dec),
                         uncovered=ranges_text(unc), uncovered_count=len(unc), stale_declared=ranges_text(stale),
                         coverage_pct=round(100.0 * len(executed) / len(pcs), 2) if pcs else 100.0)
    return out


def print_coverage(summary):
    print("instruction coverage of the original code (executed at least once on the MIPS side; declared-unreachable ranges "
          "come from tools/mips/check_exclusions.csv):")
    te = tn = 0
    for name, s in summary.items():
        te += s["executed"]
        tn += s["total"]
        tail = ""
        if s["declared_unreachable"]:
            tail += f"   declared unreachable: {s['declared_unreachable']}"
        if s["uncovered"]:
            tail += "   never executed: " + s["uncovered"]
        if s["stale_declared"]:
            tail += "   STALE (declared unreachable but executed): " + s["stale_declared"]
        print(f"  {name:36s} {s['executed']:4d}/{s['total']:4d}{tail}")
    print(f"  total {te}/{tn} instructions")


def git_head():
    try:
        return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    except Exception:  # noqa: BLE001
        return ""


def write_results(counts, summary, passed, extra=None, function_names=None):
    """Writes extract/checks/results/<checker>.json.  counts: {function: cases}; summary: coverage_summary() or None.
    function_names: functions that are listed (default: every key of counts).  Called by the checkers after a run at the
    DEFAULT scale and seed only (check_verified.py compares the numbers with tools/verified.csv)."""
    stem = checker_stem()
    if stem is None:
        return None
    funcs = {}
    for name in (function_names if function_names is not None else counts):
        s = (summary or {}).get(name)
        funcs[name] = {"cases": counts.get(name, 0),
                       "coverage_pct": s["coverage_pct"] if s else None,
                       "uncovered": s["uncovered"] if s else None,
                       "declared_unreachable": s["declared_unreachable"] if s else None,
                       "stale_declared": s["stale_declared"] if s else None}
    data = {"checker": stem, "head": git_head(), "passed": bool(passed), "functions": funcs,
            "total_cases": sum(counts.values()), "coverage_collected": summary is not None}
    if extra:
        data.update(extra)
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    path = RESULTS_DIR / f"{stem}.json"
    path.write_text(json.dumps(data, indent=1), encoding="utf-8")
    return path
