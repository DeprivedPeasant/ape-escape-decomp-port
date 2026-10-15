"""Run every validation check of the project and summarise the results.

Discovers tools/**/check_*.py, src/**/check_*.py (tools/codex/retired is skipped), src/player/run_tests.sh and the other test entry points listed below, runs
each without arguments (in parallel, with a timeout) and writes extract/checks/<name>.json plus one log per check
under extract/checks/logs/. A check passes when it exits with 0.

Usage: uv run python tools/run_checks.py [-j N] [--timeout S] [--compare previous.json] [name-substring ...]
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_verified  # noqa: E402
OUT = ROOT / "extract/checks"
EXTRA = ["src/player/run_tests.sh"]
ARGS = {"src/player/check_tables.py": ["extract/disc/SCUS_944.23"]}
# Git for Windows' bash (a bare "bash" from Python can resolve to WSL's bash.exe)
def _git_bash():
    git = shutil.which("git")
    if os.name != "nt" or not git:
        return "bash"
    for parent in Path(git).resolve().parents:
        if (parent / "bin/bash.exe").is_file():
            return str(parent / "bin/bash.exe")
    return "bash"


BASH = _git_bash()
# checks that need arguments (a capture file, an emulator) or that are not standalone; run them by hand
SKIP = {
    "tools/codex/check_native_player.py",  # needs a capture .jsonl
}


# checks that build or load shared outputs (the viewer executable, shared DLLs, the audio oracle) race when run in
# parallel; they run one at a time after the others
SHARED_OUTPUT = ("native_viewer", "audio-audit", "native_actor", "pack_actor")


def is_serial(rel):
    return rel.endswith(".py") and any(k in (ROOT / rel).read_text(encoding="utf-8", errors="replace") for k in SHARED_OUTPUT)


def discover():
    found = sorted(str(p.relative_to(ROOT)).replace("\\", "/") for d in ("tools", "src") for p in (ROOT / d).rglob("check_*.py"))
    return [f for f in found if f not in SKIP and "/__pycache__/" not in f and "/retired/" not in f] + EXTRA


def run(rel, timeout):
    log = OUT / "logs" / (rel.replace("/", "__") + ".log")
    cmd = ([BASH, rel] if rel.endswith(".sh") else [sys.executable, rel]) + ARGS.get(rel, [])
    env = dict(os.environ, SANITIZE="0", PYTHONIOENCODING="utf-8")
    t0 = time.time()
    try:
        with open(log, "w", encoding="utf-8", errors="replace") as f:
            rc = subprocess.run(cmd, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT, timeout=timeout, env=env).returncode
        status = "pass" if rc == 0 else f"fail({rc})"
    except subprocess.TimeoutExpired:
        status = "timeout"
    tail = log.read_text(encoding="utf-8", errors="replace").strip().splitlines()[-1:] if log.exists() else []
    return {"check": rel, "status": status, "seconds": round(time.time() - t0, 1), "last": tail[0][:200] if tail else ""}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=6)
    ap.add_argument("--timeout", type=float, default=900)
    ap.add_argument("--out", default="last")
    ap.add_argument("--compare")
    ap.add_argument("filters", nargs="*")
    a = ap.parse_args()
    (OUT / "logs").mkdir(parents=True, exist_ok=True)
    checks = [c for c in discover() if not a.filters or any(f in c for f in a.filters)]
    parallel = [c for c in checks if not is_serial(c)]
    serial = [c for c in checks if is_serial(c)]
    with ThreadPoolExecutor(a.j) as ex:
        results = list(ex.map(lambda c: run(c, a.timeout), parallel))
    results += [run(c, a.timeout) for c in serial]
    (OUT / f"{a.out}.json").write_text(json.dumps(results, indent=1))
    for r in results:
        if r["status"] != "pass":
            print(f"{r['status']:10} {r['seconds']:7.1f}s  {r['check']}  | {r['last']}")
    npass = sum(r["status"] == "pass" for r in results)
    print(f"{npass} / {len(results)} checks pass ({sum(r['seconds'] for r in results):.0f} s total) -> {OUT / (a.out + '.json')}")
    ledger_errors = check_verified.results_report()      # the case counts of the slow checkers against tools/verified.csv
    if a.compare:
        old = {r["check"]: r["status"] for r in json.loads(Path(a.compare).read_text())}
        for r in results:
            if r["check"] in old and old[r["check"]] != r["status"]:
                print(f"CHANGED {r['check']}: {old[r['check']]} -> {r['status']}")
    sys.exit(0 if npass == len(results) and not ledger_errors else 1)


if __name__ == "__main__":
    main()
