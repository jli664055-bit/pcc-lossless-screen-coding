"""由逐图结果重算论文表 6.1。

  python experiments/summarize.py              # 本文相对各已有算法的文件中位节省率（%）
  python experiments/summarize.py --polarity   # 图 1.1 的极性代价
正值表示本文文件更小；合成文字按 4 种条件成组配对。
"""
import json,statistics as st,sys
from pathlib import Path
RES=Path(__file__).resolve().parent/'results/v18'
def load(name):
    return {(r['dataset'],r['id']):r['bytes'] for r in json.loads((RES/f'{name}.json').read_text())['rows'] if r.get('lossless') is not False and 'bytes' in r}
CODECS=[('png9','PNG'),('jpegls','JPEG-LS'),('j2k','JPEG2000'),('flif_default','FLIF'),('webp_m6','WebP'),('jxl_e9','JXL e9'),('jxl_e10','JXL e10'),('paq8px_8','paq8px')]
DS=['SCID','SIQAD','Real20','Kodak','Contrast32','MixHalf','MixBand','DIV2K100g','Synth']
def saving(a,b,ds):
    ks=[k for k in a if k[0]==ds and k in b]
    if not ks:return '—'
    if ds=='Synth':
        g={}
        for k in ks:g.setdefault(k[1].split('/')[1],[]).append(k)
        s=[(sum(b[k] for k in v)-sum(a[k] for k in v))/sum(b[k] for k in v)*100 for v in g.values() if len(v)==4]
    else:s=[(b[k]-a[k])/b[k]*100 for k in ks]
    return f'{st.median(s):.2f} ({len(s)})'
ours=load('ours_v18_final')
if '--polarity' in sys.argv:
    print(json.dumps(json.loads((RES/'polarity_penalty.json').read_text()),ensure_ascii=False,indent=1));sys.exit()
print('| 数据 | '+' | '.join(n for _,n in CODECS)+' |');print('|'+'---|'*(len(CODECS)+1))
for d in DS:print(f'| {d} | '+' | '.join(saving(ours,load('base_'+c),d) for c,_ in CODECS)+' |')
