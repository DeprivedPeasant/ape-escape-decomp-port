# Native MIPS test oracle

`MipsRam.call` selects `MIPS_BACKEND=c` (default), `py`, or `both`.
This is a test tool; no interpreter or translated code is added to the port.

`mips_fast.c` implements the existing Python interpreter's instruction semantics.
`mips_fast.py` builds a source-hashed MinGW DLL on demand under `extract/mips`,
then exposes the existing bytearrays through ctypes `from_buffer` views. The C
loop reads and writes those buffers directly. Snapshots remain immutable byte
copies; restore uses memoryview assignment to avoid bytearray's temporary copy.

The state crossing the boundary contains registers, HI/LO, PC, pending branch,
pending load and remaining budget. The C loop yields before hooks and patches,
committing a pending load first. Python uses the original hook protocol and
resumes C with the changed registers. Hook visits consume budget without adding
an instruction to `steps`. A hook in a branch delay slot remains an error.
All hooks receive one live register list; aliases retained by an earlier hook
and the returned list preserve the Python protocol's identity and later writes.
For the exceptional Python protocol case where a hook writes raw negative or
wider-than-32-bit register values, execution continues from that boundary in the
unchanged reference instruction loop. Hooks are not rerun, and budget, HI/LO and
coverage remain continuous. Out-of-word root/SP values use the same compatibility
path. Ordinary game/checker register words execute in C.
Executed steps and coverage are published before Python callbacks, so hooks see
the same current prefix as in the reference loop. Validation checks those hook
entry fields and replays changes to counters, coverage and the hook log as well.

The pinned GTE oracle's exported function pointer and existing 64-word state go
directly to C, avoiding Python for every COP2 instruction. Custom Python GTE
backends also remain supported, with Python exceptions propagated at the boundary.

Coverage uses a bitmap for the three RAM aliases and scratchpad, plus a sparse
list of newly set bits. Invalid fetch PCs are recorded exactly. Python merges
only touched bits into the caller's existing set. The instruction cache preserves
the reference's virtual-address policy, including explicit `_decoded.clear()`
and backend switches; clearing RAM or restoring a snapshot does not implicitly
clear decoded instructions, just as before.

`both` first runs Python and records hook entry state and hook effects. It then
restores the original memory/GTE state and runs C, checking hook entry state and
replaying effects. User hooks run once, so external RNGs, logs and native callback
side effects are not duplicated. Each call compares all registers, HI/LO, every
RAM/scratchpad byte, coverage, instruction count, GTE state, hook order, returned
registers and exception type/message. Validation remains enabled under Python
`-O`. Instruction-prefix tests also compare pending branch/load state.

An overridden `MipsRam.write` remains an interception surface: C calls Python
only for CPUs with that override, and `both` records/replays its effects once.
This preserves the sound checker's SPU-store interception and checkpoints.

Run with the repository's Python, without changing any checker scale:

```powershell
$env:MIPS_BACKEND='both'
.venv/Scripts/python.exe tools/mips/check_mips_ram.py
.venv/Scripts/python.exe tools/mips/benchmark_backends.py
.venv/Scripts/python.exe tools/mips/benchmark_core.py
.venv/Scripts/python.exe tools/mips/benchmark_backends.py --report-only
```

The self-test covers every supported decoder arm, fixed-seed random streams,
edge operands, instruction-prefix state, delay slots, traps, memory faults,
unaligned merges, GTE operations, hooks and cache behavior. It separately builds
three intentionally incorrect C DLLs and requires each to fail equivalence:
immediate load visibility, unsigned `lh`, and a missing positive overflow trap.
These mutations never edit the production interpreter or any port source.

`benchmark_backends.py` runs every `tools/mips/check_*.py` under all three
backends, preserves logs and wall timings, and verifies identical result ledgers
including per-function case counts and coverage. `--backend` and `--checker`
can repeat selected runs while retaining the rest of the matrix. Logs and JSON
live in `extract/mips/validation`; the generated timing table is
`extract/codex-task/report.md`. `benchmark_core.py` separates warm interpreter
throughput from scenario setup, whole-RAM copies, port callbacks and C builds.
