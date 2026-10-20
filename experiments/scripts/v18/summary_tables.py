"""汇总各数据集平均比特率与本文相对已有算法的中位节省率矩阵。"""
import json,csv,statistics as st
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[3]
V=ROOT/'experiments'
man={(r['dataset'],r['id']):r for r in json.loads((V/'configs/manifest_all_relative.json').read_text())}
px={}
def pixels(k):
    if k not in px:
        w,h=Image.open(man[k]['path']).size;px[k]=w*h
    return px[k]
M={'v18_final':'ours','abl_noPCC_v17':'ours','png9':'base','jpegls':'base','j2k':'base','flif_default':'base','flif_matched':'base','webp_m6':'base','jxl_e9':'base','jxl_e10':'base','paq8px_8':'base'}
data={m:{(r['dataset'],r['id']):r['bytes'] for r in json.loads((V/'results'/f'{p}_{m}.json').read_text())['rows'] if r.get('lossless') is not False and 'bytes' in r} for m,p in M.items()}
DS=['SCID','SIQAD','Real20','Kodak','Contrast32','MixHalf','MixBand','DIV2K100g','Synth']
bpp={}
for d in DS:
    keys=[k for k in data['v18_final'] if k[0]==d]
    bpp[d]={}
    for m in M:
        ks=[k for k in keys if k in data[m]]
        if len(ks)==len(keys):bpp[d][m]=sum(data[m][k]*8/pixels(k) for k in ks)/len(ks)
        elif ks:bpp[d][m+f'(n={len(ks)})']=sum(data[m][k]*8/pixels(k) for k in ks)/len(ks)
(V/'results/bpp_v18.json').write_text(json.dumps(bpp,indent=1))
for d in DS:print(d,{k:round(v,3) for k,v in bpp[d].items()})
