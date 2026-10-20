"""在 v10 的 1000 组 × 4 条件合成文字集上运行指定配置，可恢复。用法：run_synth.py 配置 [组数上限]"""
import json,sys,csv
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path(__file__).parent));import quick
V=Path(__file__).resolve().parents[1];R=V.parent;S=R/'v10_逐专家可靠性学习_2026-10-18/新增独立内容1000'
cfg=json.loads((V/'configs/ablation.json').read_text())
name=sys.argv[1];lim=int(sys.argv[2]) if len(sys.argv)>2 else 1000;fin=name in cfg.get('final_configs',[]);binary=str(V/(cfg['final_binary'] if fin else cfg['binary']));flags=tuple(cfg['final_flags']) if fin else ()
imgs=[(f'{c}/expert_independent_{g:04d}',str(S/'data'/c/f'expert_independent_{g:04d}.pgm')) for g in range(0,1000,1000//lim) for c in ['uniform','moderate','low_contrast','polarity_mixed']]
out=V/'results'/f'synth_{name}.json';done={}
if out.exists():done={r['id']:r for r in json.loads(out.read_text())['rows']}
env=dict(cfg['common']);env.update(cfg['configs'][name])
todo=[i for i in imgs if i[0] not in done]
def job(i):r=quick.run(binary,i[1],env,True,flags);r['id']=i[0];return r
with ThreadPoolExecutor(9) as ex:
    for k,r in enumerate(ex.map(job,todo)):
        done[r['id']]=r
        if k%50==49 or k==len(todo)-1:out.write_text(json.dumps({'config':name,'env':env,'binary':binary,'rows':list(done.values())}))
print(name,len(done),'not lossless',sum(1 for r in done.values() if not r.get('lossless')))
