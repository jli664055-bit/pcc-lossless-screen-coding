// 因果二维原始与归一化匹配专家：在历史像素中检索相似模板并输出概率证据，
// 与对比度/极性迁移及 FLIF 残差编码器联合使用。
#pragma once
namespace match_model {
inline int clip(int value,int low,int high){return std::max(low,std::min(high,value));}
struct Descriptor {
    uint64_t hash=1469598103934665603ULL;
    int low=0,span=1,direction=1;
    int canonical(int x)const{return ((direction>0?x-low:low+span-x)*4096)/span;}
    int transport(int q)const {
        int value=(q*span+(q>=0?2048:-2048))/4096;
        return direction>0?low+value:low+span-value;
    }
};
template<class Plane>int pixel(const Plane&p,int64_t pos,int width){return int(p.get(int(pos/width),int(pos%width)));}
template<class Plane>bool describe(const Plane&p,int64_t pos,int width,int mode,Descriptor &d) {
    bool normalized=(mode&1);int length=mode<2?(normalized?16:32):10;
    int values[32];
    if(mode<2){if(pos<length)return false;for(int i=0;i<length;i++)values[i]=pixel(p,pos-length+i,width);}
    else {
        int r=int(pos/width),c=int(pos%width);if(r<2||c<2||c+2>=width)return false;
        const int dy[10]={0,-1,-1,-1,0,-2,-2,-2,-1,-1};
        const int dx[10]={-1,0,-1,1,-2,0,-1,1,-2,2};
        for(int i=0;i<length;i++)values[i]=int(p.get(r+dy[i],c+dx[i]));
    }
    if(normalized) {
        d.low=*std::min_element(values,values+length);int high=*std::max_element(values,values+length);
        d.span=high-d.low;if(d.span<4)return false;
        for(int i=1;i<length;i++)if(values[i]!=values[0]){d.direction=values[0]<values[i]?1:-1;break;}
    }
    for(int i=0;i<length;i++) {
        int value=normalized?((d.direction>0?values[i]-d.low:d.low+d.span-values[i])*15+d.span/2)/d.span:values[i];
        d.hash^=uint64_t(value);d.hash*=1099511628211ULL;
    }
    return true;
}
struct Stats {
    bool enabled=std::getenv("MATCH_STATS_PATH")!=nullptr;
    uint64_t symbols=0,active_symbols=0,raw_symbols=0,normalized_symbols=0,successful_symbols=0,bits=0;
    double baseline_bits=0,mixed_bits=0;
    ~Stats(){const char*name=std::getenv("MATCH_STATS_PATH");if(!name)return;std::ofstream f(name);
        f<<"{\"symbols\":"<<symbols<<",\"active_symbols\":"<<active_symbols<<",\"raw_symbols\":"<<raw_symbols
         <<",\"normalized_symbols\":"<<normalized_symbols<<",\"successful_symbols\":"<<successful_symbols
         <<",\"bits\":"<<bits<<",\"shadow_v3_bits\":"<<baseline_bits<<",\"mixed_bits\":"<<mixed_bits<<"}\n";}
};
inline Stats&stats(){static Stats s;return s;}
inline int learning_mode(){static int mode=[](){const char*p=std::getenv("SMEM_EXPERT_LEARNING");return p?std::atoi(p):0;}();return mode;}
struct State {
    int collecting_mode=0,expert_mass[4][256]={{0}},expert_key[4]={0};
    std::array<int,128> reliability;
    uint32_t base_likelihood=1u<<24;

