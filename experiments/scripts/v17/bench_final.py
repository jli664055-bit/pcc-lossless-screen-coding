"""v12、v17 与 paq8px（−8）在 4 张图像上的串行时间与内存；每个输出均解码比对。"""
import subprocess,json,os,re,tempfile,shutil,statistics as st,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));import quick
V=Path(__file__).resolve().parents[1];R=V.parent;cfg=json.loads((V/'configs/ablation.json').read_text())
PAQ=R/'v11_真实截图与强基线_2026-10-18/vendor/paq8px/build_mac/paq8px'
imgs={'SCID_SCI01':R/'v11_真实截图与强基线_2026-10-18/data/public84/SCID_SCI01.pgm','SIQAD_cim10':R/'v11_真实截图与强基线_2026-10-18/data/public84/SIQAD_cim10.pgm',
      'Kodak_kodim01':R/'v11_真实截图与强基线_2026-10-18/data/public84/Kodak_kodim01.pgm','real01':R/'v11_真实截图与强基线_2026-10-18/data/real20/real01.pgm'}
def timed(cmd,env,cwd=None):
    with tempfile.NamedTemporaryFile('r',suffix='.time') as tf:
        r=subprocess.run(['/usr/bin/time','-lp','-o',tf.name,*map(str,cmd)],env=env,capture_output=True,text=True,cwd=cwd);assert r.returncode==0,r.stderr[-800:]
        out=open(tf.name).read()
    g=lambda k:float(re.search(rf'^{k}\s+([\d.]+)',out,re.M).group(1))
    return dict(wall=g('real'),cpu=g('user')+g('sys'),rss=int(re.search(r'(\d+)\s+maximum resident',out).group(1)))
base={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')}
def flif(binary,envx,flags):
    env=dict(base);env.update(quick.BASE_ENV);env.update(envx)
    def run(img):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/'a.flif';o=Path(t)/'b.pgm'
            e=timed([binary,'-e','-N','-E60','-P0','-C',*flags,img,f],env);d=timed([binary,'-d',f,o],env)
            assert quick.pgm_pixels(o)==quick.pgm_pixels(img);return e,d,f.stat().st_size+8
    return run
def paq(img):
    with tempfile.TemporaryDirectory() as t:
        shutil.copy(img,Path(t)/'input.pgm');(Path(t)/'restore').mkdir()
        e=timed([PAQ,'-8','input.pgm','image.paq8px216'],base,cwd=t);d=timed([PAQ,'-d','image.paq8px216','restore/'],base,cwd=t)
        out=[p for p in (Path(t)/'restore').rglob('*') if p.is_file()][0];assert out.read_bytes()==(Path(t)/'input.pgm').read_bytes()
        return e,d,(Path(t)/'image.paq8px216').stat().st_size
methods={'v12_original':(flif(str(V/'bin/flif_v12repro'),{},()),3),
         'v17_final':(flif(str(V/cfg['final_binary']),{**cfg['common'],**cfg['configs']['G_final']},tuple(cfg['final_flags'])),3),
         'paq8px_8':(paq,1)}
rows=[]
for iid,img in imgs.items():
    for m,(fn,reps) in methods.items():
        if reps>1:fn(img)                      # warm-up
        runs=[fn(img) for _ in range(reps)]
        med=lambda i,k:st.median(r[i][k] for r in runs)
        rows.append(dict(image=iid,method=m,repeats=reps,bytes=runs[0][2],enc_wall=med(0,'wall'),enc_cpu=med(0,'cpu'),dec_wall=med(1,'wall'),dec_cpu=med(1,'cpu'),enc_rss_mb=max(r[0]['rss'] for r in runs)/1e6,dec_rss_mb=max(r[1]['rss'] for r in runs)/1e6))
        print(rows[-1],flush=True)
        (V/'results/serial_final.json').write_text(json.dumps(rows,indent=1))
