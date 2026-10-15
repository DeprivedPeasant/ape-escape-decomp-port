"""Run the repo-local Codex CLI without the desktop runtime ACL conflict."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    executable = (
        root / "extract/codex/deps/codex-cli/node_modules/@openai/codex-win32-x64"
        / "vendor/x86_64-pc-windows-msvc/bin/codex.exe"
    )
    if not executable.is_file():
        print(f"Repo-local Codex CLI is missing: {executable}", file=sys.stderr)
        return 1

    # Codex 0.162.1 recursively updates ACLs beneath LOCALAPPDATA/OpenAI/Codex/
    # runtimes during elevated sandbox setup, including running desktop helpers.
    # Scope the alternate cache location to this child, leaving Windows and the
    # desktop app's environment/configuration untouched. Keep CODEX_HOME for login.
    local_app_data = root / "extract/codex/cli-localappdata"
    local_app_data.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["LOCALAPPDATA"] = str(local_app_data)
    command = [
        str(executable), "exec", "--ignore-user-config",
        "--sandbox", "workspace-write", "-c", 'windows.sandbox="elevated"',
        "--enable", "code_mode_host",
        "--disable", "computer_use", "--disable", "browser_use",
        "--disable", "in_app_browser", "--disable", "plugins",
        *sys.argv[1:],
    ]
    # default model unless the caller chose one (--model / -m may be given only once)
    if not any(a in ("--model", "-m") or a.startswith("--model=") for a in sys.argv[1:]):
        command[2:2] = ["--model", "gpt-6.1-sol"]
    # Preserve cwd, stdin, stdout, stderr and the CLI exit status for Claude.
    return subprocess.call(command, env=environment)


if __name__ == "__main__":
    raise SystemExit(main())
