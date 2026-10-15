# Local Claude implementation bridge

Codex coordinates and validates; isolated Claude Code workers implement bounded jobs using **claude-sonnet-5-5**. No MCP registration, new dependency, global install or application configuration change is required. The controller uses the existing authenticated Claude Code CLI. The live worker uses its subscription sign-in; reported dollar values are Claude's client estimates, not a promise about billing or subscription consumption.

Run from the repository root:

```powershell
python tools/codex/retired/bridge/bridge.py status
python tools/codex/retired/bridge/bridge.py enqueue task-name --prompt-file extract/codex/bridge/task.txt --output extract/codex/bridge/example.h --budget 1 --timeout 900
python tools/codex/retired/bridge/bridge.py run task-name
```

The prompt file contains the concrete problem, evidence paths, expected API and acceptance checks. Repeat `--output` for every permitted output file. Workers can currently deliver only explicit files under tools/codex/. Existing player/capture and naming ownership remains in force; do not assign those files through this bridge. Native-audio implementations can be delivered as new files for Codex to review and place in the owned audio lane.

Each job gets a detached worktree at its recorded committed base under ignored extract/codex/bridge/work/. This does not switch the shared checkout or use the existing Claude player worktree. Ignored game data, runtime captures and Ghidra databases are not present in the worker. Supply necessary facts in the prompt or committed text evidence; do not guess around missing assets. The worker cannot run builds, access the shell or launch subagents: restricted Read/Glob/Grep/Edit/Write tools only. Safe mode disables customizations; MCP is explicitly empty, so the project's Ghidra server is not started. This is Claude's restricted tool mode, not a separate Windows account or OS sandbox.

Jobs execute serially behind an exclusive lock. A job records its prompt, pinned model, base commit, output paths, time limit and per-call estimated dollar limit. The default bridge admission ceiling is $5 of reported/reserved estimates; an unknown interrupted run reserves its full allowance. Configure another ceiling explicitly with `configure --claude PATH --total-budget NUMBER`. Claude's own `--max-budget-usd` checks its estimated API-call cost and may overshoot by an in-flight request; neither ceiling guarantees a bank-account cap or a subscription-token allowance. No fallback model is configured, and unexpected reported model usage blocks acceptance.

The result stays **review_required** until Codex reviews it. Deliverable copies, SHA256s and the tracked-file patch live under the ignored job directory. New files are in deliverables/ and are not included in tracked.patch. Changed files outside the allowlist block collection. Primary file hashes flag concurrent changes. There is no automatic merge, commit, push, retry or modification of the existing Claude session. Re-check primary hashes and review scope before copying an accepted result; run independent tests, then commit exact owned files normally. A worker may report success without making changes, so inspect deliverables rather than trusting its prose.

Timeout stops only the worker process started by this controller. Shell/subagent tools are absent. Worktrees and logs are retained for review/recovery; no automatic deletion. If a controller crashes, its job can remain running and worker.lock persists. Do not remove that lock or kill a PID until ownership and process state are verified. Failed jobs are retained; retry by enqueueing a new ID. Rate limits, expired login and missing evidence become blockers rather than retry loops.

Validation: `python tools/codex/retired/bridge/check_bridge.py` runs offline fake-worker tests in real isolated worktrees. Checks cover path confinement, primary-tree preservation, restricted model/tool arguments, scoped result collection, repeated-run rejection, wrong-model/error handling, budget admission and timeout/lock release. A separate live smoke validates sign-in, actual model usage and file delivery; the fake worker does not establish live connectivity.

Official references: [programmatic Claude Code](https://code.claude.com/docs/en/headless), [model configuration](https://code.claude.com/docs/en/model-config), [CLI controls](https://code.claude.com/docs/en/cli-reference). CLI 2.1.288 was inspected locally. Model availability and billing remain account-dependent.

## Progress workflow

1. Codex chooses one evidence-backed task in its owned lane and writes acceptance checks.
2. Sonnet implements it in an isolated worktree; Codex continues independent review/research.
3. Codex inspects every changed file, runs reference/differential tests, and integrates only supported behavior.
4. Failed checks produce a bounded follow-up job with concrete diagnostics; no endless retry loop.
5. Record validation and limitations in the worklog/inbox and commit the owned implementation.

First implementation candidate: the integer reverb input/output FIR component, separately tested against an independent reference before replacing viewer reverb. This leaves local Claude's player/replay lane independent. Subsequent gameplay work should follow the coordination claims and use captures to choose the next missing behavior. The bridge does not itself keep Codex running overnight or enable disabled native computer control; those are separate execution capabilities.
