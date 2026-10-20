"""按内容组成组（4 种条件求和）计算 v17 相对 v10 记录的中位节省率。"""
import json,csv,sys,statistics as st
from pathlib import Path
V=Path(__file__).resolve().parents[1];R=V.parent;S=R/'v10_逐专家可靠性学习_2026-10-18/新增独立内容1000'
base={}
for r in csv.DictReader(open(S/'results/records.csv')):base.setdefault(r['method'],{})[f"{r['condition']}/{r['id']}"]=int(r['bytes'])
for name in sys.argv[1:]:
    rows={r['id']:r['bytes'] for r in json.loads((V/'results'/f'synth_{name}.json').read_text())['rows'] if r.get('lossless')}
    groups={}
    for k in rows:groups.setdefault(k.split('/')[1],[]).append(k)
    groups={g:ks for g,ks in groups.items() if len(ks)==4}
    print(f'== {name}: {len(groups)} complete groups')
    refs=list(base)+[f'synth:{x}' for x in ['A0_v12'] if (V/'results'/f'synth_{x}.json').exists() and x!=name]
    for m in refs:
        b=base[m] if not m.startswith('synth:') else {r['id']:r['bytes'] for r in json.loads((V/'results'/f'synth_{m[6:]}.json').read_text())['rows']}
        s=[];tb=ta=0
        for g,ks in groups.items():
            if all(k in b for k in ks):
                A=sum(rows[k] for k in ks);B=sum(b[k] for k in ks);s.append((B-A)/B*100);ta+=A;tb+=B
        if s:print(f'  vs {m:22s} n={len(s):4d} median={st.median(s):7.3f}% total={(tb-ta)/tb*100:7.3f}% W/L={sum(x>0 for x in s)}/{sum(x<0 for x in s)}')
