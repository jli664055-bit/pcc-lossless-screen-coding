"""按公开数据集重建实验所用的全部灰度图像，并以 SHA-256 校验。

  python data/scripts/prepare_data.py --scid <SCID/ReferenceSCIs> --siqad <SIQAD/references> \
         --kodak <kodim01.png 至 kodim24.png 所在目录> --div2k <DIV2K_valid_HR> [--real20 <私有目录>]

输出（相对仓库根目录）：
  data/public_gray/{SCID,SIQAD,Kodak}/*.pgm   原分辨率灰度图像
  data/derived/DIV2K100g/*.pgm                DIV2K 验证集 0801 至 0900
  data/derived/contrast32/*.pgm               512×512 裁剪 × 4 种对比度
  data/derived/{MixHalf,MixBand}/*.pgm        混合主题拼接与分带图像
真实截图不随仓库分发，未提供 --real20 时跳过由其构造的样本。
"""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
CK=json.loads((ROOT/'data/checksums/gray_pgm_sha256.json').read_text())
DEV=['SCID_SCI01','SCID_SCI05','SCID_SCI12','SIQAD_cim3','Kodak_kodim03','real02']
OUT=ROOT
def save(rel,arr):
    p=OUT/rel;p.parent.mkdir(parents=True,exist_ok=True);Image.fromarray(arr.astype(np.uint8)).save(p)
    want=CK.get(rel);got=hashlib.sha256(p.read_bytes()).hexdigest()
    return want is None or want==got,rel
def gray(path):return np.array(Image.open(path).convert('L'))
def main():
    a=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    for k in ['scid','siqad','kodak','div2k','real20']:a.add_argument('--'+k,type=Path)
    a.add_argument('--out',type=Path,default=ROOT,help='output root (default: repository root)')
    x=a.parse_args();res=[];src={}
    global OUT;OUT=x.out
    if x.scid:
        for i in range(1,41):g=gray(x.scid/f'SCI{i:02d}.bmp');src[f'SCID_SCI{i:02d}']=g;res.append(save(f'data/public_gray/SCID/SCID_SCI{i:02d}.pgm',g))
    if x.siqad:
        for i in range(1,21):g=gray(x.siqad/f'cim{i}.bmp');src[f'SIQAD_cim{i}']=g;res.append(save(f'data/public_gray/SIQAD/SIQAD_cim{i}.pgm',g))
    if x.kodak:
        for i in range(1,25):g=gray(x.kodak/f'kodim{i:02d}.png');src[f'Kodak_kodim{i:02d}']=g;res.append(save(f'data/public_gray/Kodak/Kodak_kodim{i:02d}.pgm',g))
    if x.real20:
        for i in range(1,21):
            p=x.real20/f'real{i:02d}.pgm'
            if p.exists():src[f'real{i:02d}']=gray(p)
    if x.div2k:
        for i in range(801,901):
            f=next((x.div2k/f'{i:04d}{e}' for e in ('.png','.ppm') if (x.div2k/f'{i:04d}{e}').exists()),None)
            if f:res.append(save(f'data/derived/DIV2K100g/DIV2K_{i:04d}.pgm',gray(f)))
    # 跨对比度：裁剪位置取自原始清单，4 种整数对比度映射
    man=json.loads((ROOT/'experiments/results/earlier/contrast32_manifest.json').read_text())
    for m in man:
        if m['source_group'] not in src:continue
        cx,cy,w,h=m['crop_xywh'];g=src[m['source_group']][cy:cy+h,cx:cx+w].astype(np.int64)
        y={'original':g,'range32_224':32+(g*192+127)//255,'range96_160':96+(g*64+127)//255,'inverted32_224':224-(g*192+127)//255}[m['condition']]
        res.append(save(f"data/derived/contrast32/{m['id']}.pgm",y))
    # 混合主题合成图（非开发源图，同一数据集内相邻成对）
    def half(a,b):H=min(a.shape[0],b.shape[0]);W=min(a.shape[1],b.shape[1]);m=W//2;return np.concatenate([a[:H,:m],255-b[:H,m:W]],axis=1)
    def band(a):
        o=a.copy();H=a.shape[0];e=[round(H*k/4) for k in range(5)]
        for k in (1,3):o[e[k]:e[k+1]]=255-o[e[k]:e[k+1]]
        return o
    for ds,prefix in [('SCID','SCID_'),('SIQAD','SIQAD_'),('Real20','real')]:
        ids=sorted([k for k in src if k.startswith(prefix) and k not in DEV])
        if ds=='SIQAD':ids=sorted(ids)
        for k,i in enumerate(ids):
            j=ids[(k+1)%len(ids)]
            res.append(save(f'data/derived/MixHalf/half_{i}__{j}.pgm',half(src[i],src[j])));res.append(save(f'data/derived/MixBand/band_{i}.pgm',band(src[i])))
    bad=[r for ok,r in res if not ok]
    print(f'written {len(res)} images, checksum mismatches: {len(bad)}');[print('  mismatch',r) for r in bad[:10]]
if __name__=='__main__':main()
