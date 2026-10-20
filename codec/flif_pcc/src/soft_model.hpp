// 因果软上下文检索：四个固定子模板取候选，完整模板验证，
// 候选灰度区间迁移后作为概率证据。
#pragma once
namespace soft_model {
inline bool enabled(){static bool b=std::getenv("SMEM_SOFT_CONTEXT")!=nullptr&&std::getenv("SMEM_NO_NORM")==nullptr;return b;}
inline int clip(int x,int a,int b){return std::max(a,std::min(b,x));}
struct Descriptor {int low=0,span=0,dir=1;int q[10];int v[10]={0};};
template<class Plane>bool describe(const Plane&p,int64_t pos,int w,Descriptor&d){
 int r=pos/w,c=pos%w;if(r<2||c<2||c+2>=w)return false;
 const int dy[10]={0,-1,-1,-1,0,-2,-2,-2,-1,-1},dx[10]={-1,0,-1,1,-2,0,-1,1,-2,2};
 int *v=d.v;for(int i=0;i<10;i++)v[i]=int(p.get(r+dy[i],c+dx[i]));
 d.low=*std::min_element(v,v+10);d.span=*std::max_element(v,v+10)-d.low;if(d.span<4)return false;
 for(int i=1;i<10;i++)if(v[i]!=v[0]){d.dir=v[0]<v[i]?1:-1;break;}
 for(int i=0;i<10;i++)d.q[i]=((d.dir>0?v[i]-d.low:d.low+d.span-v[i])*15+d.span/2)/d.span;
 return true;
}
inline uint64_t key(const Descriptor&d,int mode){
 const int sub[4][5]={{0,1,2,3,4},{0,1,2,5,6},{0,1,3,5,7},{0,2,4,6,8}};
 uint64_t k=mode;for(int i=0;i<5;i++)k=(k<<2)|uint64_t(d.q[sub[mode][i]]/4);return k;
}
inline bool diverse_enabled(){static bool b=std::getenv("SMEM_DIVERSE_HISTORY")!=nullptr;return b;}
struct Slots {int64_t a=-1,b=-1;int category=-999;void insert(int64_t p,int q){if(!diverse_enabled()||q!=category)b=a;a=p;category=q;}};
struct Stats {
    bool enabled=std::getenv("SOFT_STATS_PATH")!=nullptr;uint64_t queried=0,retrieved=0,active=0,hits=0;double base_bits=0,mixed_bits=0;
 ~Stats(){const char*n=std::getenv("SOFT_STATS_PATH");if(!n)return;std::ofstream f(n);
 f<<"{\"queried\":"<<queried<<",\"retrieved\":"<<retrieved<<",\"active\":"<<active<<",\"hits\":"<<hits<<",\"base_bits\":"<<base_bits<<",\"mixed_bits\":"<<mixed_bits<<"}\n";}};
inline Stats&stats(){static Stats t;return t;}
struct State {bool pending=false,active=false;int guess=0,min=0,max=0,count=0,position=0,key=0;
 int targets[256],weights[256];Path paths[256];std::array<int,5>priors;
 int prior=8192,posterior=8192,last_base=2048,last_model=0,last_mixed=2048;
 State(){priors.fill(8192);}};
// 迁移源量化单元与目标区间的精确整数重叠。
inline void add_mass(State&s,int value,int weight){if(weight<=0)return;for(int i=0;i<s.count;i++)if(s.targets[i]==value){s.weights[i]+=weight;return;}assert(s.count<256);s.targets[s.count]=value;s.weights[s.count++]=weight;}
inline void cell_transport(State&s,const Descriptor&cur,const Descriptor&past,int value,int weight){
 int offset=past.dir>0?value-past.low:past.low+past.span-value;
 int center=cur.dir>0?cur.low*past.span+offset*cur.span:(cur.low+cur.span)*past.span-offset*cur.span;
 static bool calibrate=std::getenv("SMEM_ANCHOR_CALIBRATION")!=nullptr;
 static bool residual_mix=std::getenv("SMEM_RESIDUAL_MIX")!=nullptr;
 if(calibrate||residual_mix){
  int residual[10];for(int j=0;j<10;j++){
   int o=past.dir>0?past.v[j]-past.low:past.low+past.span-past.v[j];
   int predicted=cur.dir>0?cur.low*past.span+o*cur.span:(cur.low+cur.span)*past.span-o*cur.span;
   residual[j]=cur.v[j]*past.span-predicted;
  }
  std::sort(residual,residual+10);
  // 中位偏移采用双倍分子计算，避免浮点舍入。
  int adjustment=residual[4]+residual[5];
  int left=2*center+adjustment-cur.span,right=2*center+adjustment+cur.span,total=0;
  for(int x=clip(left/(2*past.span)-2,0,255);x<=clip(right/(2*past.span)+2,0,255);x++){int low=x==0?left:std::max(left,(2*x-1)*past.span);int high=x==255?right:std::min(right,(2*x+1)*past.span);int mass=std::max(0,high-low);if(mass){add_mass(s,x,mass*weight);total+=mass;}}
  assert(total==2*cur.span);if(!residual_mix)return;weight*=3;
 }
 static bool use_cell=std::getenv("SMEM_TRANSPORT_CELL")!=nullptr;
 if(!use_cell){int x=center>=0?(center+past.span/2)/past.span:-((-center+past.span/2)/past.span);add_mass(s,clip(x,0,255),weight*2*cur.span);return;}
 int left=2*center-cur.span,right=2*center+cur.span,total=0;
 for(int x=clip(left/(2*past.span)-2,0,255);x<=clip(right/(2*past.span)+2,0,255);x++){
  int low=x==0?left:std::max(left,(2*x-1)*past.span);
  int high=x==255?right:std::min(right,(2*x+1)*past.span);
  int mass=std::max(0,high-low);if(mass){add_mass(s,x,mass*weight);total+=mass;}
 }
 assert(total==2*cur.span);
}
inline State&state(){static State s;return s;}
inline void reset_pixel(){auto&s=state();s.pending=false;s.active=false;}
struct Memory {const void*identity=nullptr;int width=0,height=0;int64_t last=-1,next=0;
 std::array<Slots,4096>index;
 void reset(const void*id,int w,int h){identity=id;width=w;height=h;last=-1;next=0;for(auto&x:index)x=Slots();state().priors.fill(8192);}};
template<class Plane>void insert(Memory&m,const Plane&p,int64_t pos){Descriptor d;if(describe(p,pos,m.width,d))for(int k=0;k<4;k++)m.index[key(d,k)].insert(pos,clip(((d.dir>0?int(p.get(pos/m.width,pos%m.width))-d.low:d.low+d.span-int(p.get(pos/m.width,pos%m.width)))*15+d.span/2)/d.span,-16,31));}
template<class Plane>void prepare(const Plane&p,int r,int c,int w,int h,int guess,int minimum,int maximum){
 if(!enabled())return;static Memory m;int64_t pos=int64_t(r)*w+c;
 if(m.identity!=&p||m.width!=w||m.height!=h||pos<=m.last)m.reset(&p,w,h);m.last=pos;
 for(;m.next<pos;m.next++)insert(m,p,m.next);
 auto&s=state();s.count=0;s.pending=false;s.guess=guess;s.min=minimum-guess;s.max=maximum-guess;
 // 精确归一化二维匹配已有专家，仅在未命中时补充证据。
 if(match_model::state().mask&8)return;
 Descriptor d;if(!describe(p,pos,w,d))return;stats().queried++;
 int64_t refs[8];int n=0,best=1000;
 for(int k=0;k<4;k++){auto slot=m.index[key(d,k)];for(auto ref:{slot.a,slot.b}){
  if(ref<0||ref>=pos)continue;bool duplicate=false;for(int j=0;j<n;j++)duplicate|=refs[j]==ref;if(duplicate)continue;refs[n++]=ref;
  Descriptor past;if(!describe(p,ref,w,past))continue;
  int distance=0;for(int j=0;j<10;j++)distance+=std::abs(d.q[j]-past.q[j]);if(distance>20)continue;
  int y=int(p.get(ref/w,ref%w));cell_transport(s,d,past,y,21-distance);best=std::min(best,distance);
 }}
 s.pending=s.count>0;if(s.pending){stats().retrieved++;s.key=std::min(4,best/5);}
}
inline void begin(int minimum,int maximum){auto&s=state();s.active=false;s.position=0;
 if(!s.pending||minimum!=s.min||maximum!=s.max)return;
 for(int i=0;i<s.count;i++){s.paths[i]=residual_head(minimum,maximum,clip(s.targets[i]-s.guess,minimum,maximum));s.paths[i].weight=s.weights[i];}
 s.prior=s.priors[s.key];s.posterior=s.prior;s.active=true;stats().active++;
}
inline int chance(int base,int type,int index){auto&s=state();s.last_base=base;s.last_mixed=base;if(!s.active)return base;
 int total=0,ones=0;for(int i=0;i<s.count;i++)if(s.paths[i].weight){const auto&p=s.paths[i];assert(s.position<p.length);auto&e=p.code[s.position];assert(e.type==type&&e.index==index);total+=p.weight;if(e.bit)ones+=p.weight;}
 s.last_model=total?(ones*4096+total/2)/total:0;if(!total)s.posterior=0;
 return s.last_mixed=clip(int((int64_t(65536-s.posterior)*base+int64_t(s.posterior)*s.last_model+32768)/65536),1,4095);
}
inline void observe(bool bit){auto&s=state();if(!s.active)return;int pb=bit?s.last_base:4096-s.last_base,ps=bit?s.last_model:4096-s.last_model;
 if(stats().enabled){stats().base_bits-=std::log2(pb/4096.);stats().mixed_bits-=std::log2((bit?s.last_mixed:4096-s.last_mixed)/4096.);}
 int64_t den=int64_t(65536-s.posterior)*pb+int64_t(s.posterior)*ps;s.posterior=den?int((int64_t(s.posterior)*ps*65536+den/2)/den):0;
 for(int i=0;i<s.count;i++)if(s.paths[i].weight&&s.paths[i].code[s.position].bit!=bit)s.paths[i].weight=0;s.position++;if(s.position==1&&!bit)for(int q=0;q<s.count;q++)if(s.paths[q].weight)materialize(s.paths[q]);
}
inline void end(){auto&s=state();if(s.active){s.priors[s.key]=clip((7*s.prior+s.posterior+4)/8,256,61440);int n=0;for(int i=0;i<s.count;i++)n+=s.paths[i].weight;if(n)stats().hits++;}s.active=false;}
}
