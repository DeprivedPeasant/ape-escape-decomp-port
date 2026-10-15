"""Separate interpreter throughput from checker setup/copy/port costs."""
import json
import os
from pathlib import Path
import statistics
import time

import mips_ram as mr


def main():
    old_backend = os.environ.get('MIPS_BACKEND')
    results = {}
    try:
        for coverage in (False, True):
            for backend in ('py', 'c'):
                os.environ['MIPS_BACKEND'] = backend
                cpu = mr.MipsRam()
                cpu.coverage = set() if coverage else None
                # t0 = a0; repeat { v0++; t0--; } until t0 == 0.
                words = [0x24880000, 0x24420001, 0x2508ffff, 0x1500fffd,
                         0, 0x03e00008, 0]
                for i, word in enumerate(words):
                    cpu.write32(0x80100000+4*i, word)
                cpu.call(0x80100000, (10,))  # warm DLL/import/decode caches
                timings = []
                for _ in range(5):
                    start = time.perf_counter()
                    regs = cpu.call(0x80100000, (100_000,), limit=500_000)
                    timings.append(time.perf_counter()-start)
                    if regs[2] != 100_000:
                        raise AssertionError('benchmark loop result differs')
                results[f'{backend}_coverage_{coverage}'] = {
                    'seconds_median': statistics.median(timings), 'samples': timings,
                    'instructions': 400_003}
        cpu = mr.MipsRam()
        snap = cpu.snapshot()
        for name, restore in [('old_slice_restore', lambda: (cpu.ram.__setitem__(slice(None), snap[0]),
                                                            cpu.scratch.__setitem__(slice(None), snap[1]))),
                              ('memoryview_restore', lambda: cpu.restore(snap)),
                              ('snapshot', cpu.snapshot)]:
            samples = []
            for _ in range(5):
                start = time.perf_counter()
                for _ in range(1000):
                    restore()
                samples.append((time.perf_counter()-start)/1000)
            results[name] = {'seconds_median': statistics.median(samples), 'samples': samples}
        out = Path(__file__).resolve().parents[2] / 'extract/mips/validation/core_timings.json'
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(results, indent=2)+'\n')
        for name, row in results.items():
            print(f"{name}: {row['seconds_median']:.6f}s")
    finally:
        if old_backend is None:
            os.environ.pop('MIPS_BACKEND', None)
        else:
            os.environ['MIPS_BACKEND'] = old_backend


if __name__ == '__main__':
    main()
