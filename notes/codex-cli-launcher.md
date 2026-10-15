# Claude-to-Codex CLI launcher (2026-10-13)

From the repository root, Claude can call:

```text
uv run python tools/codex/run_cli.py --json --ephemeral "Your task here"
```

For the existing world worktree:

```text
uv run python tools/codex/run_cli.py -C <repo>/extract/worktrees/codex-world --json "Your task here"
```

The wrapper already supplies `exec`. It accepts its remaining arguments, including
`-C`, `--model`, `--output-last-message`, and `-` for a prompt on stdin. It preserves
the caller's working directory, streams and Codex exit code. The default model is
`gpt-6.1-sol`; explicitly select another model with `--model` if needed.

It uses the existing repo-local npm installation of `@openai/codex` 0.162.1 under
`extract/codex/deps/codex-cli/` (version and npm integrity in `VERSION.txt` and
`package-lock.json`). It keeps the existing login via `CODEX_HOME`/`~/.codex`.
It ignores the desktop user configuration and disables desktop plugins/browser/
computer-use for this invocation. Custom user MCP settings must be supplied
explicitly if a task needs them. The command host stays enabled.

The elevated Windows sandbox and `workspace-write` remain enabled. This does not
select `unelevated`, full access, or an approval bypass. No user application settings
or global installation are changed.

## Confirmed cause and workaround

The failure was reproduced after the desktop app had already updated to build
26.1007.21434. In CLI 0.162.1, elevated sandbox setup scans
`LOCALAPPDATA/OpenAI/Codex/runtimes` and updates runtime ACLs. A running desktop
`node_repl.exe` makes its root-only ACL update fail with Windows error 32. Disabling
the command host removes usable command execution for the tested model.

The launcher gives only its child process a repository-local `LOCALAPPDATA`:
`extract/codex/cli-localappdata/`. Sandbox setup therefore does not scan the desktop
runtime tree. Programs run by this CLI also inherit that cache location; tasks
depending on user-specific LocalAppData paths should use explicit paths.

Evidence: `extract/codex/cli-isolated-smoke.jsonl` reproduces the failure;
`extract/codex/cli-private-appdata-smoke.jsonl` records successful PowerShell execution
with exit code 0. The final launcher check is in
`extract/codex/cli-launcher-smoke.jsonl`. These logs are ignored.
The launcher also passed a prompt-through-stdin command check in the world worktree
(`extract/codex/cli-world-smoke.jsonl`, command exit code 0).

References:
- https://learn.chatgpt.com/docs/windows/windows-sandbox
- https://learn.chatgpt.com/docs/config-file/config-reference
- Upstream implementation inspected (facts only):
  https://github.com/openai/codex/blob/main/codex-rs/windows-sandbox-rs/src/setup_provisioning/setup_runtime_bin.rs

Codex can exit 0 even when an agent reports a tool failure. For automation, inspect
the JSON events and required task evidence as well as the process exit code.
