"""5 张图像上 v18_final、v17（关闭极性规范化）与已有算法的串行时间与内存；快速编解码器预热 1 次、重复 3 次，paq8px 运行 1 次。每个输出均解码比对。"""
import json,os,re,subprocess,tempfile,shutil,statistics as st,sys,time
from pathlib import Path
V=Path(__file__).resolve().parents[1];R=V.parent
sys.path.insert(0,str(Path(__file__).parent));import quick
cfg=json.loads((V/'configs/v18_configs.json').read_text())
IC=str(R/'v3_归一化条件分布熵编码_2026-10-18/runtime/imagecodecs')
P11=R/'v11_真实截图与强基线_2026-10-18/data'
imgs={'SCID_SCI01':P11/'public84/SCID_SCI01.pgm','SIQAD_cim10':P11/'public84/SIQAD_cim10.pgm','Kodak_kodim01':P11/'public84/Kodak_kodim01.pgm','real01':P11/'real20/real01.pgm',
      'MixBand_SCI02':V/'data/MixBand/band_SCID_SCI02.pgm'}
def timed(cmd,env=None,cwd=None):
    with tempfile.NamedTemporaryFile('r',suffix='.t') as tf:
        r=subprocess.run(['/usr/bin/time','-lp','-o',tf.name,*map(str,cmd)],env=env,cwd=cwd,capture_output=True);assert r.returncode==0,r.stderr[-400:]
        o=open(tf.name).read()
    g=lambda k:float(re.search(rf'^{k}\s+([\d.]+)',o,re.M).group(1))
    return dict(wall=g('real'),cpu=g('user')+g('sys'),rss=int(re.search(r'(\d+)\s+maximum resident',o).group(1))/1e6)
def ours(name):
    env={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')};env.update(cfg['base_env']);env.update(cfg['configs'][name])
    def run(img):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/'a.flif';o=Path(t)/'b.pgm'
            e=timed([V/cfg['binary'],'-e','-N','-E60','-P0','-C',*cfg['flags'],img,f],env);d=timed([V/cfg['binary'],'-d',f,o],env)
            assert quick.pgm_pixels(o)==quick.pgm_pixels(img);return e,d,f.stat().st_size+8
    return run
def pycodec(method):
    code=f'''import sys;sys.path.insert(0,{IC!r});import imagecodecs as ic,numpy as np;from PIL import Image
x=np.array(Image.open(sys.argv[1]));mode=sys.argv[2];out=sys.argv[3]
if mode=='e':
  b={{'jxl_e10':lambda:ic.jpegxl_encode(x,lossless=True,effort=10,usecontainer=True,numthreads=1),'jpegls':lambda:ic.jpegls_encode(x,level=0)}}['{method}']();open(out,'wb').write(b)
else:
  b=open(out,'rb').read();y={{'jxl_e10':lambda:ic.jpegxl_decode(b,numthreads=1),'jpegls':lambda:ic.jpegls_decode(b)}}['{method}']();assert np.array_equal(np.asarray(y).reshape(x.shape),x)'''
    def run(img):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/'a.bin';py=str(R.parent/'.venv/bin/python')
            e=timed([py,'-I','-c',code,img,'e',f]);d=timed([py,'-I','-c',code,img,'d',f]);return e,d,f.stat().st_size
    return run
def paq(img):
    PAQ=R/'v11_真实截图与强基线_2026-10-18/vendor/paq8px/build_mac/paq8px'
    with tempfile.TemporaryDirectory() as t:
        shutil.copy(img,Path(t)/'input.pgm');(Path(t)/'restore').mkdir()
        e=timed([PAQ,'-8','input.pgm','image.paq8px216'],cwd=t);d=timed([PAQ,'-d','image.paq8px216','restore/'],cwd=t)
        o=[p for p in (Path(t)/'restore').rglob('*') if p.is_file()][0];assert o.read_bytes()==Path(img).read_bytes()
        return e,d,(Path(t)/'image.paq8px216').stat().st_size
if __name__=='__main__':
    methods={'v18_final':(ours('v18_final'),3),'v17_noPCC':(ours('abl_noPCC_v17'),3),'jxl_e10':(pycodec('jxl_e10'),3),'jpegls':(pycodec('jpegls'),3),'paq8px_8':(paq,1)}
    out=V/'results/serial_v18_cpu.json'
    rows=json.loads(out.read_text()) if out.exists() else []
    done={(r['image'],r['method']) for r in rows}
    for iid,img in imgs.items():
        for m,(fn,reps) in methods.items():
            if (iid,m) in done:continue   # resume
            if reps>1:fn(img)
            runs=[fn(img) for _ in range(reps)]
            rows.append(dict(image=iid,method=m,repeats=reps,bytes=runs[0][2],enc_s=st.median(r[0]['cpu'] for r in runs),dec_s=st.median(r[1]['cpu'] for r in runs),enc_wall=st.median(r[0]['wall'] for r in runs),dec_wall=st.median(r[1]['wall'] for r in runs),
                             enc_rss_mb=max(r[0]['rss'] for r in runs),dec_rss_mb=max(r[1]['rss'] for r in runs)))
            print(rows[-1],flush=True);out.write_text(json.dumps(rows,indent=1))
