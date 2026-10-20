"""按固定规则生成混合主题图像（确定性）。

拼接：A 图左半与 255−B 图右半（A、B 为同一数据集的相邻源图，循环取用）。
分带：A 图均分为 4 条横带，第 2、4 条反色。
开发集混合图仅由 6 张开发图构造，留出集仅使用非开发源图。
"""
import json,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
V=Path(__file__).resolve().parents[1];R=V.parent;D11=R/'v11_真实截图与强基线_2026-10-18'
DEV=['SCID_SCI01','SCID_SCI05','SCID_SCI12','SIQAD_cim3','Kodak_kodim03','real02']
imgs=json.loads((R/'v12_多场景在线预测研究_2026-10-19/configs/integer104.json').read_text())['images']
src={e['id']:(e['dataset'],D11/e['path']) for e in imgs}
load=lambda i:np.array(Image.open(src[i][1]))
def half(a,b):
    H=min(a.shape[0],b.shape[0]);W=min(a.shape[1],b.shape[1]);m=W//2
    return np.concatenate([a[:H,:m],255-b[:H,m:W]],axis=1)
def band(a):
    out=a.copy();H=a.shape[0];e=[round(H*k/4) for k in range(5)]
    for k in (1,3):out[e[k]:e[k+1]]=255-out[e[k]:e[k+1]]
    return out
man=[]
def save(name,group,arr,sources):
    p=V/'data'/group/f'{name}.pgm';p.parent.mkdir(parents=True,exist_ok=True);Image.fromarray(arr.astype(np.uint8)).save(p)
    man.append(dict(id=name,dataset=group,path=str(p.relative_to(V)),sources=sources,width=arr.shape[1],height=arr.shape[0],pixels=int(arr.size),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
# 开发集混合图（仅用开发图）
for a,b in [('SCID_SCI01','SCID_SCI05'),('SCID_SCI12','SIQAD_cim3'),('real02','SCID_SCI12')]:save(f'devhalf_{a}__{b}','dev_mix',half(load(a),load(b)),[a,b])
for a in ['SCID_SCI05','SIQAD_cim3','real02']:save(f'devband_{a}','dev_mix',band(load(a)),[a])
# 留出集混合图
for ds in ['SCID','SIQAD','Real20']:
    ids=sorted(i for i,(d,_) in src.items() if d==ds and i not in DEV)
    for k,a in enumerate(ids):
        b=ids[(k+1)%len(ids)];save(f'half_{a}__{b}','MixHalf',half(load(a),load(b)),[a,b]);save(f'band_{a}','MixBand',band(load(a)),[a])
# DIV2K 灰度图（Pillow convert('L')，原分辨率）
for p in sorted((R/'v15_独创性核查与大规模冻结验证_2026-10-19/data/DIV2K').glob('*.ppm')):
    save(f'DIV2K_{p.stem}','DIV2K100g',np.array(Image.open(p).convert('L')),[p.name])
(V/'configs/datasets_v18.json').write_text(json.dumps(man,ensure_ascii=False,indent=1))
from collections import Counter;print(Counter(m['dataset'] for m in man))
