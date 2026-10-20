"""单图快速配对测试：按给定环境编码并解码，校验无损并输出字节数。"""
import subprocess,sys,os,tempfile,hashlib,json,argparse,time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
BASE_ENV={'SMEM_SOFT_CONTEXT':'1','SMEM_GEOMETRY_PALETTE':'1','SMEM_TRANSPORT_CELL':'1','SMEM_EXACT_CELL':'1','SMEM_RESIDUAL_MIX':'1','SMEM_EXPERT_LEARNING':'2','SMEM_ONLINE_PREDICTOR':'5'}
def pgm_pixels(path):
    d=Path(path).read_bytes();assert d[:2]==b'P5'
    parts=[];i=2
    while len(parts)<3:
        while d[i:i+1].isspace():i+=1
        if d[i:i+1]==b'#':
            while d[i:i+1]!=b'\n':i+=1
            continue
        j=i
        while not d[j:j+1].isspace():j+=1
        parts.append(int(d[i:j]));i=j
    return d[i+1:]
def run(binary,img,extra,decode=True,flags=()):
    env={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')};env.update(BASE_ENV);env.update(extra)
    with tempfile.TemporaryDirectory() as t:
        f=Path(t)/'a.flif';o=Path(t)/'b.pgm'
        t0=time.perf_counter();r=subprocess.run([binary,'-e','-N','-E60','-P0','-C',*flags,img,str(f)],env=env,capture_output=True);te=time.perf_counter()-t0
        if r.returncode:return {'error':r.stderr.decode()[-300:]}
        n=f.stat().st_size+8;ok=None;td=None
        if decode:
            t0=time.perf_counter();r=subprocess.run([binary,'-d',str(f),str(o)],env=env,capture_output=True);td=time.perf_counter()-t0
            ok=(r.returncode==0 and pgm_pixels(o)==pgm_pixels(img))
        return {'bytes':n,'lossless':ok,'enc_s':te,'dec_s':td}
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('binary');a.add_argument('--env',action='append',default=[]);a.add_argument('--nodecode',action='store_true');a.add_argument('--jobs',type=int,default=8);a.add_argument('--out');a.add_argument('--flags',default='');a.add_argument('images',nargs='+');x=a.parse_args()
    extra=dict(e.split('=',1) for e in x.env)
    with ThreadPoolExecutor(x.jobs) as ex:res=list(ex.map(lambda i:run(x.binary,i,extra,not x.nodecode,tuple(x.flags.split())),x.images))
    rows=[]
    for i,r in zip(x.images,res):
        rows.append({'image':Path(i).stem,**r});print(Path(i).stem,r.get('bytes'),r.get('lossless'),r.get('error',''),flush=True)
    print('TOTAL',sum(r.get('bytes',0) for r in res))
    if x.out:Path(x.out).write_text(json.dumps({'binary':x.binary,'env':extra,'rows':rows},indent=1))
