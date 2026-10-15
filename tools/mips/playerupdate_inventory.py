"""Read-only call/dispatch inventory from the original PlayerUpdate instructions."""
import csv
from pathlib import Path
import mips_ram as mr

ROOT = Path(__file__).resolve().parents[2]


def main():
    cpu = mr.MipsRam.from_extract()
    symbols = {}
    for row in csv.reader((ROOT/'tools/symbols.csv').read_text().splitlines()):
        if len(row) > 1 and len(row[0]) == 8:
            try: symbols[int(row[0], 16)] = row[1]
            except ValueError: pass
    with (ROOT/'tools/verified.csv').open(newline='') as stream:
        ports = {int(row['address'], 16): row for row in csv.DictReader(stream)}
    symbols.update({0x80025D20: 'CdIntToPos', 0x80025E24: 'CdPosToInt',
                    0x8002FB50: 'rsin', 0x8002FC20: 'rcos',
                    0x800300B0: 'ApplyMatrixSV', 0x800301E0: 'VectorNormalSS',
                    0x800A3740: 'memset', 0x80142808: 'FUN_80142808'})
    listing = mr.listing(cpu, 0x80060780, 11944)
    calls = sorted({int(line.rsplit(' ', 1)[-1], 16) for line in listing if '  jal ' in line})
    print('| Address | Direct callee | Current implementation |')
    print('|---|---|---|')
    for address in calls:
        port = ports.get(address)
        route = port['source'] if port else 'named hook'
        if address in (0x80011F6C, 0x80012360):
            route += '; hardware-facing entry must remain a named archive hook'
        print(f'| {address:08X} | {symbols.get(address, "unknown")} | {route} |')
    print('\nIndirect call sites:')
    print('\n'.join(line for line in listing if 'jalr' in line))
    for name, table, count in [('mode jump table (modes 1..13)', 0x800A5A40, 13),
                               ('water-state jump table (states 0x46..0x4D)', 0x800A5A78, 8),
                               ('PlayerActionFns', 0x800B03AC, 40)]:
        print(f'\n{name} @{table:08X}:')
        for index in range(count):
            address = cpu.read32(table + 4*index)
            print(f'{index:2d}: {address:08X} {symbols.get(address, "internal branch")}')


if __name__ == '__main__':
    main()
