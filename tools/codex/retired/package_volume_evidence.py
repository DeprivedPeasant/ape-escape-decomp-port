"""Decode original collision dispatch tables and package private Ghidra text exports."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'extract/cloud_evidence/player/volume_dependencies'
SCRATCH=ROOT/'notes/decomp/codex/cloud-volume-dependencies'

def main():
    exe=(ROOT/'extract/disc/SCUS_944.23').read_bytes();names={}
    for line in (ROOT/'tools/symbols.csv').read_text().splitlines():
        if line.startswith('#') or ',' not in line:continue
        a,n,*_=line.split(',')
        try:names[int(a,16)]=n
        except ValueError:pass
    tables=[];roots={0x8004D8C0}
    for name,base,count in [('g_CollShapeHandlers',0x800AF180,89),('grid_probes',0x800B20E0,10),('collider_probes',0x800B2108,10)]:
        rows=[]
        for i in range(count):
            ptr=struct.unpack_from('<I',exe,base-0x80010000+0x800+i*4)[0]
            assert 0x80010000<=ptr<0x800C0000 and ptr%4==0,(name,i,hex(ptr))
            roots.add(ptr);rows.append(dict(index=i,address=f'{ptr:08X}',name=names.get(ptr,'unnamed')))
        tables.append(dict(name=name,base=f'{base:08X}',count=count,entries=rows))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'dispatch_tables.json').write_bytes((json.dumps(dict(exe_sha256=hashlib.sha256(exe).hexdigest(),tables=tables),indent=2)+'\n').encode())
    local=ROOT/'extract/codex/volume-roots.txt';local.write_text(''.join(f'{a:08X}\n' for a in sorted(roots)))
    if '--roots-only' in sys.argv:
        print(f'{len(roots)} unique roots; {sum(t["count"] for t in tables)} verified dispatch entries');return
    files={}
    for p in sorted(SCRATCH.iterdir()):
        if not (p.name.endswith('.c') or p.name.endswith('.asm.txt') or p.suffix=='.tsv'):continue
        data=p.read_bytes().replace(b'\r\n',b'\n');data.decode('utf-8');(OUT/p.name).write_bytes(data)
        files[p.name]=hashlib.sha256(data).hexdigest()
    files['dispatch_tables.json']=hashlib.sha256((OUT/'dispatch_tables.json').read_bytes()).hexdigest()
    volume=json.loads((ROOT/'extract/codex/native/cloud-volume-validation.json').read_text())
    ramps=json.loads((ROOT/'extract/codex/native/ray-ramp-validation.json').read_text())
    smoke=json.loads((ROOT/'extract/codex/native/original-player-validation.json').read_text())
    assert not volume['failures'] and ramps['failures']==0 and smoke['failures']==0
    assert len(volume['areas'])==101 and volume['cloud_revision']==smoke['source']
    validation=dict(cloud_revision=volume['cloud_revision'],areas=len(volume['areas']),
                    volume=volume['totals'],failures=len(volume['failures']),ramp_returns=ramps,viewer=smoke,
                    scope='Real area records at spawn points and collider centers/corners, seeded query comparisons to original MIPS. Eight unsupported queries excluded. No continuous body/world replay or original end-to-end ledge-grab validation.')
    (OUT/'validation.json').write_bytes((json.dumps(validation,indent=2)+'\n').encode())
    files['validation.json']=hashlib.sha256((OUT/'validation.json').read_bytes()).hexdigest()
    report=dict(roots=len(roots),functions=len(list(SCRATCH.glob('*.c'))),files=files)
    (OUT/'manifest.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    (OUT/'README.md').write_bytes(b'''# Collision volume dependency supplement (generated)

Original EXE dispatch tables: shape handlers 0x800AF180 (89 entries), grid
probes 0x800B20E0 and collider probes 0x800B2108 (10 entries each).
Entry counts match implemented shape ids 0..88 and geometry classes 0..9.
All pointers are aligned main-EXE addresses. Unique targets plus GetSolidHeight
0x8004D8C0 exported from private read-only Ghidra with named direct callees.
See dispatch_tables.json, functions.tsv, dependencies.tsv and manifest.json.
Reference pseudocode/listings only; no embedded game binary/asset or port source.
The existing Codex collision routines already implement/test these handler families.
GetSolidHeight scratch 0x1F8003FA is contact record field extra_b (id), zero
when the GetGroundHeight scratch reset remains unmodified.
''')
    with (OUT/'README.md').open('ab') as f:
        f.write(f'''\n## Local integration and original-instruction checks\n\nCloud source revision {volume['cloud_revision']} was staged unchanged.\nCodex host adapter now passes collider scan and mode-0 ray callbacks instead of\nNULL, using original area grids/colliders and existing native shape/height/probe\nhandlers. Unsupported dispatch stays flagged and blocks a ray.\n\nAll {len(volume['areas'])} areas: {volume['totals']['scan']} matched collider scans,\n{volume['totals']['ray']} matched mode-0 ray queries, {volume['totals']['unknown']}\nunsupported queries excluded, zero mismatches. Test positions are real spawn\nrecords and collider centers/corners; these are seeded queries, not gameplay\nreplay. Run tools/codex/check_cloud_volumes.py locally with extracted data.\n\nOriginal CollisionRayTest passes output at 0x1F800124, aliasing CollProbe_Ramp's\ntop/height scratch at 0x1F800124/128. The ramp's x push can replace its top before\nlater overlap/edge tests. Native ray dispatch reproduces this alias; ordinary\nmovement ramp dispatch retains distinct output behavior. Regression: area 0x17,\nposition (-7936,-2048,2368), velocity (0,-20,0), probe radius64/height128.\nBefore correction native blocked, original clear; now match.\nRun check_cloud_volumes.py --area 0x17 --regression-case and check_ray_ramps.py.\nThe separate aliased-ramp return sweep matches {ramps['comparisons']} comparisons\n({ramps['hits']} hits); it does not compare unused output words.\n\nSDL viewer smoke: {smoke['frames']} ticks, update/body/volume flags zero,\nwalk/run/jump/landing checked. Run tools/codex/run_native_player.cmd. Original\nend-to-end ledge grabbing, continuous body replay, other ray modes, animation\nside effects and gameplay sound/effects remain unvalidated or pending.\nSee validation.json for machine-readable scope and counts.\n'''.encode())
    print(report['functions'],'functions packaged')

if __name__=='__main__':main()
