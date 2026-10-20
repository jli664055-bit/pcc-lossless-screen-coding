"""v18 分析：本文相对已有算法（按数据集）、合成文字成组配对、极性规范化消融拆分。"""
import json,statistics as st,random,csv,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
V=ROOT/'experiments'
def load(prefix,name):
    f=V/'results'/f'{prefix}_{name}.json'
    if not f.exists():return {}
    return {(r['dataset'],r['id']):r['bytes'] for r in json.loads(f.read_text())['rows'] if r.get('lossless') is not False and 'bytes' in r}
BASES=['png9','jpegls','j2k','flif_default','flif_matched','webp_m6','jxl_e9','jxl_e10','paq8px_8']
OURS=['v18_final','abl_noPCC_v17','abl_capacity','abl_hard','abl_nonorm']
ours={n:load('ours',n) for n in OURS};base={b:load('base',b) for b in BASES}
DS=['SCID','SIQAD','Kodak','Real20','Contrast32','MixHalf','MixBand','DIV2K100g']
def paired(a,b,keys):
    ks=[k for k in keys if k in a and k in b]
    if not ks:return None
    s=[(b[k]-a[k])/b[k]*100 for k in ks];A=sum(a[k] for k in ks);B=sum(b[k] for k in ks)
    return dict(n=len(ks),median=st.median(s),total=(B-A)/B*100,wins=sum(x>0 for x in s),losses=sum(x<0 for x in s))
def group_paired(a,b,keys,rng=None):
    g={}
    for k in keys:
        if k in a and k in b:g.setdefault(k[1].split('/')[1],[]).append(k)
    g={kk:v for kk,v in g.items() if len(v)==4}
    if not g:return None
    s=[(sum(b[k] for k in v)-sum(a[k] for k in v))/sum(b[k] for k in v)*100 for v in g.values()]
    res=dict(n=len(s),median=st.median(s),wins=sum(x>0 for x in s),losses=sum(x<0 for x in s))
    if rng:bs=sorted(st.median(rng.choices(s,k=len(s))) for _ in range(2000));res.update(lo=bs[49],hi=bs[1949])
    return res
rows=[]
a=ours['v18_final']
for d in DS+['Synth']:
    keys=[k for k in a if k[0]==d]
    for b in BASES+['abl_noPCC_v17']:
        ref=base[b] if b in base else ours.get(b,{})
        p=group_paired(a,ref,keys,random.Random(261018)) if d=='Synth' else paired(a,ref,keys)
        if p:rows.append(dict(dataset=d,reference=b,**{k:(round(v,6) if isinstance(v,float) else v) for k,v in p.items()}))
# 公开与真实图合并（SCID、SIQAD、Kodak、Real20）
keys=[k for k in a if k[0] in ('SCID','SIQAD','Kodak','Real20')]
for b in BASES:
    p=paired(a,base[b],keys)
    if p:rows.append(dict(dataset='Public104',reference=b,**{k:(round(v,6) if isinstance(v,float) else v) for k,v in p.items()}))
with open(V/'results/v18_vs_existing.csv','w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=['dataset','reference','n','median','total','wins','losses','lo','hi']);w.writeheader();w.writerows(rows)
# 消融：v18 相对各消融版本（按数据集）
abl=[]
for n in OURS[1:]:
    for d in DS+['Synth','dev_mix']:
        keys=[k for k in a if k[0]==d]
        p=paired(a,ours[n],keys)
        if p:abl.append(dict(dataset=d,ablation=n,**{k:(round(v,6) if isinstance(v,float) else v) for k,v in p.items()}))
with open(V/'results/v18_ablation.csv','w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=['dataset','ablation','n','median','total','wins','losses']);w.writeheader();w.writerows(abl)
show=sys.argv[1:] or ['jxl_e10','paq8px_8','abl_noPCC_v17']
for r in rows:
    if r['reference'] in show:print(f"{r['dataset']:11s} vs {r['reference']:14s} n={r['n']:4d} median={r['median']:8.3f}% W/L={r['wins']}/{r['losses']}")
print('--- ablations (positive = v18 smaller)')
for r in abl:print(f"{r['dataset']:11s} {r['ablation']:14s} n={r['n']:4d} median={r['median']:7.3f}% total={r['total']:7.3f}% W/L={r['wins']}/{r['losses']}")
