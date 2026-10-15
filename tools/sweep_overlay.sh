#!/bin/bash
# Overlay / main-EXE naming sweep helper (run from Git Bash in the repo root).
# usage: tools/sweep_overlay.sh <overlay id like 19D>   -> notes/decomp/o<id>c.txt (compact decompile of the still-unnamed functions)
# Needs notes/inventory/functions.csv (DumpCallGraph.java) and tools/symbols.csv. Read the compact file, write names into
# tools/symbols.csv, run tools/propagate_names.py (after DumpFuncHashes.java), apply with the chained headless command.
set -e
cd "$(dirname "$0")/.."
OVL="$1"
python - "$OVL" <<'P'
import csv, sys
o = sys.argv[1]
syms = {l.split(',')[0].lower() for l in open('tools/symbols.csv', encoding='utf-8', errors='replace') if l and l[0] != '#'}
a = [r['entry'] for r in csv.DictReader(open('notes/inventory/functions.csv'))
     if r['block'] == 'ovl_' + o and r['name'].startswith('FUN_') and r['entry'].lower() not in syms]
open('notes/decomp/roots_%s.txt' % o, 'w').write(' '.join(a))
print(len(a), 'unnamed functions in ovl_' + o)
P
ROOTS=$(cat notes/decomp/roots_$OVL.txt)
powershell.exe -NoProfile -Command "& '<repo>\tools\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat' '<repo>\ghidra-projects' ApeEscape -process SCUS_944.23 -noanalysis -readOnly -scriptPath '<repo>\tools\ghidra_scripts' -postScript ExportDecomp.java '<repo>\notes\decomp\o$OVL.c' 0 400 $ROOTS *> '<repo>\ghidra-projects\run.stdout.txt'"
python tools/compact_decomp.py notes/decomp/o$OVL.c notes/decomp/o${OVL}c.txt 28
