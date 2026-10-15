"""Show the state of the Codex agents running in extract/worktrees/*.

For each worktree with a recent extract/codex-task/run.jsonl it reports:
  - state: RUNNING (a codex.exe process works in that worktree), FINISHED (turn completed, report.md written),
    FAILED (turn failed, with the error, e.g. a usage limit), STOPPED (no process and no final event; crashed or
    killed), plus out-of-memory errors found in run.err;
  - how long it has been working (log created -> last write, or -> now while running) and when it last wrote;
  - the model / reasoning effort it was launched with, and token usage when the run finished;
  - its most recent progress messages.

Usage (from the repository root):
  uv run python tools/codex/agent_status.py              # agents active in the last 6 hours
  uv run python tools/codex/agent_status.py -n 3         # show the last 3 messages per agent
  uv run python tools/codex/agent_status.py --hours 72   # widen the window
  uv run python tools/codex/agent_status.py --watch 60   # refresh every 60 seconds (Ctrl+C to stop)
  uv run python tools/codex/agent_status.py luna7-ovl187 # only the named worktree(s)
"""
import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import textwrap
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKTREES = ROOT / "extract" / "worktrees"
OOM_MARKERS = ("os error 1450", "os error 1455", "paging file is too small", "Insufficient system resources")


def codex_command_lines() -> list[str]:
    """Command lines of running codex.exe processes (Windows), lower-cased with forward slashes."""
    if os.name != "nt":
        return []
    script = ("Get-CimInstance Win32_Process -Filter \"name='codex.exe'\" | "
              "ForEach-Object { $_.CommandLine }")
    try:
        out = subprocess.run(["powershell.exe", "-NoProfile", "-Command", script],
                             capture_output=True, text=True, timeout=30).stdout
    except (OSError, subprocess.SubprocessError):
        return []
    return [line.strip().lower().replace("\\", "/") for line in out.splitlines() if line.strip()]


def read_events(path: Path) -> list[dict]:
    events = []
    try:
        with path.open(encoding="utf-8", errors="replace") as stream:
            for line in stream:
                line = line.strip()
                if line.startswith("{"):
                    try:
                        events.append(json.loads(line))
                    except json.JSONDecodeError:
                        pass  # a line still being written
    except OSError:
        pass
    return events


def duration(seconds: float) -> str:
    seconds = int(max(seconds, 0))
    hours, rest = divmod(seconds, 3600)
    return f"{hours}h{rest // 60:02d}m" if hours else f"{rest // 60}m{rest % 60:02d}s"


def launch_settings(worktree: Path, processes: list[str]) -> str:
    key = str(worktree).lower().replace("\\", "/")
    for line in processes:
        if key in line:
            model = line.split("--model", 1)[1].split()[0].strip('"') if "--model" in line else "default"
            effort = "max" if "reasoning_effort" in line and "max" in line else ""
            return f"{model} {effort}".strip()
    return ""


def agent_report(worktree: Path, processes: list[str], messages: int, width: int) -> list[str]:
    task = worktree / "extract" / "codex-task"
    log = task / "run.jsonl"
    events = read_events(log)
    stat = log.stat()
    created, modified, now = stat.st_ctime, stat.st_mtime, time.time()
    key = str(worktree).lower().replace("\\", "/")
    alive = any(key in line for line in processes)
    final = next((e for e in reversed(events) if e.get("type") in ("turn.completed", "turn.failed")), None)
    report = task / "report.md"
    has_report = report.exists() and report.stat().st_size > 0 and report.stat().st_mtime >= created

    if alive:
        state = "RUNNING"
    elif final and final["type"] == "turn.failed":
        state = "FAILED"
    elif final and final["type"] == "turn.completed":
        state = "FINISHED" if has_report else "FINISHED (no report)"
    else:
        state = "STOPPED"
    worked = (now if alive else modified) - created
    idle = now - modified

    lines = [f"== {worktree.name}: {state}",
             f"   working {duration(worked)}; last output {duration(idle)} ago "
             f"(started {dt.datetime.fromtimestamp(created):%H:%M}, log {stat.st_size // 1024} KB)"]
    settings = launch_settings(worktree, processes)
    if settings:
        lines.append(f"   model: {settings}")
    if final and final["type"] == "turn.failed":
        error = final.get("error", {}).get("message", "")
        lines.append("   error: " + textwrap.shorten(error, width - 10))
    if final and final["type"] == "turn.completed":
        usage = final.get("usage", {})
        lines.append(f"   tokens: {usage.get('input_tokens', 0):,} in, {usage.get('output_tokens', 0):,} out")
    try:
        err_text = (task / "run.err").read_text(encoding="utf-8", errors="replace")
        if any(marker in err_text for marker in OOM_MARKERS):
            lines.append("   WARNING: out-of-memory errors in run.err")
    except OSError:
        pass
    texts = [e["item"]["text"] for e in events
             if e.get("type") == "item.completed" and e.get("item", {}).get("type") == "agent_message"]
    for text in texts[-messages:]:
        body = " ".join(text.split())
        wrapped = textwrap.wrap(body, width - 6)
        if len(wrapped) > 4:
            wrapped = wrapped[:4]
            wrapped[-1] += " ..."
        lines += ["   > " + wrapped[0]] + ["     " + rest for rest in wrapped[1:]]
    if has_report and state.startswith("FINISHED"):
        lines.append(f"   report: {report.relative_to(ROOT)}")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("names", nargs="*", help="worktree names (default: all recently active)")
    parser.add_argument("-n", "--messages", type=int, default=2, help="recent messages per agent (default 2)")
    parser.add_argument("--hours", type=float, default=6, help="only logs written in the last N hours (default 6)")
    parser.add_argument("--watch", type=int, default=0, metavar="SECONDS", help="refresh every SECONDS")
    args = parser.parse_args()
    width = max(80, min(160, os.get_terminal_size().columns if sys.stdout.isatty() else 120))

    while True:
        processes = codex_command_lines()
        cutoff = time.time() - args.hours * 3600
        logs = sorted(WORKTREES.glob("*/extract/codex-task/run.jsonl"))
        selected = [log.parents[2] for log in logs
                    if (log.parents[2].name in args.names) if args.names] if args.names else \
                   [log.parents[2] for log in logs if log.stat().st_mtime >= cutoff]
        output = [f"Codex agents at {dt.datetime.now():%Y-%m-%d %H:%M:%S}  "
                  f"({len(processes)} codex.exe processes running)"]
        counts = {}
        for worktree in selected:
            block = agent_report(worktree, processes, args.messages, width)
            state = block[0].split(": ", 1)[1].split(" ")[0]
            counts[state] = counts.get(state, 0) + 1
            output += [""] + block
        if not selected:
            output.append("no agent logs match")
        output += ["", "summary: " + ", ".join(f"{n} {s.lower()}" for s, n in sorted(counts.items()))]
        if args.watch:
            os.system("cls" if os.name == "nt" else "clear")
        print("\n".join(output), flush=True)
        if not args.watch:
            return 0
        try:
            time.sleep(args.watch)
        except KeyboardInterrupt:
            return 0


if __name__ == "__main__":
    sys.exit(main())
