"""合成文字分组统计：按条件、按字族给出中位节省与组自助置信区间（2000 次，种子 261018）。"""
import json,csv,statistics as st,random
from pathlib import Path
V=Path(__file__).resolve().parents[1];R=V.parent;S=R/'v10_逐专家可靠性学习_2026-10-18/新增独立内容1000'
base={};fam={}
for r in csv.DictReader(open(S/'results/records.csv')):
    base.setdefault(r['method'],{})[f"{r['condition']}/{r['id']}"]=int(r['bytes']);fam[r['id']]=r['family']
G={r['id']:r['bytes'] for r in json.loads((V/'results/synth_G_final.json').read_text())['rows']}
A0={r['id']:r['bytes'] for r in json.loads((V/'results/synth_A0_v12.json').read_text())['rows']}
refs={'FLIF匹配':base['flif_author_matched'],'FLIF默认':base['flif_author_default'],'WebP':base['webp_author_lossless'],'JPEG XL e9':base['jpegxl_e9'],'v10':base['v10_regime'],'v12':A0}
conds=['uniform','moderate','low_contrast','polarity_mixed']
groups=sorted({k.split('/')[1] for k in G})
def sav(a,b):return (b-a)/b*100
out={}
rnd=random.Random(261018)
for name,b in refs.items():
    gs=[sav(sum(G[f'{c}/{g}'] for c in conds),sum(b[f'{c}/{g}'] for c in conds)) for g in groups]
    boots=sorted(st.median(rnd.choices(gs,k=len(gs))) for _ in range(2000))
    row={'all':(st.median(gs),boots[49],boots[1949])}
    for c in conds:row[c]=st.median(sav(G[f'{c}/{g}'],b[f'{c}/{g}']) for g in groups)
    for f in ['chinese','english','mixed_language','tabular_code']:
        fg=[g for g in groups if fam[g]==f]
        if fg:row[f]=st.median(sav(sum(G[f'{c}/{g}'] for c in conds),sum(b[f'{c}/{g}'] for c in conds)) for g in fg)
    out[name]=row
    print(name,{k:(tuple(round(x,2) for x in v) if isinstance(v,tuple) else round(v,2)) for k,v in row.items()})
print('families',sorted(set(fam.values())))
(V/'results/synth_breakdown.json').write_text(json.dumps(out,ensure_ascii=False,indent=1))
