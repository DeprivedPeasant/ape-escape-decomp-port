"""Run every unchanged checker at default scale, keeping logs and wall timings."""
import json
import argparse
import os
import re
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'extract/mips/validation'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', action='append', choices=('both', 'py', 'c'))
    parser.add_argument('--checker', action='append', help='checker stem; repeat to select several')
    parser.add_argument('--report-only', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    partial = args.backend or args.checker or args.report_only
    timings = OUT / 'timings.json'
    results = json.loads(timings.read_text()) if partial and timings.exists() else []
    for backend in [] if args.report_only else args.backend or ('both', 'py', 'c'):
        for checker in sorted((ROOT / 'tools/mips').glob('check_*.py')):
            if args.checker and checker.stem not in args.checker:
                continue
            env = dict(os.environ, MIPS_BACKEND=backend, PYTHONUNBUFFERED='1')
            log = OUT / f'{checker.stem}_{backend}.log'
            start = time.perf_counter()
            with log.open('w', encoding='utf-8') as stream:
                process = subprocess.run([sys.executable, str(checker)], cwd=ROOT, env=env,
                                         stdout=stream, stderr=subprocess.STDOUT)
            elapsed = time.perf_counter()-start
            ledger = ROOT / f'extract/checks/results/{checker.stem}.json'
            result = dict(checker=checker.stem, backend=backend, seconds=elapsed,
                          exit_code=process.returncode, log=str(log.relative_to(ROOT)),
                          ledger=json.loads(ledger.read_text()) if ledger.exists() else None)
            results = [row for row in results if (row['checker'], row['backend']) != (checker.stem, backend)]
            results.append(result)
            timings.write_text(json.dumps(results, indent=2)+'\n')
            print(f'{checker.stem} {backend}: exit={process.returncode} {elapsed:.3f}s', flush=True)
            if process.returncode:
                print(log.read_text()[-6000:], flush=True)
                return process.returncode
    for checker in sorted({row['checker'] for row in results}):
        rows = [row for row in results if row['checker'] == checker]
        reference = rows[0]['ledger']
        for row in rows[1:]:
            if row['ledger'] != reference:
                raise AssertionError(f'{checker}: backend case counts or coverage results differ')
    expected = {(p.stem, b) for p in (ROOT / 'tools/mips').glob('check_*.py') for b in ('both', 'py', 'c')}
    if {(r['checker'], r['backend']) for r in results} == expected:
        if any(row['exit_code'] or not row['ledger'] or not row['ledger']['passed'] for row in results):
            raise AssertionError('some checker results are missing or failed')
        print('All checker results, case counts and coverage summaries match across both/py/c.', flush=True)
        write_report(results)
    else:
        print('Requested checks passed; the complete backend matrix is not recorded yet.', flush=True)
    return 0


def write_report(results):
    lines = ['# Fast MIPS oracle validation', '',
             '## Files and design', '',
             '- `tools/mips/mips_fast.c`: hand-written C interpreter loop.',
             '- `tools/mips/mips_fast.py`: ctypes binding, source-hashed DLL build and sparse caches.',
             '- `tools/mips/mips_ram.py`: backend selection, validation/replay and faster restore.',
             '- `tools/mips/check_mips_ram.py`: expanded equivalence and C mutation suite.',
             '- `tools/mips/benchmark_backends.py`, `benchmark_core.py`: reproducible timings and report.',
             '- `tools/mips/FAST_BACKEND.md`: usage and implementation details.', '',
             'C operates directly on the existing RAM/scratchpad bytearrays. It yields to Python '
             'at hooks/patches, preserving branch/load delay and budget semantics. The pinned GTE '
             'function pointer operates on the same oracle state directly from C. Coverage uses '
             'a bitmap plus touched-PC list. Instruction-cache behavior survives backend switches '
             'and explicit clears. Overridden memory writes remain Python callbacks. In both mode, '
             'hooks and write callbacks run once on the reference side and their effects are '
             'validated/replayed on C, preserving external side effects.', '',
             'Hooks see current instruction counts and coverage before they run. '
             'A single live register list preserves aliases retained across hooks and at return. '
             'Exceptional raw negative/wide Python hook-register values continue from that '
             'boundary in the reference loop without rerunning hooks; ordinary register '
             'words and the game checkers execute in C.', '',
             'No `src/` files or differential-checker logic/case counts were changed. '
             'Changes are uncommitted in the working tree.', '',
             '## Validation and timings', '',
             'Every checker ran at default scale with `.venv/Scripts/python.exe`. '
             'All exit codes are zero; per-function case counts and coverage summaries '
             'are identical across `both`, `py`, and `c`. Timings are process wall time, '
             'including checker DLL and GTE builds.', '',
             '| Checker | Cases | both (s) | py (s) | c (s) | py/c |',
             '|---|---:|---:|---:|---:|---:|']
    totals = dict.fromkeys(('both', 'py', 'c'), 0.0)
    for checker in sorted({r['checker'] for r in results}):
        rows = {r['backend']: r for r in results if r['checker'] == checker}
        cases = rows['py']['ledger'].get('total_cases', 0)
        values = [rows[b]['seconds'] for b in ('both', 'py', 'c')]
        for b, value in zip(totals, values):
            totals[b] += value
        lines.append(f'| {checker} | {cases} | {values[0]:.3f} | {values[1]:.3f} | '
                     f'{values[2]:.3f} | {values[1]/values[2]:.2f}x |')
    lines += ['', f"Total: both {totals['both']:.3f}s; py {totals['py']:.3f}s; "
              f"c {totals['c']:.3f}s; py/c {totals['py']/totals['c']:.2f}x.", '',
              'The interpreter self-test always includes its forced-both equivalence suite '
              'and isolated C mutation builds, even when launched under py or c.', '',
              'Machine-readable results and full checker output: '
              '`extract/mips/validation/timings.json` and `check_*_<backend>.log`.', '']
    selftest = (OUT / 'check_mips_ram_both.log').read_text()
    match = re.search(r'backend equivalence: (\d+) instruction-prefix cases passed', selftest)
    lines += [f"Equivalence suite: {match.group(1) if match else 'see log'} cases, including "
              'instruction-prefix state comparisons, every supported decoder arm, random streams, '
              'edge operands, faults, GTE, hooks, memory interception and cache behavior. '
              'Three isolated C mutations were all caught: immediate load visibility, unsigned '
              '`lh`, and missing positive overflow detection.', '',
              'No known backend differences remain in the validated surface.', '']
    core_path = OUT / 'core_timings.json'
    if core_path.exists():
        core = json.loads(core_path.read_text())
        lines += ['## Core throughput and snapshots', '']
        for coverage in (False, True):
            py = core[f'py_coverage_{coverage}']['seconds_median']
            c = core[f'c_coverage_{coverage}']['seconds_median']
            lines += [f'400,003 instructions, coverage={coverage}: py {py:.6f}s, '
                      f'c {c:.6f}s, {py/c:.2f}x (median of five warm runs).', '']
        old = core['old_slice_restore']['seconds_median']
        new = core['memoryview_restore']['seconds_median']
        lines += [f'2 MiB restore: old slice assignment {old*1e6:.1f} us, '
                  f'memoryview {new*1e6:.1f} us, {old/new:.2f}x. '
                  f"Snapshot: {core['snapshot']['seconds_median']*1e6:.1f} us "
                  '(one required immutable copy).', '']
    profile = OUT / 'profile_monkey_level1_c.prof'
    if profile.exists():
        import pstats
        stats = pstats.Stats(str(profile))
        def cumulative(filename, function):
            return sum(row[3] for (path, _, name), row in stats.stats.items()
                       if Path(path).name == filename and name == function)
        lines += ['## Remaining costs', '',
                  f'A representative C-backend profile of monkey level 1 (11,843 cases; '
                  f'{stats.total_tt:.2f}s instrumented wall time) shows:', '',
                  f"- Whole-RAM comparison/copies/masks: {cumulative('monkey_harness.py', 'compare'):.2f}s.",
                  f"- Scenario world construction: {cumulative('monkey_harness.py', 'build_monkey_world'):.2f}s cumulative.",
                  f"- Copying RAM into the port: {cumulative('monkey_harness.py', 'sync_to_c'):.2f}s.",
                  f"- Subprocesses, principally checker/GTE DLL builds: {cumulative('subprocess.py', 'run'):.2f}s.",
                  f"- Entire native interpreter boundary, including hooks and coverage: {cumulative('mips_fast.py', 'call'):.2f}s.", '',
                  'Profiling adds Python overhead and cumulative setup times overlap; these '
                  'figures explain the remaining work, while the table above uses unprofiled '
                  'process wall times. Both mode additionally copies/compares all memory and '
                  'records hook effects for every call. No checker setup or comparison logic '
                  'was relaxed to improve timings.', '']
    destination = ROOT / 'extract/codex-task/report.md'
    destination.write_text('\n'.join(lines), encoding='utf-8')
    print(f'Report: {destination}', flush=True)


if __name__ == '__main__':
    sys.exit(main())
