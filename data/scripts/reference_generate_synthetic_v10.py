# 合成文字图像的原始生成脚本（依赖 macOS 系统字体，仅供查阅）。
from pathlib import Path
import json,hashlib,datetime,functools
import numpy as np
from PIL import Image,ImageDraw,ImageFont
V=Path(__file__).resolve().parents[1];C=V.parent;R=C.parent;D=C/'v3_归一化条件分布熵编码_2026-10-18';O=V/'新增独立内容1000'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
@functools.lru_cache(maxsize=96)
def font(path,size):return ImageFont.truetype(path,size)
def main():
 assert json.loads((V/'configs/selection.json').read_text())['selected']=='regime'
 for sub in ['data','configs','results','files','logs']:(O/sub).mkdir(parents=True,exist_ok=True)
 target=O/'configs/manifest.json'
 if target.exists():
  rows=json.loads(target.read_text());assert len(rows)==4000
  for r in rows:assert sha(O/r['path'])==r['sha256']
  print('Verified retained4000 inputs');return
 fonts=json.loads((D/'新增独立内容100/configs/split_audit.json').read_text())['fonts']
 for r in fonts.values():assert sha(Path(r['path']))==r['sha256']
 old=json.loads((D/'新增独立内容100/configs/manifest.json').read_text());old_text={r['ground_truth'] for r in old};old_hash={r['sha256'] for r in json.loads((V/'data/输入清单.json').read_text())}
 bankzh=['图像分析','编码实验','读书记录','软件设计','信号处理','数学模型','校园活动','科学计算','语言表达','数据管理','信息检索','界面布局','地图标注','资源共享','知识结构','实时监测','字体设计','消息队列','课程测验','服务统计','环境观测','器材清单','历史档案','阅读进度','交通线路','网络日志','天气记录','学习日程','数字媒体','可靠传输','参数说明','图书分类']
 banken=['image study','coding trial','reading notes','software design','signal analysis','math model','campus event','scientific computing','language skills','data handling','query system','layout review','map legend','shared resource','knowledge graph','live monitor','font design','message queue','course quiz','service count','field sensor','equipment list','history record','reading progress','traffic route','network log','weather record','study plan','digital media','reliable link','parameter notes','book catalogue']
 rng=np.random.default_rng(2610181000);rows=[];groups=[];texts=set();hashes=set()
 for g in range(1000):
  gid=f'expert_independent_{g:04d}';family=['chinese','english','mixed_language','tabular_code'][g%4];w,h=[(257,97),(321,113),(385,129),(289,105)][(g//4)%4];panels=[];descs=[]
  for j in range(4):
   lang='zh' if family=='chinese' or(family=='mixed_language' and j%2==0) else 'en';bank=bankzh if lang=='zh' else banken
   words=rng.choice(bank,4,replace=False).tolist();token=''.join(rng.choice(list('23456789ABCDEFGHJKMNPQRSTUVWXYZ'),10));num=int(rng.integers(10,99999))
   if family=='tabular_code':lines=[f'item_{token[:5]} = {num}',f'key | {words[0]}',f'{words[1]} : {int(rng.integers(1,999))}',f'log_{token[5:]} -> ok']
   elif lang=='zh':lines=[words[0]+' / '+words[1],words[2]+'与'+words[3],f'采样序号 {num}',f'记录 {token}']
   else:lines=[words[0]+' / '+words[1],words[2]+' + '+words[3],f'record {num}',f'key {token}']
   truth='\n'.join(lines);assert truth not in texts and truth not in old_text;texts.add(truth)
   keys=['zh_noto','zh_heiti','zh_songti'] if lang=='zh' else ['en_arial','en_times','en_courier','en_georgia'];key=keys[int(rng.integers(len(keys)))];aa=[1,2,4][(g+j)%3];size=[9,11,13,16,19][(g//7+j)%5];f=font(fonts[key]['path'],size*aa)
   while max(f.getlength(t) for t in lines)>(w-16)*aa:size-=1;f=font(fonts[key]['path'],size*aa)
   mask=Image.new('L',(w*aa,h*aa),255);draw=ImageDraw.Draw(mask);px,py=map(int,rng.integers(0,4,2))
   for k,line in enumerate(lines):draw.text(((5+px)*aa,(2+py+k*(h//4))*aa),line,font=f,fill=0)
   if family=='tabular_code':
    for yy in [h//2,h-3]:draw.line([(3*aa,yy*aa),((w-3)*aa,yy*aa)],fill=0,width=aa)
   if aa!=1:mask=mask.resize((w,h),Image.Resampling.LANCZOS)
   panels.append(np.asarray(mask,dtype=np.int32));descs.append(dict(text=truth,language=lang,font=key,size=size,aa=aa,phase=[px,py]))
  # 各条件均含四段不同文本，改动仅作用于面板外观。
  conditions={'uniform':[(255,0)]*4,'moderate':[(245,35),(225,65),(200,90),(180,110)],'low_contrast':[(220,180),(170,140),(120,100),(75,63)],'polarity_mixed':[(248,8),(25,225),(190,115),(70,100)]}
  groups.append(dict(group=gid,family=family,panels=descs,panel_width=w,panel_height=h))
  for condition,palette in conditions.items():
   x=np.full((2*h+11,2*w+13),128,np.uint8)
   for j,(bg,fg) in enumerate(palette):
    patch=((panels[j]*bg+(255-panels[j])*fg+127)//255).astype(np.uint8);yy=(j//2)*(h+7)+2;xx=(j%2)*(w+9)+2;x[yy:yy+h,xx:xx+w]=patch
   p=O/'data'/condition/(gid+'.pgm');p.parent.mkdir(exist_ok=True);Image.fromarray(x).save(p);digest=sha(p);assert digest not in hashes and digest not in old_hash;hashes.add(digest)
   rows.append(dict(id=gid,group=gid,condition=condition,family=family,path=str(p.relative_to(O)),sha256=digest,width=x.shape[1],height=x.shape[0],pixels=x.size,split='frozen_new_content_test',palette=palette))
  if (g+1)%100==0:print('generated',g+1,'/1000 groups',flush=True)
 target.write_text(json.dumps(rows,ensure_ascii=False,indent=2));(O/'configs/content_groups.json').write_text(json.dumps(groups,ensure_ascii=False,indent=2))
 (O/'configs/split_audit.json').write_text(json.dumps(dict(seed=2610181000,groups=1000,images=4000,distinct_panel_texts=4000,conditions=4,prior_complete_text_overlap=0,prior_input_hash_overlap=0,generator_sha256=sha(Path(__file__)),selection_sha256=sha(V/'configs/selection.json'),fonts=fonts,scope='new synthetic content groups, shared renderer/font families; not a new real-world dataset; variants grouped for inference',created=datetime.datetime.now().isoformat()),ensure_ascii=False,indent=2))
if __name__=='__main__':main()
