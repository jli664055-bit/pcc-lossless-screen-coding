// 在线线性预测专家：对因果邻域做 RLS 拟合，并按上下文条件化给出残差概率证据。
#pragma once
namespace online_model {
inline int clip(int x,int a,int b){return std::max(a,std::min(b,x));}
inline int mode(){static int m=[](){const char*p=std::getenv("SMEM_ONLINE_PREDICTOR");return p?std::atoi(p):0;}();return m;}
struct RLS {
 static const int D=7; double w[D]={0},P[D][D];uint64_t samples=0;
 RLS(){for(int i=0;i<D;i++)for(int j=0;j<D;j++)P[i][j]=i==j?4.:0.;}
 double predict(const std::array<double,D>&x)const{double y=0;for(int i=0;i<D;i++)y+=w[i]*x[i];return y;}
 void update(const std::array<double,D>&x,double y){
  samples++;if(mode()==3 && samples%4)return;
  double u[D]={0},den=.999;
  for(int i=0;i<D;i++){for(int j=0;j<D;j++)u[i]+=P[i][j]*x[j];den+=u[i]*x[i];}
  if(!(den>.000001)||!std::isfinite(den)){*this=RLS();return;}
  double error=std::max(-4.,std::min(4.,y-predict(x)));
  for(int i=0;i<D;i++)w[i]+=u[i]*error/den;
  for(int i=0;i<D;i++)for(int j=i;j<D;j++){double v=(P[i][j]-u[i]*u[j]/den)/.999;P[i][j]=P[j][i]=v;}
  for(int i=0;i<D;i++)if(!std::isfinite(w[i])||P[i][i]>100000||P[i][i]<0){*this=RLS();return;}
 }
};
// 2025 年文献中符号梯度更新的独立实现。
// 估计器集合按 8 位灰度调整，与原文献的 L1 编码器不同。
struct IntegerL1 {
 std::array<int64_t,8> weights;uint64_t samples=0;
 IntegerL1(){weights.fill(131072/8);}
 int raw(const std::array<int,8>&x)const{int64_t y=65536;for(int i=0;i<8;i++)y+=weights[i]*x[i];return int(y>=0?y/131072:-((-y+131071)/131072));}
 void update(const std::array<int,8>&x,int target){int pred=raw(x);int sign=(target>pred)-(target<pred);for(int i=0;i<8;i++)weights[i]=std::max<int64_t>(-(int64_t(1)<<30),std::min<int64_t>(int64_t(1)<<30,weights[i]+sign*int64_t(x[i])));samples++;}
};
struct Histogram {
 std::array<int,511> count{};std::array<int,513> tree{};int samples=0,maeQ8=1024;
 int prefix(int x)const{if(x<0)return 0;x=std::min(510,x);int n=0;for(int i=x+1;i;i-=i&-i)n+=tree[i];return n;}
 int range(int lo,int hi)const{return lo>hi?0:prefix(hi+255)-prefix(lo+254);}
 void observe(int error){
  error=clip(error,-255,255);int at=error+255;count[at]++;samples++;for(int i=at+1;i<=511;i+=i&-i)tree[i]++;
  maeQ8+=(std::abs(error)*256-maeQ8)/32;
  if(samples>=4096){samples=0;tree.fill(0);for(int j=0;j<511;j++){count[j]=(count[j]+1)/2;samples+=count[j];for(int i=j+1;i<=511;i+=i&-i)tree[i]+=count[j];}}
 }
};
struct Prior {
 std::array<std::array<int,512>,8> cdf;
 Prior(){for(int k=0;k<8;k++){cdf[k][0]=0;for(int j=0;j<511;j++)cdf[k][j+1]=cdf[k][j]+std::max(1,int(4096*std::exp(-std::abs(j-255)/double(1<<k))+.5));}}
 int range(int k,int lo,int hi)const{lo=clip(lo,-255,255);hi=clip(hi,-255,255);return lo>hi?0:cdf[k][hi+256]-cdf[k][lo+255];}
};
inline const Prior&prior(){static Prior p;return p;}
struct IntervalSet {
 int lo[2]={0,0},hi[2]={-1,-1},n=0;
 void insert(int x){if(n&&hi[n-1]+1==x)hi[n-1]=x;else{assert(n<2);lo[n]=hi[n]=x;n++;}}
};
struct TreeNode {int child[2]={-1,-1};int type=-1,index=-1;IntervalSet values;};
struct Topology {
 std::vector<TreeNode> nodes;
 Topology(int guess,int minimum,int maximum){
  nodes.emplace_back();
  for(int x=minimum;x<=maximum;x++){
   Path path=residual_path(minimum-guess,maximum-guess,x-guess);int at=0;nodes[at].values.insert(x);
   for(int j=0;j<path.length;j++){
    Event e=path.code[j];if(nodes[at].type<0){nodes[at].type=e.type;nodes[at].index=e.index;}
    assert(nodes[at].type==e.type&&nodes[at].index==e.index);
    int next=nodes[at].child[e.bit];if(next<0){next=int(nodes.size());nodes[at].child[e.bit]=next;nodes.emplace_back();}
    at=next;nodes[at].values.insert(x);
   }
  }
 }
};
struct Features {std::array<double,7>x;std::array<int,8> estim;int clampLo=0,clampHi=255;int anchor=0,key=0,median=0;};
template<class Plane>Features features(const Plane&p,int r,int c,int width){
 Features f;int W=c?int(p.get(r,c-1)):r?int(p.get(r-1,c)):128;
 int N=r?int(p.get(r-1,c)):W,NW=r&&c?int(p.get(r-1,c-1)):N;
 int NE=r&&c+1<width?int(p.get(r-1,c+1)):N,NN=r>1?int(p.get(r-2,c)):N;
 int WW=c>1?int(p.get(r,c-2)):W,NEE=r&&c+2<width?int(p.get(r-1,c+2)):NE;
 int NNN=r>2?int(p.get(r-3,c)):NN,WWW=c>2?int(p.get(r,c-3)):WW;
 int NNE=r>1&&c+1<width?int(p.get(r-2,c+1)):NN;
 int NEEE=r&&c+3<width?int(p.get(r-1,c+3)):NE;
 f.estim={{N-128,W-128,N+W-NW-128,W+NE-N-128,3*(N-NN)+NNN-128,3*(W-WW)+WWW-128,N+NE-NNE-128,(N+W+NE+NW)/4-128}};
 f.clampLo=std::min(std::min(N,W),std::min(NE,NEEE));f.clampHi=std::max(std::max(N,W),std::max(NE,NEEE));
 int a[6]={W,NW,NE,NN,WW,NEE};f.anchor=N;
 for(int i=0;i<6;i++)f.x[i]=(a[i]-N)/64.;f.x[6]=1.;
 f.median=clip(W+N-NW,std::min(W,N),std::max(W,N));
 int gh=std::abs(W-WW)+std::abs(N-NW)+std::abs(NE-N),gv=std::abs(N-NN)+std::abs(W-NW)+std::abs(NE-NEE);
 int direction=gh>2*gv?1:gv>2*gh?2:0;
 int distinct=1;for(int i=0;i<6;i++){bool seen=a[i]==N;for(int j=0;j<i;j++)seen=seen||a[j]==a[i];distinct+=!seen;}
 f.key=direction+3*(distinct>=5)+6*(gh+gv>=64);return f;
}
struct Memory {
 const void*identity=nullptr;int width=0,height=0;int64_t next=0,last=-1;
 std::array<IntegerL1,13> integer_predictors;std::array<RLS,13> regressors;std::array<Histogram,12> histograms;std::array<int,12> reliability;
 std::unordered_map<int,std::unique_ptr<Topology>> topologies;
 Memory(){reliability.fill(8192);}
 int predict(const Features&f)const{
  if(mode()>=4){
   int y=integer_predictors[12].raw(f.estim);
   if(mode()==5&&integer_predictors[f.key].samples>=64)y=integer_predictors[f.key].raw(f.estim);
   return clip(y+128,f.clampLo,f.clampHi);
  }
  const RLS&global=regressors[12];const RLS&local=regressors[f.key];
  double y=global.predict(f.x);
  if(mode()>=2){double a=std::min(1.,local.samples/64.);y=(1-a)*y+a*local.predict(f.x);}
  return clip(int(std::floor(f.anchor+64*y+.5)),0,255);
 }
 void train(const Features&f,int actual){int pred=predict(f);histograms[f.key].observe(actual-pred);
  if(mode()>=4){integer_predictors[12].update(f.estim,actual-128);if(mode()==5)integer_predictors[f.key].update(f.estim,actual-128);return;}
  double y=(actual-f.anchor)/64.;regressors[12].update(f.x,y);if(mode()>=2)regressors[f.key].update(f.x,y);}
};
struct State {
 bool pending=false,active=false;Memory*m=nullptr;const Topology*topology=nullptr;int node=0,key=0,pred=0,scale=0,guess=0,min=0,max=255;
 int reliability=8192,posterior=8192,last_base=2048,last_model=2048,last_mixed=2048;
};
inline State&state(){static State s;return s;}
inline void reset_pixel(){state().pending=state().active=false;}
template<class Plane>void prepare(const Plane&p,int r,int c,int width,int height,int guess,int minimum,int maximum){
 if(!mode()||minimum<0||maximum>255)return;
 static std::unique_ptr<Memory> memory(new Memory());Memory*m=memory.get();int64_t pos=int64_t(r)*width+c;
 if(m->identity!=&p||m->width!=width||m->height!=height||pos<=m->last){memory.reset(new Memory());m=memory.get();m->identity=&p;m->width=width;m->height=height;}
 m->last=pos;
 for(;m->next<pos;m->next++){int rr=int(m->next/width),cc=int(m->next%width);Features f=features(p,rr,cc,width);m->train(f,int(p.get(rr,cc)));}
 Features f=features(p,r,c,width);auto&s=state();s.m=m;s.key=f.key;s.pred=m->predict(f);s.guess=guess;s.min=minimum-guess;s.max=maximum-guess;
 int mae=std::max(1,m->histograms[f.key].maeQ8/256);s.scale=0;while(s.scale<7&&(1<<s.scale)<mae)s.scale++;
 int key=guess|(minimum<<8)|(maximum<<16);auto it=m->topologies.find(key);
 if(it==m->topologies.end()){if(m->topologies.size()>=512)m->topologies.clear();it=m->topologies.emplace(key,std::unique_ptr<Topology>(new Topology(guess,minimum,maximum))).first;}
 s.topology=it->second.get();s.pending=true;
}
inline void begin(int minimum,int maximum){auto&s=state();s.active=s.pending&&minimum==s.min&&maximum==s.max;s.pending=false;s.node=0;if(s.active){s.reliability=s.m->reliability[s.key];s.posterior=s.reliability;}}
inline uint64_t mass(const IntervalSet&is){auto&s=state();const auto&h=s.m->histograms[s.key];const auto&p=prior();uint64_t a=0,b=0;for(int j=0;j<is.n;j++){a+=h.range(is.lo[j]-s.pred,is.hi[j]-s.pred);b+=p.range(s.scale,is.lo[j]-s.pred,is.hi[j]-s.pred);}return a*uint64_t(p.cdf[s.scale][511])+32*b;}
inline int chance(int base,int type,int index){
 auto&s=state();s.last_base=s.last_mixed=base;if(!s.active)return base;
 const auto&node=s.topology->nodes[s.node];assert(node.type==type&&node.index==index);
 uint64_t total=mass(node.values),ones=node.child[1]<0?0:mass(s.topology->nodes[node.child[1]].values);
 s.last_model=total?int((ones*4096+total/2)/total):2048;
 s.last_mixed=clip(int((int64_t(65536-s.posterior)*base+int64_t(s.posterior)*s.last_model+32768)/65536),1,4095);return s.last_mixed;
}
inline void observe(bool bit){auto&s=state();if(!s.active)return;
 int pb=bit?s.last_base:4096-s.last_base,pm=bit?s.last_model:4096-s.last_model;
 int64_t den=int64_t(65536-s.posterior)*pb+int64_t(s.posterior)*pm;
 s.posterior=den?int((int64_t(s.posterior)*pm*65536+den/2)/den):0;
 s.node=s.topology->nodes[s.node].child[bit];assert(s.node>=0);
}
inline void end(){auto&s=state();if(s.active)s.m->reliability[s.key]=clip((7*s.reliability+s.posterior+4)/8,256,61440);s.active=false;}
}
