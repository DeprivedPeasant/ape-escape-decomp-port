"""Software top-down preview of an exported area OBJ (extract/obj/area_XX): checks textures, UVs, placement.

Needs numpy (not a project dependency: run with a Python that has it, e.g. `python tools/preview_obj.py ...`).
Usage: python tools/preview_obj.py extract/obj/area_01 area_01 out.png top 10   (last arg = pixels per OBJ unit)
"""
import sys,zlib,struct,numpy as np
from pathlib import Path
def readpng(p):
    b=Path(p).read_bytes(); pos=8; idat=b''
    while pos<len(b):
        n,=struct.unpack('>I',b[pos:pos+4]); t=b[pos+4:pos+8]; d=b[pos+8:pos+8+n]; pos+=12+n
        if t==b'IHDR': w,h=struct.unpack('>II',d[:8])
        if t==b'IDAT': idat+=d
    raw=zlib.decompress(idat); a=np.frombuffer(raw,np.uint8).reshape(h,w*4+1)[:,1:].reshape(h,w,4); return a
def writepng(p,a):
    h,w,_=a.shape; raw=b''.join(b'\0'+a[y].tobytes() for y in range(h))
    def ch(t,d): c=struct.pack('>I',len(d))+t+d; return c+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    Path(p).write_bytes(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b''))
folder=Path(sys.argv[1]); name=sys.argv[2]; out=sys.argv[3]; mode=sys.argv[4] if len(sys.argv)>4 else 'top'
V=[];VT=[];F=[];mat='white'
cols=[]
for l in (folder/f'{name}.obj').read_text().splitlines():
    p=l.split()
    if not p: continue
    if p[0]=='v': V.append([float(x) for x in p[1:4]]); cols.append([float(x) for x in p[4:7]] if len(p)>=7 else [1,1,1])
    elif p[0]=='vt': VT.append([float(p[1]),float(p[2])])
    elif p[0]=='usemtl': mat=p[1]
    elif p[0]=='f':
        idx=[tuple(int(t) if t else 0 for t in x.split('/')) + (0,0) for x in p[1:4]]
        F.append((mat,[(i[0]-1,i[1]-1 if i[1] else -1) for i in idx]))
V=np.array(V);cols=np.array(cols)
if mode=='front': V=V[:,[0,2,1]]*np.array([1,1,-1]); V[:,1]=-V[:,1]  # horizontal x, vertical (down-screen) -y, depth z
if mode=='side': V=V[:,[2,1,0]]; V[:,1]=-V[:,1]  # horizontal z
tex={}
def gettex(m):
    if m not in tex:
        f=folder/(f'tex_{m}.png' if m!='terrain_atlas' else 'terrain_atlas.png')
        if not f.exists():
            import glob
            g=glob.glob(str(folder/'tex*'/f'tex_{m}.png')); f=Path(g[0]) if g else f
        tex[m]=readpng(f) if f.exists() else None
    return tex[m]
S=int(sys.argv[5]) if len(sys.argv)>5 else 12  # px per unit
x0,x1=V[:,0].min(),V[:,0].max(); z0,z1=V[:,2].min(),V[:,2].max()
W=int((x1-x0)*S)+2;H=int((z1-z0)*S)+2
img=np.zeros((H,W,4),np.uint8); img[:]=(40,40,60,255); zb=np.full((H,W),-1e9)
for m,tr in F:
    if m=='white': continue
    pts=[(V[i][0],V[i][2],V[i][1],c) for i,c in tr]
    px=np.array([[ (p[0]-x0)*S,(p[1]-z0)*S] for p in pts]); y=np.array([p[2] for p in pts])
    uv=[VT[c] if c>=0 else None for _,c in tr]
    t=gettex(m)
    mn=np.floor(px.min(0)).astype(int); mx=np.ceil(px.max(0)).astype(int)
    xs=np.arange(max(mn[0],0),min(mx[0],W)); ys=np.arange(max(mn[1],0),min(mx[1],H))
    if len(xs)==0 or len(ys)==0: continue
    X,Y=np.meshgrid(xs+.5,ys+.5)
    a,b,c=px
    d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
    if abs(d)<1e-9: continue
    l1=((b[1]-c[1])*(X-c[0])+(c[0]-b[0])*(Y-c[1]))/d
    l2=((c[1]-a[1])*(X-c[0])+(a[0]-c[0])*(Y-c[1]))/d
    l3=1-l1-l2
    ok=(l1>=-1e-3)&(l2>=-1e-3)&(l3>=-1e-3)
    if not ok.any(): continue
    yy=l1*y[0]+l2*y[1]+l3*y[2]
    cc=np.array([cols[i] for i,_ in tr])
    col=(l1[...,None]*cc[0]+l2[...,None]*cc[1]+l3[...,None]*cc[2])
    if t is not None and uv[0] is not None:
        u=l1*uv[0][0]+l2*uv[1][0]+l3*uv[2][0]; v=l1*uv[0][1]+l2*uv[1][1]+l3*uv[2][1]
        tx=np.clip((u*255).astype(int),0,255); ty=np.clip(((1-v)*255).astype(int),0,255)
        rgb=t[ty,tx,:3].astype(float); alpha=t[ty,tx,3]
        rgb=rgb*np.clip(col*2,0,1.6)/1.0 if m!='terrain_atlas' else rgb
    else:
        rgb=col*255; alpha=np.full(X.shape,255)
    sub=zb[ys[0]:ys[-1]+1,xs[0]:xs[-1]+1]
    msk=ok&(yy>sub)&(alpha>0)
    sub[msk]=yy[msk]
    im=img[ys[0]:ys[-1]+1,xs[0]:xs[-1]+1]
    im[msk,:3]=np.clip(rgb[msk],0,255).astype(np.uint8)
writepng(out,img)
print(img.shape)
