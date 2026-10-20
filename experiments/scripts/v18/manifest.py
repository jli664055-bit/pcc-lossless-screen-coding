"""汇总 v18 评测清单（绝对路径版本）。"""
import json
from pathlib import Path
V=Path(__file__).resolve().parents[1];R=V.parent
rows=[]
for e in json.loads((R/'v12_多场景在线预测研究_2026-10-19/configs/integer104.json').read_text())['images']:
    rows.append(dict(id=e['id'],dataset=e['dataset'],path=str(R/'v11_真实截图与强基线_2026-10-18'/e['path'])))
for f in sorted((R/'v12_多场景在线预测研究_2026-10-19/data/contrast32').glob('*.pgm')):rows.append(dict(id=f.stem,dataset='Contrast32',path=str(f)))
for m in json.loads((V/'configs/datasets_v18.json').read_text()):rows.append(dict(id=m['id'],dataset=m['dataset'],path=str(V/m['path'])))
S=R/'v10_逐专家可靠性学习_2026-10-18/新增独立内容1000/data'
for g in range(1000):
    for c in ['uniform','moderate','low_contrast','polarity_mixed']:
        rows.append(dict(id=f'{c}/expert_independent_{g:04d}',dataset='Synth',path=str(S/c/f'expert_independent_{g:04d}.pgm'),group=g,condition=c))
for r in rows:assert Path(r['path']).exists(),r
(V/'configs/manifest_all.json').write_text(json.dumps(rows,ensure_ascii=False,indent=0))
from collections import Counter;print(Counter(r['dataset'] for r in rows),len(rows))
