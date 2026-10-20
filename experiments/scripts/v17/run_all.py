"""在 104 张图像（另加跨对比度 32 张）上运行全部消融配置并校验无损，可恢复。"""
import json,sys,os
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path(__file__).parent));import quick
V=Path(__file__).resolve().parents[1];R=V.parent
cfg=json.loads((V/'configs/ablation.json').read_text())
imgs=[]
for e in json.loads((R/'v12_多场景在线预测研究_2026-10-19/configs/integer104.json').read_text())['images']:
    imgs.append((e['id'],e['dataset'],str(R/'v11_真实截图与强基线_2026-10-18'/e['path'])))
for f in sorted((R/'v12_多场景在线预测研究_2026-10-19/data/contrast32').glob('*.pgm')):imgs.append((f.stem,'contrast32',str(f)))
only=sys.argv[1:] or list(cfg['configs'])
for name in only:
    out=V/'results'/f'{name}.json';done={}
    if out.exists():done={r['id']:r for r in json.loads(out.read_text())['rows']}
    env=dict(cfg['common']);env.update(cfg['configs'][name])
    fin=name in cfg.get('final_configs',[]);binary=cfg['final_binary'] if fin else cfg['binary'];flags=tuple(cfg['final_flags']) if fin else ()
    todo=[i for i in imgs if i[0] not in done]
    def job(i):
        r=quick.run(str(V/binary),i[2],env,True,flags);r.update(id=i[0],dataset=i[1]);return r
    with ThreadPoolExecutor(8) as ex:
        for k,r in enumerate(ex.map(job,todo)):
            done[r['id']]=r
            if k%8==7 or k==len(todo)-1:out.write_text(json.dumps({'config':name,'env':env,'rows':list(done.values())},indent=1))
    bad=[r for r in done.values() if not r.get('lossless')]
    print(name,'n=',len(done),'total=',sum(r.get('bytes',0) for r in done.values()),'NOT LOSSLESS:',len(bad),flush=True)