    bool pending=false,active=false;
    int guess=0,min=0,max=0,count=0,position=0,key=0,mask=0,prior=8192,posterior=8192,last_base=2048,last_model=0,last_mixed=2048;
    int targets[256]={0},weights[256]={0};Path paths[256];std::array<int,512> priors;
    State(){priors.fill(8192);reliability.fill(8192);}
};
inline State&state(){static State s;return s;}
inline void reset_pixel(){auto&s=state();s.pending=false;s.active=false;s.count=0;}
inline bool cell_enabled(){static bool b=std::getenv("SMEM_EXACT_CELL")!=nullptr;return b;}
inline bool point_enabled(){static bool b=std::getenv("SMEM_EXACT_POINT")!=nullptr;return b;}
inline void add_mass(State&s,int v,int mass){if(!mass)return;int j=0;for(;j<s.count;j++)if(s.targets[j]==v)break;
 if(j==s.count){assert(s.count<256);s.targets[j]=v;s.weights[j]=0;for(int e=0;e<4;e++)s.expert_mass[e][j]=0;s.count++;}
 s.weights[j]+=mass;if(learning_mode())s.expert_mass[s.collecting_mode][j]+=mass;
}
// 每个专家恰好贡献 4096 个单位，采用最大余数法分配，
// 在局部灰度跨度不同时仍保持各专家总权重相等。
inline void transported_expert(State&s,const Descriptor&cur,const Descriptor&past,int value){
 int off=past.direction>0?value-past.low:past.low+past.span-value;
 int center=cur.direction>0?cur.low*past.span+off*cur.span:(cur.low+cur.span)*past.span-off*cur.span;
 if(!cell_enabled()){int v=center>=0?(center+past.span/2)/past.span:-((-center+past.span/2)/past.span);add_mass(s,clip(v,0,255),4096);return;}
 int left=2*center-cur.span,right=2*center+cur.span,den=2*cur.span;
 int mass[256]={0},rem[256]={0},used=0;
 int first=clip(left/(2*past.span)-2,0,255),last=clip(right/(2*past.span)+2,0,255);
 for(int v=first;v<=last;v++){
  int lo=v==0?left:std::max(left,(2*v-1)*past.span);
  int hi=v==255?right:std::min(right,(2*v+1)*past.span);
  int overlap=std::max(0,hi-lo);mass[v]=overlap*4096/den;rem[v]=overlap*4096%den;used+=mass[v];
 }
 assert(used<=4096);while(used<4096){int v=int(std::max_element(rem+first,rem+last+1)-rem);assert(rem[v]>0);mass[v]++;rem[v]=-1;used++;}
 for(int v=first;v<=last;v++)add_mass(s,v,mass[v]);
}
struct History {
    std::unordered_map<uint64_t,int64_t> indices;
    int64_t reference=-1;int run=0;
};
struct DescriptorCacheEntry {int64_t pos=-1;bool valid=false;Descriptor descriptor;};
struct Memory {
    std::unique_ptr<std::array<DescriptorCacheEntry,4096>[]> cache;
    const void*identity=nullptr;int width=0,height=0;int64_t last=-1,next=0;History history[4];
    void reset(const void*id,int w,int h){identity=id;width=w;height=h;last=-1;next=0;
        if(!cache)cache.reset(new std::array<DescriptorCacheEntry,4096>[4]);
        for(int k=0;k<4;k++)for(auto &entry:cache[k])entry.pos=-1;
        for(auto &x:history){x.indices.clear();x.reference=-1;x.run=0;}state().priors.fill(8192);state().reliability.fill(8192);}
};
// 每个专家容量固定；因果邻域确定后完整描述符不再变更。
// 原始与归一化二维描述符的共用构造过程，不读取目标像素。
template<class Plane>void cache_2d(Memory&m,const Plane&p,int64_t pos){
 auto&a=m.cache[2][size_t(pos)&4095];auto&b=m.cache[3][size_t(pos)&4095];
 a.pos=b.pos=pos;a.descriptor=b.descriptor=Descriptor();a.valid=b.valid=false;
 int r=int(pos/m.width),c=int(pos%m.width);if(r<2||c<2||c+2>=m.width)return;
 const int dy[10]={0,-1,-1,-1,0,-2,-2,-2,-1,-1},dx[10]={-1,0,-1,1,-2,0,-1,1,-2,2};
 int v[10];for(int i=0;i<10;i++)v[i]=int(p.get(r+dy[i],c+dx[i]));a.valid=true;
 for(int i=0;i<10;i++){a.descriptor.hash^=uint64_t(v[i]);a.descriptor.hash*=1099511628211ULL;}
 auto&d=b.descriptor;d.low=*std::min_element(v,v+10);d.span=*std::max_element(v,v+10)-d.low;if(d.span<4)return;b.valid=true;
 for(int i=1;i<10;i++)if(v[i]!=v[0]){d.direction=v[0]<v[i]?1:-1;break;}
 for(int i=0;i<10;i++){int q=((d.direction>0?v[i]-d.low:d.low+d.span-v[i])*15+d.span/2)/d.span;d.hash^=uint64_t(q);d.hash*=1099511628211ULL;}
}
template<class Plane>bool cached_describe(Memory&m,const Plane&p,int64_t pos,int mode,Descriptor&d){
 auto &entry=m.cache[mode][size_t(pos)&4095];
 if(entry.pos!=pos){
  if(mode>=2)cache_2d(m,p,pos);
  else{entry.pos=pos;entry.descriptor=Descriptor();entry.valid=describe(p,pos,m.width,mode,entry.descriptor);}
 }
 d=entry.descriptor;return entry.valid;
}
template<class Plane>void process(Memory&m,const Plane&p,int64_t pos,bool prepare_current) {
    auto&s=state();int mask=0,maxrun=0,count=0;if(prepare_current)s.count=0;
    static const bool no_norm=std::getenv("SMEM_NO_NORM")!=nullptr;
    for(int mode=0;mode<4;mode++) {
        if(no_norm&&(mode&1))continue;
        auto&h=m.history[mode];Descriptor current;
        if(!cached_describe(m,p,pos,mode,current)){h.reference=-1;h.run=0;continue;}
        int64_t candidate=-1;Descriptor past;
        if(h.reference>=0&&h.reference+1<pos) {
            Descriptor follow;
            if(cached_describe(m,p,h.reference+1,mode,follow)&&follow.hash==current.hash){candidate=h.reference+1;past=follow;}
        }
        if(candidate<0) {
            auto found=h.indices.find(current.hash);
            if(found!=h.indices.end()&&found->second<pos) {
                candidate=found->second;
                if(!cached_describe(m,p,candidate,mode,past))candidate=-1;
            }
        }
        if(candidate>=0) {
            h.run=candidate==h.reference+1?h.run+1:1;
            int value=pixel(p,candidate,m.width);
            if(prepare_current){
                if(cell_enabled()||point_enabled()){
                    s.collecting_mode=mode;
                    int regime=0;
                    if(learning_mode()>=2){int scale=(mode&1)?(current.span<past.span?0:(current.span==past.span?1:2)):1;
                      int flip=(mode&1)?int(current.direction!=past.direction):0;
                      int run=h.run>=8?3:(h.run>=4?2:(h.run>=2?1:0));regime=(scale*2+flip)*4+run;}
                    s.expert_key[mode]=mode*32+regime;
                    if(mode&1)transported_expert(s,current,past,value);else add_mass(s,clip(value,0,255),4096);
                }else{if(mode&1)value=current.transport(past.canonical(value));s.targets[count]=clip(value,0,255);s.weights[count++]=1;}
                mask|=1<<mode;maxrun=std::max(maxrun,h.run);
            }
        } else h.run=0;
        h.reference=candidate;
        if(h.indices.size()>200000)h.indices.clear();
        h.indices[current.hash]=pos;
    }
    if(prepare_current) {
        int bucket=0;for(int run=maxrun;run>1&&bucket<15;run>>=1)bucket++;
        if(!(cell_enabled()||point_enabled()))s.count=count;s.key=(mask<<5)|bucket;s.mask=mask;s.pending=s.count>0;
        if(learning_mode())for(int j=0;j<s.count;j++){
          int64_t w=0;for(int e=0;e<4;e++)if(mask&(1<<e)){int reliability=s.reliability[s.expert_key[e]];
            if(learning_mode()==3)reliability=(reliability+8192)/2;w+=int64_t(s.expert_mass[e][j])*reliability;}
          assert(w>0&&w<=2147483647);s.weights[j]=int(w);
        }
    }
}
template<class Plane>void prepare(const Plane&p,int r,int c,int width,int height,int guess,int minimum,int maximum) {
    static const bool disabled=std::getenv("SMEM_DISABLE_MATCH")!=nullptr;if(disabled)return;
    static Memory memory;int64_t pos=int64_t(r)*width+c;
    if(memory.identity!=&p||memory.width!=width||memory.height!=height||pos<=memory.last)memory.reset(&p,width,height);
    memory.last=pos;
    for(;memory.next<pos;memory.next++)process(memory,p,memory.next,false);
    process(memory,p,pos,true);memory.next=pos+1;
    auto&s=state();s.guess=guess;s.min=minimum-guess;s.max=maximum-guess;
}
inline void begin(int minimum,int maximum) {
    auto&s=state();stats().symbols++;s.active=false;s.position=0;
    if(!s.pending||minimum!=s.min||maximum!=s.max)return;
    for(int i=0;i<s.count;i++){s.paths[i]=residual_head(minimum,maximum,clip(s.targets[i]-s.guess,minimum,maximum));s.paths[i].weight=s.weights[i];}
    s.base_likelihood=1u<<24;s.prior=s.priors[s.key];s.posterior=s.prior;s.active=true;stats().active_symbols++;
    if(s.mask&5)stats().raw_symbols++;if(s.mask&10)stats().normalized_symbols++;
}
inline int chance(int base,int type,int index) {
    auto&s=state();s.last_base=base;s.last_mixed=base;if(!s.active)return base;
    int64_t total=0,ones=0;
    for(int i=0;i<s.count;i++)if(s.paths[i].weight) {
        const auto&p=s.paths[i];assert(s.position<p.length);const auto&e=p.code[s.position];assert(e.type==type&&e.index==index);
        total+=p.weight;if(e.bit)ones+=p.weight;
    }
    s.last_model=total?(ones*4096+total/2)/total:0;if(!total)s.posterior=0;
    s.last_mixed=clip(int((int64_t(65536-s.posterior)*base+int64_t(s.posterior)*s.last_model+32768)/65536),1,4095);
    return s.last_mixed;
}
inline void observe(bool bit) {
    auto&s=state();auto&t=stats();t.bits++;
    int pb=bit?s.last_base:4096-s.last_base,pm=bit?s.last_mixed:4096-s.last_mixed;
    if(t.enabled){t.baseline_bits-=std::log2(pb/4096.);t.mixed_bits-=std::log2(pm/4096.);}
    if(!s.active)return;
    if(learning_mode())s.base_likelihood=std::max(uint32_t(1),uint32_t((uint64_t(s.base_likelihood)*pb+2048)/4096));
    int ps=bit?s.last_model:4096-s.last_model;
    int64_t denominator=int64_t(65536-s.posterior)*pb+int64_t(s.posterior)*ps;
    s.posterior=denominator?int((int64_t(s.posterior)*ps*65536+denominator/2)/denominator):0;
    for(int i=0;i<s.count;i++)if(s.paths[i].weight&&s.paths[i].code[s.position].bit!=bit)s.paths[i].weight=0;
    s.position++;if(s.position==1&&!bit)for(int q=0;q<s.count;q++)if(s.paths[q].weight)materialize(s.paths[q]);
}
inline void end() {
    auto&s=state();if(s.active) {
        s.priors[s.key]=clip((7*s.prior+s.posterior+4)/8,256,61440);
        int total=0;for(int i=0;i<s.count;i++)total+=s.paths[i].weight;
        if(total)stats().successful_symbols++;
        if(learning_mode())for(int e=0;e<4;e++)if(s.mask&(1<<e)){
          int surviving=0;for(int j=0;j<s.count;j++)if(s.paths[j].weight)surviving+=s.expert_mass[e][j];
          int &reliability=s.reliability[s.expert_key[e]];
          uint64_t yes=uint64_t(reliability)*surviving*4096;
          uint64_t no=uint64_t(65536-reliability)*s.base_likelihood;
          int posterior=int((yes*65536+(yes+no)/2)/(yes+no));
          reliability=clip((7*reliability+posterior+4)/8,256,61440);
        }
    }
    s.active=false;
}
} // namespace match_model
