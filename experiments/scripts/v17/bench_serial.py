"""串行计时：每个（图像，配置）预热 1 次、重复 3 次，一次运行一个进程，内存由 /usr/bin/time -l 读取。"""
import subprocess,json,os,re,tempfile,statistics as st,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));import quick
V=Path(__file__).resolve().parents[1];R=V.parent;cfg=json.loads((V/'configs/ablation.json').read_text())
imgs={'SCID_SCI01':R/'v11_真实截图与强基线_2026-10-18/data/public84/SCID_SCI01.pgm','SIQAD_cim10':R/'v11_真实截图与强基线_2026-10-18/data/public84/SIQAD_cim10.pgm',
      'Kodak_kodim01':R/'v11_真实截图与强基线_2026-10-18/data/public84/Kodak_kodim01.pgm','real01':R/'v11_真实截图与强基线_2026-10-18/data/real20/real01.pgm'}
def timed(cmd,env):
    r=subprocess.run(['/usr/bin/time','-l',*cmd],env=env,capture_output=True,text=True);assert r.returncode==0,r.stderr[-500:]
    real=float(re.search(r'([\d.]+) real',r.stderr).group(1));rss=int(re.search(r'(\d+)\s+maximum resident',r.stderr).group(1));return real,rss
rows=[]
for name in ['A0_v12','F_full']:
    env={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')};env.update(quick.BASE_ENV);env.update(cfg['common']);env.update(cfg['configs'][name])
    for iid,img in imgs.items():
        enc=[];dec=[]
        for rep in range(4):
            with tempfile.TemporaryDirectory() as t:
                f=Path(t)/'a.flif';o=Path(t)/'b.pgm'
                e=timed([str(V/cfg['binary']),'-e','-N','-E60','-P0','-C',str(img),str(f)],env);d=timed([str(V/cfg['binary']),'-d',str(f),str(o)],env)
                assert quick.pgm_pixels(o)==quick.pgm_pixels(img)
                if rep:enc.append(e);dec.append(d)
        rows.append(dict(config=name,image=iid,enc_s=st.median(x[0] for x in enc),dec_s=st.median(x[0] for x in dec),enc_rss_mb=max(x[1] for x in enc)/1e6,dec_rss_mb=max(x[1] for x in dec)/1e6,bytes=f.stat().st_size if False else None))
        print(rows[-1],flush=True)
(V/'results/serial_timing.json').write_text(json.dumps(rows,indent=1))
