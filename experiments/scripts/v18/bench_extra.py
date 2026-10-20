"""其余已有算法在同样 5 张图像上的串行 CPU 时间（压缩率—速度图），预热 1 次、重复 3 次。"""
import json,sys,tempfile,statistics as st,os
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import bench_v18 as B
V=B.V;R=B.R
CODE='''import sys;sys.path.insert(0,{ic!r});import imagecodecs as ic,numpy as np,io;from PIL import Image
x=np.array(Image.open(sys.argv[1]));mode=sys.argv[2];out=sys.argv[3];m={m!r}
E={{'png9':lambda:ic.png_encode(x,level=9),'j2k':lambda:ic.jpeg2k_encode(x,level=0,reversible=True,codecformat='j2k'),'jxl_e9':lambda:ic.jpegxl_encode(x,lossless=True,effort=9,usecontainer=True,numthreads=1)}}
D={{'png9':ic.png_decode,'j2k':ic.jpeg2k_decode,'jxl_e9':lambda b:ic.jpegxl_decode(b,numthreads=1)}}
if m=='webp_m6':
  if mode=='e':
    bio=io.BytesIO();Image.fromarray(x).save(bio,format='WEBP',lossless=True,quality=100,method=6,exact=True);open(out,'wb').write(bio.getvalue())
  else:
    y=np.array(Image.open(out).convert('L'));assert np.array_equal(y,x)
else:
  if mode=='e':open(out,'wb').write(E[m]())
  else:y=D[m](open(out,'rb').read());assert np.array_equal(np.asarray(y).reshape(x.shape),x)'''
def py(m):
    code=CODE.format(ic=B.IC,m=m)
    def run(img):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/'a.bin';exe=str(R.parent/'.venv/bin/python')
            e=B.timed([exe,'-I','-c',code,img,'e',f]);d=B.timed([exe,'-I','-c',code,img,'d',f]);return e,d,f.stat().st_size
    return run
def flif(img):
    FL=R/'vendor/FLIF/src/flif'
    with tempfile.TemporaryDirectory() as t:
        f=Path(t)/'a.flif';o=Path(t)/'b.pgm';env={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')}
        e=B.timed([FL,'-e','-o','-E60',img,f],env);d=B.timed([FL,'-d','-o',f,o],env)
        assert B.quick.pgm_pixels(o)==B.quick.pgm_pixels(img);return e,d,f.stat().st_size
out=V/'results/serial_extra_cpu.json';rows=json.loads(out.read_text()) if out.exists() else [];done={(r['image'],r['method']) for r in rows}
for iid,img in B.imgs.items():
    for m,fn in [('png9',py('png9')),('j2k',py('j2k')),('webp_m6',py('webp_m6')),('jxl_e9',py('jxl_e9')),('flif_default',flif)]:
        if (iid,m) in done:continue
        fn(img);runs=[fn(img) for _ in range(3)]
        rows.append(dict(image=iid,method=m,bytes=runs[0][2],enc_s=st.median(r[0]['cpu'] for r in runs),dec_s=st.median(r[1]['cpu'] for r in runs),enc_rss_mb=max(r[0]['rss'] for r in runs)))
        print(rows[-1],flush=True);out.write_text(json.dumps(rows,indent=1))
