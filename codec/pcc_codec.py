"""研究编码器的命令行编解码工具（仅支持 8 位灰度）。

用法：
  python codec/pcc_codec.py encode 输入.png 输出.pcc [--config v18|v17|v12]
  python codec/pcc_codec.py decode 输出.pcc 还原.pgm

文件头 8 字节标识配置，其后为 FLIF 码流。三个版本由环境变量切换：v17 关闭极性
规范化，v12 为前序串联编码器。优先使用 codec/flif_pcc/src/flif，未编译时回退到
bin/flif_v18_final。
"""
import argparse,json,os,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
_BUILT=ROOT/'codec/flif_pcc/src/flif'
_PREBUILT=ROOT/'bin/flif_v18_final'
BIN=_BUILT if _BUILT.exists() else _PREBUILT
CFG=json.loads((ROOT/'experiments/configs/v18_configs.json').read_text())
PROFILES={'v18':(dict(CFG['base_env'],**CFG['configs']['v18_final']),['-R0']),
          'v17':(dict(CFG['base_env'],**CFG['configs']['abl_noPCC_v17']),['-R0']),
          'v12':({k:v for k,v in CFG['base_env'].items()}|{'SMEM_LMIX':'0'},[])}
MAGIC={'v18':b'PCC18\0\0\0','v17':b'PCC17\0\0\0','v12':b'PCC12\0\0\0'}
def env_for(name):
    e={k:v for k,v in os.environ.items() if not k.startswith('SMEM_')};e.update(PROFILES[name][0]);return e
def run(args,name):
    r=subprocess.run([str(BIN),*map(str,args)],env=env_for(name),capture_output=True)
    if r.returncode:sys.exit(r.stderr.decode(errors='replace'))
def encode(src,dst,name):
    from PIL import Image
    with tempfile.TemporaryDirectory() as t:
        pgm=Path(t)/'in.pgm';im=Image.open(src)
        if im.mode!='L':raise SystemExit('only 8-bit grayscale input is supported (convert with Pillow .convert("L") first)')
        im.save(pgm);payload=Path(t)/'a.flif'
        run(['-e','-N','-E60','-P0','-C',*PROFILES[name][1],pgm,payload],name)
        Path(dst).write_bytes(MAGIC[name]+payload.read_bytes())
def decode(src,dst):
    raw=Path(src).read_bytes();name=next((k for k,v in MAGIC.items() if raw[:8]==v),None)
    if name is None:raise SystemExit('unknown header')
    with tempfile.TemporaryDirectory() as t:
        payload=Path(t)/'a.flif';payload.write_bytes(raw[8:]);run(['-d',payload,dst],name)
if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    a.add_argument('op',choices=['encode','decode']);a.add_argument('src');a.add_argument('dst');a.add_argument('--config',choices=list(PROFILES),default='v18')
    x=a.parse_args()
    if not BIN.exists():raise SystemExit('binary not found: run codec/build.sh or ship bin/flif_v18_final')
    encode(x.src,x.dst,x.config) if x.op=='encode' else decode(x.src,x.dst)
