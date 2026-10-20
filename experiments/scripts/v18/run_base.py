"""在 v18 清单上运行已有算法基线，每个新输出均解码比对；可复用 v11、v12、v10 中同参数记录，字段 source 标明来源。用法：run_base.py 方法 数据集[,数据集...] [--subset synth400] [--workers N] [--nodecode]"""
import json,sys,os,subprocess,tempfile,time,argparse,csv,shutil
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor
import numpy as np
from PIL import Image
V=Path(__file__).resolve().parents[1];R=V.parent
sys.path.insert(0,str(R/'v3_归一化条件分布熵编码_2026-10-18/runtime/imagecodecs'));import imagecodecs as ic
FLIF=R/'vendor/FLIF/src/flif';PAQ=R/'v11_真实截图与强基线_2026-10-18/vendor/paq8px/build_mac/paq8px'
REUSE={'flif_default':['flif_default','flif_author_default'],'flif_matched':['flif_matched','flif_author_matched'],'webp_m6':['webp_m6','webp_author_lossless'],'jxl_e9':['jxl_e9','jpegxl_e9'],'paq8px_8':['paq8px_8']}
def reuse_table(method):
    t={}
    if method not in REUSE:return t
    for f,ds in [(R/'v11_真实截图与强基线_2026-10-18/results/analysis/records.csv',None),(R/'v12_多场景在线预测研究_2026-10-19/results/contrast32/records.csv','Contrast32')]:
        for r in csv.DictReader(open(f)):
            if r['method'] in REUSE[method] and r['lossless']=='True':t[(ds or r['dataset'],r['id'])]=(int(r['bytes']),f.parent.name)
    for r in csv.DictReader(open(R/'v10_逐专家可靠性学习_2026-10-18/新增独立内容1000/results/records.csv')):
        if r['method'] in REUSE[method] and r['lossless']=='True':t[('Synth',f"{r['condition']}/{r['id']}")]=(int(r['bytes']),'v10_新增独立内容1000')
    return t
def enc_inproc(method,x):
    if method=='png9':return ic.png_encode(x,level=9),ic.png_decode
    if method=='jpegls':return ic.jpegls_encode(x,level=0),ic.jpegls_decode
    if method=='j2k':return ic.jpeg2k_encode(x,level=0,reversible=True,codecformat='j2k'),ic.jpeg2k_decode
    if method=='jxl_e9':return ic.jpegxl_encode(x,lossless=True,effort=9,usecontainer=True,numthreads=1),lambda b:ic.jpegxl_decode(b,numthreads=1)
    if method=='jxl_e10':return ic.jpegxl_encode(x,lossless=True,effort=10,usecontainer=True,numthreads=1),lambda b:ic.jpegxl_decode(b,numthreads=1)
    if method=='webp_m6':
        import io;bio=io.BytesIO();Image.fromarray(x).save(bio,format='WEBP',lossless=True,quality=100,method=6,exact=True)
        return bio.getvalue(),lambda b:np.array(Image.open(__import__('io').BytesIO(b)).convert('L'))
    raise ValueError(method)
def job(arg):
    method,r,decode=arg;x=np.array(Image.open(r['path']));t0=time.perf_counter()
    if method in ('flif_default','flif_matched'):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/'a.flif';o=Path(t)/'b.pgm';opts=['-E60'] if method=='flif_default' else ['-N','-E60','-P0','-C']
            env={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')}
            subprocess.run([str(FLIF),'-e','-o',*opts,r['path'],str(f)],env=env,check=True,capture_output=True);n=f.stat().st_size;te=time.perf_counter()-t0
            ok=None
            if decode:subprocess.run([str(FLIF),'-d','-o',str(f),str(o)],env=env,check=True,capture_output=True);ok=bool(np.array_equal(np.array(Image.open(o)),x))
    elif method=='paq8px_8':
        with tempfile.TemporaryDirectory() as t:
            shutil.copy(r['path'],Path(t)/'input.pgm');(Path(t)/'restore').mkdir()
            subprocess.run([str(PAQ),'-8','input.pgm','image.paq8px216'],cwd=t,check=True,capture_output=True);n=(Path(t)/'image.paq8px216').stat().st_size;te=time.perf_counter()-t0;ok=None
            if decode:
                subprocess.run([str(PAQ),'-d','image.paq8px216','restore/'],cwd=t,check=True,capture_output=True)
                outf=[p for p in (Path(t)/'restore').rglob('*') if p.is_file()][0];ok=outf.read_bytes()==Path(r['path']).read_bytes()
    else:
        b,dec=enc_inproc(method,x);n=len(b);te=time.perf_counter()-t0
        ok=bool(np.array_equal(np.asarray(dec(b)).reshape(x.shape),x)) if decode else None
    return dict(id=r['id'],dataset=r['dataset'],bytes=n,lossless=ok,enc_s=te,source='v18_run')
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('method');a.add_argument('datasets');a.add_argument('--subset',default='');a.add_argument('--workers',type=int,default=6);a.add_argument('--nodecode',action='store_true');x=a.parse_args()
    man=[r for r in json.loads((V/'configs/manifest_all.json').read_text()) if r['dataset'] in x.datasets.split(',')]
    if x.subset=='synth400':man=[r for r in man if r['dataset']!='Synth' or r['group']%10==0]
    out=V/'results'/f'base_{x.method}.json';done={}
    if out.exists():done={(r['dataset'],r['id']):r for r in json.loads(out.read_text())['rows']}
    reuse=reuse_table(x.method)
    for r in man:
        k=(r['dataset'],r['id'])
        if k not in done and k in reuse:done[k]=dict(id=r['id'],dataset=r['dataset'],bytes=reuse[k][0],lossless=True,source=reuse[k][1])
    todo=[(x.method,r,not x.nodecode) for r in man if (r['dataset'],r['id']) not in done]
    print(x.method,'reused',sum(1 for r in man if (r['dataset'],r['id']) in done),'to run',len(todo),flush=True)
    with ProcessPoolExecutor(x.workers) as ex:
        for k,res in enumerate(ex.map(job,todo,chunksize=1)):
            done[(res['dataset'],res['id'])]=res
            if k%20==19 or k==len(todo)-1:out.write_text(json.dumps({'method':x.method,'rows':list(done.values())}))
    out.write_text(json.dumps({'method':x.method,'rows':list(done.values())}))
    bad=[r for r in done.values() if r.get('lossless') is False]
    print(x.method,'n=',len(done),'failed lossless:',len(bad),flush=True)
