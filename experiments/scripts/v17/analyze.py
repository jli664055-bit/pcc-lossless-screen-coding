"""v17 消融与前序基线（v11 记录、v12 跨对比度）的配对分析。"""
import json,csv,statistics as st
from pathlib import Path
V=Path(__file__).resolve().parents[1];R=V.parent
cfg=json.loads((V/'configs/ablation.json').read_text());dev=set(cfg['dev_images'])
base={}
for f in [R/'v11_真实截图与强基线_2026-10-18/results/analysis/records.csv',R/'v12_多场景在线预测研究_2026-10-19/results/contrast32/records.csv']:
    for r in csv.DictReader(open(f)):
        if r['method'] in('flif_default','flif_matched','jxl_e9','webp_m6','paq8px_8'):base.setdefault(r['id'],{})[r['method']]=int(r['bytes'])
res={}
for f in sorted(p for p in (V/'results').glob('*.json') if p.stem in cfg['configs']):
    d=json.loads(f.read_text());res[d['config']]={r['id']:r for r in d['rows']}
order=[c for c in cfg['configs'] if c in res]
def paired(a,b):
    s=[(b[k]-a[k])/b[k]*100 for k in a if k in b]
    if not s:return None
    return dict(n=len(s),median=st.median(s),total=(sum(b[k] for k in a if k in b)-sum(a[k] for k in a if k in b))/sum(b[k] for k in a if k in b)*100,wins=sum(x>0 for x in s),losses=sum(x<0 for x in s))
groups={'All104':lambda r:r['dataset']!='contrast32','Heldout98':lambda r:r['dataset']!='contrast32' and r['id'] not in dev,
        'SCID':lambda r:r['dataset']=='SCID','SIQAD':lambda r:r['dataset']=='SIQAD','Kodak':lambda r:r['dataset']=='Kodak','Real20':lambda r:r['dataset']=='Real20','Contrast32':lambda r:r['dataset']=='contrast32'}
rows=[]
for c in order:
    for g,fn in groups.items():
        a={k:r['bytes'] for k,r in res[c].items() if fn(r) and r.get('lossless')}
        if not a:continue
        refs={'v12(A0)':{k:r['bytes'] for k,r in res.get('A0_v12',{}).items()}}
        for m in ['flif_matched','webp_m6','jxl_e9','paq8px_8']:refs[m]={k:v[m] for k,v in base.items() if m in v}
        for rn,b in refs.items():
            p=paired(a,b)
            if p:rows.append(dict(config=c,group=g,reference=rn,**{k:(round(v,3) if isinstance(v,float) else v) for k,v in p.items()}))
out=V/'results/summary.csv'
with open(out,'w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
lossless={c:(sum(1 for r in res[c].values() if r.get('lossless')),len(res[c])) for c in order}
print('lossless (ok/total):',lossless)
import sys
want=sys.argv[1:] or ['All104','Heldout98','Contrast32']
for r in rows:
    if r['group'] in want:print(f"{r['config']:14s} {r['group']:10s} vs {r['reference']:12s} n={r['n']:3d} median={r['median']:8.3f}% total={r['total']:8.3f}% W/L={r['wins']}/{r['losses']}")
