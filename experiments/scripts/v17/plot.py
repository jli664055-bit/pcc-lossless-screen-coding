"""v17 阶段图：消融阶梯图与 v12/v17 对比曲线（中文标签）。"""
import csv,json
from pathlib import Path
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
V=Path(__file__).resolve().parents[1]
plt.rcParams.update({'font.family':'Hiragino Sans GB','font.size':9,'axes.unicode_minus':False,'axes.spines.top':False,'axes.spines.right':False,'axes.edgecolor':'#8a8984','axes.labelcolor':'#52514e','xtick.color':'#52514e','ytick.color':'#52514e'})
C1,C2,INK,MUTED,GRID='#2a78d6','#eb6834','#0b0b0b','#52514e','#e6e5e0'
rows=list(csv.DictReader(open(V/'results/summary.csv')))
def get(c,g,r):return next(x for x in rows if x['config']==c and x['group']==g and x['reference']==r)
# 图 1：消融阶梯
cfgs=[('A1_mix1_apm','+单混合器+APM'),('A2_mix3_apm','+三门控混合器'),('A3_plus_cm','+直接上下文模型'),('A4_plus_ncm','+邻域一致性上下文'),('F_full','+值域直方图专家'),('G_final','最终版(8直方图+提速)')]
fig,ax=plt.subplots(figsize=(6.2,3.6),dpi=300)
import numpy as np
y=np.arange(len(cfgs))[::-1];h=0.36
for k,(g,lab,col) in enumerate([('Heldout98','留出集98图',C1),('Contrast32','跨对比度32图',C2)]):
    vals=[float(get(c,g,'v12(A0)')['median']) for c,_ in cfgs]
    bars=ax.barh(y+(h/2+0.02)*(1 if k==0 else -1),vals,height=h,color=col,label=lab)
    for b,v in zip(bars,vals):ax.text(v+0.08,b.get_y()+b.get_height()/2,f'{v:.2f}%',va='center',fontsize=7.5,color=INK)
ax.set_yticks(y);ax.set_yticklabels([l for _,l in cfgs]);ax.set_xlabel('相对v12的文件大小中位节省（%）')
ax.xaxis.grid(True,color=GRID,lw=0.6);ax.set_axisbelow(True);ax.set_xlim(0,8.6)
ax.legend(frameon=False,loc='lower center',bbox_to_anchor=(0.5,1.0),ncol=2);fig.tight_layout()
for ext in ['png','pdf']:fig.savefig(V/f'figures/图1_消融阶梯.{ext}')
# 图 2：v12 与 v17 相对基线（104 张）
refs=[('flif_matched','FLIF(匹配设置)'),('webp_m6','WebP无损m6'),('jxl_e9','JPEG XL e9'),('paq8px_8','paq8px -8')]
fig,ax=plt.subplots(figsize=(6.2,2.6),dpi=300)
y=np.arange(len(refs))[::-1]
a=[float(get('A0_v12','All104',r)['median']) for r,_ in refs];b=[float(get('G_final','All104',r)['median']) for r,_ in refs]
for yi,ai,bi in zip(y,a,b):ax.plot([ai,bi],[yi,yi],color='#c3c2b7',lw=2,zorder=1)
ax.scatter(a,y,s=42,color=C1,zorder=2,label='v12（改进前）',edgecolor='white',lw=1.2)
ax.scatter(b,y,s=42,color=C2,zorder=3,label='v17（本文）',edgecolor='white',lw=1.2)
for yi,ai,bi in zip(y,a,b):
    ax.text(ai,yi+0.22,f'{ai:.1f}',ha='center',fontsize=7.5,color=MUTED);ax.text(bi,yi-0.38,f'{bi:.1f}',ha='center',fontsize=7.5,color=INK)
ax.axvline(0,color='#8a8984',lw=0.8);ax.set_yticks(y);ax.set_yticklabels([l for _,l in refs])
ax.set_xlabel('相对参照编码器的文件中位节省（%，负值表示更大），104图');ax.xaxis.grid(True,color=GRID,lw=0.6);ax.set_axisbelow(True)
ax.set_ylim(-0.7,len(refs)-0.3);ax.legend(frameon=False,loc='lower right',ncol=2);fig.tight_layout()
for ext in ['png','pdf']:fig.savefig(V/f'figures/图2_相对作者基线.{ext}')
print('ok')
