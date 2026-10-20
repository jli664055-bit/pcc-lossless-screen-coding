"""在指定数据集上运行一个 v18 配置，每个输出均解码比对，可恢复。用法：run_ours.py 配置 数据集[,数据集...] [--subset synth400] [--jobs N]"""
import json,sys,os,subprocess,tempfile,time,argparse
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path(__file__).parent));import quick
ROOT=Path(__file__).resolve().parents[3]
V=ROOT/'experiments'
cfg=json.loads((V/'configs/v18_configs.json').read_text())
a=argparse.ArgumentParser();a.add_argument('config');a.add_argument('datasets');a.add_argument('--subset',default='');a.add_argument('--jobs',type=int,default=6);x=a.parse_args()
man=[r for r in json.loads((V/'configs/manifest_all_relative.json').read_text()) if r['dataset'] in x.datasets.split(',')]
if x.subset=='synth400':man=[r for r in man if r['dataset']!='Synth' or r['group']%10==0]
env={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')};env.update(cfg['base_env']);env.update(cfg['configs'][x.config])
binary=str(ROOT/cfg['binary']);flags=cfg['flags']
out=V/'results'/f'ours_{x.config}.json';done={}
if out.exists():done={(r['dataset'],r['id']):r for r in json.loads(out.read_text())['rows']}
todo=[r for r in man if (r['dataset'],r['id']) not in done]
def job(r):
    with tempfile.TemporaryDirectory() as t:
        f=Path(t)/'a.flif';o=Path(t)/'b.pgm'
        t0=time.perf_counter();p=subprocess.run([binary,'-e','-N','-E60','-P0','-C',*flags,str(ROOT/r['path']),str(f)],env=env,capture_output=True);te=time.perf_counter()-t0
        if p.returncode:return dict(id=r['id'],dataset=r['dataset'],error=p.stderr.decode()[-300:])
        t0=time.perf_counter();q=subprocess.run([binary,'-d',str(f),str(o)],env=env,capture_output=True);td=time.perf_counter()-t0
        ok=q.returncode==0 and quick.pgm_pixels(o)==quick.pgm_pixels(ROOT/r['path'])
        return dict(id=r['id'],dataset=r['dataset'],bytes=f.stat().st_size+8,lossless=ok,enc_s=te,dec_s=td)
with ThreadPoolExecutor(x.jobs) as ex:
    for k,res in enumerate(ex.map(job,todo)):
        done[(res['dataset'],res['id'])]=res
        if k%25==24 or k==len(todo)-1:out.write_text(json.dumps({'config':x.config,'env':cfg['configs'][x.config],'rows':list(done.values())}))
bad=[r for r in done.values() if not r.get('lossless')]
print(x.config,x.datasets,'n=',len(done),'not lossless/errors:',len(bad),flush=True)
