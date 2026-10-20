// 局部调色板概率专家：以因果邻域内的主要灰度构造条件分布，
// 以后验混合方式提供概率证据。
#pragma once
namespace palette_model {
inline int clip(int x,int a,int b){return std::max(a,std::min(b,x));}
struct Node {
 int values[16]={0},counts[16]={0};
 void observe(int value){
  int free=-1,found=-1;for(int i=0;i<16;i++){if(counts[i]&&values[i]==value)found=i;if(!counts[i])free=i;}
  if(found>=0){counts[found]++;if(counts[found]>=1024)for(int &n:counts)n=(n+1)/2;}
  else if(free>=0){values[free]=value;counts[free]=1;}
  else for(int &n:counts)n--;
 }
};
struct State {
 bool pending=false,active=false;int key=0,guess=0,min=0,max=0,count=0,position=0;
 int prior=8192,posterior=8192,last_base=2048,last_model=0,last_mixed=2048;
 std::array<int,8704> priors;std::array<Path,16> paths;int values[16],weights[16];
 State(){priors.fill(8192);}
};
inline State&state(){static State s;return s;}
inline void reset_pixel(){auto&s=state();s.pending=false;s.active=false;}
struct Memory {const void*identity=nullptr;int width=0,height=0;int64_t next=0,last=-1;std::array<Node,8704> nodes;
 void reset(const void*id,int w,int h){identity=id;width=w;height=h;next=0;last=-1;for(auto&n:nodes)n=Node();state().priors.fill(8192);}};
template<class Plane>int key(const Plane&p,int r,int c,int width){
 int v[4],n=0;if(r>0)v[n++]=int(p.get(r-1,c));if(c>0)v[n++]=int(p.get(r,c-1));
 if(r>0&&c>0)v[n++]=int(p.get(r-1,c-1));if(r>0&&c+1<width)v[n++]=int(p.get(r-1,c+1));
 if(!n)return 0;int anchor=v[0],best=0;
 for(int i=0;i<n;i++){int count=0;for(int j=0;j<n;j++)count+=v[i]==v[j];if(count>best){anchor=v[i];best=count;}}
 int low=*std::min_element(v,v+n),high=*std::max_element(v,v+n);return anchor*2+(high-low>=4);
}
inline bool geometry_enabled(){static bool enabled=std::getenv("SMEM_GEOMETRY_PALETTE")!=nullptr;return enabled;}
template<class Plane>int geometry_key(const Plane&p,int r,int c,int width,int parent){
 int anchor=parent/2,mask=0;
 if(r>0 && int(p.get(r-1,c))==anchor)mask|=1;
 if(c>0 && int(p.get(r,c-1))==anchor)mask|=2;
 if(r>0 && c>0 && int(p.get(r-1,c-1))==anchor)mask|=4;
 if(r>0 && c+1<width && int(p.get(r-1,c+1))==anchor)mask|=8;
 return 512+parent*16+mask;
}
template<class Plane>void prepare(const Plane&p,int r,int c,int width,int height,int guess,int minimum,int maximum){
 static Memory m;int64_t pos=int64_t(r)*width+c;
 if(m.identity!=&p||m.width!=width||m.height!=height||pos<=m.last)m.reset(&p,width,height);m.last=pos;
 for(;m.next<pos;m.next++){int rr=m.next/width,cc=m.next%width;int parent=key(p,rr,cc,width);m.nodes[parent].observe(int(p.get(rr,cc)));if(geometry_enabled())m.nodes[geometry_key(p,rr,cc,width,parent)].observe(int(p.get(rr,cc)));}
 auto&s=state();s.key=key(p,r,c,width);s.guess=guess;s.min=minimum-guess;s.max=maximum-guess;s.count=0;
 if(geometry_enabled()){int finer=geometry_key(p,r,c,width,s.key),samples=0;for(int n:m.nodes[finer].counts)samples+=n;if(samples>=8)s.key=finer;}
 const auto&node=m.nodes[s.key];int total=0;
 for(int i=0;i<16;i++)if(node.counts[i]){s.values[s.count]=node.values[i];s.weights[s.count++]=node.counts[i];total+=node.counts[i];}
 s.pending=total>=8;
}
inline void begin(int minimum,int maximum){auto&s=state();s.active=false;s.position=0;
 if(!s.pending||minimum!=s.min||maximum!=s.max)return;
 for(int i=0;i<s.count;i++){s.paths[i]=residual_head(minimum,maximum,clip(s.values[i]-s.guess,minimum,maximum));s.paths[i].weight=s.weights[i];}
 s.prior=s.priors[s.key];s.posterior=s.prior;s.active=true;
}
inline int chance(int base,int type,int index){auto&s=state();s.last_base=base;s.last_mixed=base;if(!s.active)return base;
 int total=0,ones=0;for(int i=0;i<s.count;i++)if(s.paths[i].weight){const auto&p=s.paths[i];assert(s.position<p.length);auto&e=p.code[s.position];assert(e.type==type&&e.index==index);total+=p.weight;if(e.bit)ones+=p.weight;}
 s.last_model=total?(ones*4096+total/2)/total:0;if(!total)s.posterior=0;
 s.last_mixed=clip(int((int64_t(65536-s.posterior)*base+int64_t(s.posterior)*s.last_model+32768)/65536),1,4095);return s.last_mixed;
}
inline void observe(bool bit){auto&s=state();if(!s.active)return;int pb=bit?s.last_base:4096-s.last_base,ps=bit?s.last_model:4096-s.last_model;
 int64_t den=int64_t(65536-s.posterior)*pb+int64_t(s.posterior)*ps;
 s.posterior=den?int((int64_t(s.posterior)*ps*65536+den/2)/den):0;
 for(int i=0;i<s.count;i++)if(s.paths[i].weight&&s.paths[i].code[s.position].bit!=bit)s.paths[i].weight=0;s.position++;if(s.position==1&&!bit)for(int q=0;q<s.count;q++)if(s.paths[q].weight)materialize(s.paths[q]);
}
inline void end(){auto&s=state();if(s.active)s.priors[s.key]=clip((7*s.prior+s.posterior+4)/8,256,61440);s.active=false;}
}
