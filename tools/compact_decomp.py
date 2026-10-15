"""Shrink an ExportDecomp text file for reading: drops declarations, callee lists and noise, keeps the first N lines per function.

Usage: python tools/compact_decomp.py <in.c> <out.txt> <lines per function>
"""
import re,sys
src,dst,n=sys.argv[1],sys.argv[2],int(sys.argv[3])
s=open(src,encoding='utf-8',errors='replace').read()
parts=re.split(r'(?=/\* ==== )',s)
decl=re.compile(r'^\s+(undefined\d*|int|uint|short|ushort|byte|char|long|bool|[A-Za-z_]+) \**\w+(\[\d+\])?;$')
out=[]
for p in parts:
    ls=[l for l in p.splitlines() if l.strip() and not (decl.match(l) and '=' not in l) and not l.startswith('   callees') and not l.startswith('/* WARNING') and l.strip() not in ('{','}') and 'unaff' not in l and '& 3' not in l and 'trap(' not in l and not l.startswith('   callers')]
    out+= [l[:105] for l in ls[:n]]
open(dst,'w').write('\n'.join(out)); print(len(out))
