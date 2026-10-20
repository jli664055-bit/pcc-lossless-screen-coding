// 值域直方图专家：按精确因果邻域记录至多 8 个灰度值及计数，
// 依残差路径一致性投影到当前判决，避免编码假设变化导致统计碎片化。
// v18 支持规范坐标系副本。
#pragma once
namespace hist_model {
inline int clip(int x,int a,int b){return std::max(a,std::min(b,x));}
inline int count_models(){static int n=[](){const char*p=std::getenv("SMEM_HIST");return p?clip(std::atoi(p),0,8):0;}();return n;}
constexpr int KMAIN=8;constexpr int KC=8;constexpr int K=KMAIN+KC;constexpr int SLOTS=8;
inline int canon_models(){static int n=[](){const char*p=std::getenv("SMEM_PCC_HC");return p?clip(std::atoi(p),0,KC):4;}();return (polarity_model::mode()==2&&count_models()>0)?n:0;}
inline int total_models(){return count_models()+canon_models();}
inline int env_bits(const char*n,int d){const char*p=std::getenv(n);return p?std::max(12,std::min(24,std::atoi(p))):d;}
static const int BITS=env_bits("SMEM_HIST_BITS",20);static const uint32_t MASK=(1u<<BITS)-1;
inline uint64_t mixh(uint64_t x){x^=x>>31;x*=0x7fb5d329728ea185ULL;x^=x>>27;x*=0x81dadef4bc2dd44dULL;x^=x>>33;return x;}
struct Node{uint32_t check=0;uint8_t v[SLOTS]={0};uint16_t c[SLOTS]={0};
    void observe(int value){
        int f=-1,minj=0;for(int j=0;j<SLOTS;j++){if(c[j]&&v[j]==value)f=j;if(c[j]<c[minj])minj=j;}
        if(f<0){f=minj;v[f]=uint8_t(value);c[f]=0;}
        c[f]+=2;if(c[f]>=60000)for(auto&x:c)x>>=1;
        // 竞争项的轻度衰减保持节点自适应性
        for(int j=0;j<SLOTS;j++)if(j!=f&&c[j]>8)c[j]-=c[j]>>5;
    }};
struct State{
    bool ready=false,pending=false,active=false;std::vector<Node> table;
    const void*identity=nullptr;int width=0,height=0;int64_t next=0,last=-1;
    int guess=0,pos=0,nvals[K]={0},vals[K][SLOTS],cnt[K][SLOTS];Path paths[K][SLOTS];uint32_t alive[K]={0};
    int last_p[K]={0};int extra_at[4096];int sig_main=1,sig_c=1;
    void init(){table.assign(size_t(total_models())<<BITS,Node());ready=true;}
};
inline State&state(){static State s;if(!s.ready&&count_models()>0)s.init();return s;}
inline void reset_pixel(){auto&s=state();s.pending=s.active=false;}
// 在极性 σ 对应坐标系中，对邻域值求值的 8 个键公式。
template<class Plane>void build(const Plane&p,int r,int c,int width,int extra,int sigma,uint64_t*k){
    auto px=[&](int dy,int dx)->int{int rr=r+dy,cc=c+dx;if(rr<0||cc<0||cc>=width)return 256;int v=int(p.get(rr,cc));return sigma<0?255-v:v;};
    if(sigma<0&&extra>=0&&extra<=255)extra=255-extra;
    uint64_t W=px(0,-1),N=px(-1,0),NW=px(-1,-1),NE=px(-1,1),WW=px(0,-2),NN=px(-2,0),NNE=px(-2,1),NWW=px(-1,-2),NEE=px(-1,2),NNW=px(-2,-1);
    k[0]=W|N<<9|NW<<18|NE<<27|1ULL<<60;
    k[1]=mixh(W|N<<9|NW<<18|NE<<27|WW<<36|NN<<45)^mixh(NNE|NWW<<9|NEE<<18|NNW<<27|2ULL<<60);
    k[2]=W|N<<9|3ULL<<60;
    k[3]=(W>>2)|(N>>2)<<8|(NW>>2)<<16|(NE>>2)<<24|(WW>>2)<<32|(NN>>2)<<40|4ULL<<60;
    int grad=int(W)+int(N)-int(NW);grad=grad<0?0:grad>255?255:grad;
    k[4]=uint64_t(grad)|uint64_t(W==N)<<9|uint64_t(N==NE)<<10|uint64_t(W==NW)<<11|5ULL<<60;
    k[5]=N|NE<<9|NN<<18|NNE<<27|6ULL<<60;
    k[6]=W|WW<<9|NW<<18|NWW<<27|7ULL<<60;
    k[7]=uint64_t(extra<0?511:extra)|W<<9|N<<18|8ULL<<60;   // 含西、北邻域的在线 L1 预测
}
template<class Plane>void keys(const Plane&p,int r,int c,int width,uint64_t*k,int extra,int sm,int sc){
    build(p,r,c,width,extra,sm,k);
    if(canon_models()){uint64_t t[KMAIN];build(p,r,c,width,extra,sc,t);const int sel[KC]={0,1,2,5,3,4,6,7};for(int i=0;i<canon_models();i++)k[KMAIN+i]=t[sel[i]];}
}
inline int model_sigma(int i,int sm,int sc){return i<KMAIN?sm:sc;}
inline int slot_index(int i){return i<KMAIN?i:count_models()+(i-KMAIN);}   // 所属表区域
inline Node&slot(State&s,int i,uint64_t key,bool create,bool&found){
    uint64_t h=mixh(key*0x9E3779B97F4A7C15ULL+uint64_t(i));Node&n=s.table[(size_t(slot_index(i))<<BITS)|(h&MASK)];
    uint32_t chk=uint32_t(h>>40)|1u;found=n.check==chk;
    if(!found&&create){n=Node();n.check=chk;found=true;}
    return n;
}
// 模型编号：0 至 count_models()-1（主模型），KMAIN 至 KMAIN+canon_models()-1（规范副本）。
template<class F>inline void each_model(F f){for(int i=0;i<count_models();i++)f(i);for(int i=0;i<canon_models();i++)f(KMAIN+i);}
inline int main_sigma(int sigma){return polarity_model::mode()==1?sigma:1;}
template<class Plane>void prepare(const Plane&p,int r,int c,int width,int height,int guess,int extra=-1,int sigma=1){
    int n=count_models();if(!n)return;auto&s=state();int64_t pos=int64_t(r)*width+c;
    if(s.identity!=&p||s.width!=width||s.height!=height||pos<=s.last){std::fill(s.table.begin(),s.table.end(),Node());s.identity=&p;s.width=width;s.height=height;s.next=0;}
    s.last=pos;
    for(;s.next<pos;s.next++){int rr=int(s.next/width),cc=int(s.next%width);int sg=polarity_model::at(s.next),sm=main_sigma(sg);
        uint64_t k[K];keys(p,rr,cc,width,k,s.extra_at[s.next&4095],sm,sg);int v=int(p.get(rr,cc));
        each_model([&](int i){bool f;slot(s,i,k[i],true,f).observe(polarity_model::flip(v,model_sigma(i,sm,sg)));});}
    s.sig_main=main_sigma(sigma);s.sig_c=sigma;
    s.extra_at[pos&4095]=extra;uint64_t k[K];keys(p,r,c,width,k,extra,s.sig_main,s.sig_c);s.guess=guess;
    each_model([&](int i){bool f;Node&nd=slot(s,i,k[i],false,f);s.nvals[i]=0;
        if(f)for(int j=0;j<SLOTS;j++)if(nd.c[j]){s.vals[i][s.nvals[i]]=nd.v[j];s.cnt[i][s.nvals[i]++]=nd.c[j];}});
    s.pending=true;
}
inline void begin(int minimum,int maximum){
    auto&s=state();s.active=s.pending&&count_models()>0&&minimum<maximum;s.pending=false;s.pos=0;if(!s.active)return;
    each_model([&](int i){s.alive[i]=0;int sg=model_sigma(i,s.sig_main,s.sig_c);
        for(int j=0;j<s.nvals[i];j++){int res=polarity_model::flip(s.vals[i][j],sg)-s.guess;   // 已存储值（所在坐标系）换算为实际值
            if(res<minimum||res>maximum)continue;
            s.paths[i][j]=residual_head(minimum,maximum,res);s.alive[i]|=1u<<j;}});
}
// out：实际坐标系概率（1 至 4095），先主模型后规范副本；act：激活位掩码。
inline int chance(int*out,int&act){
    auto&s=state();act=0;if(!s.active)return 0;int o=0;
    each_model([&](int i){
        int64_t ones=0,total=0;
        for(int j=0;j<s.nvals[i];j++)if(s.alive[i]>>j&1){const Path&p=s.paths[i][j];if(s.pos>=p.length)continue;total+=s.cnt[i][j];if(p.code[s.pos].bit)ones+=s.cnt[i][j];}
        if(total>0){act|=1<<o;out[o]=clip(int(((ones*2+1)*4096)/(total*2+2)),1,4095);}   // 轻度加性平滑
        else out[o]=2048;
        s.last_p[i]=out[o];o++;});
    return o;
}
inline void observe(bool bit){
    auto&s=state();if(!s.active)return;
    each_model([&](int i){for(int j=0;j<s.nvals[i];j++)if(s.alive[i]>>j&1){const Path&p=s.paths[i][j];if(s.pos>=p.length||p.code[s.pos].bit!=bit)s.alive[i]&=~(1u<<j);}});
    s.pos++;
    if(s.pos==1&&!bit)each_model([&](int i){for(int j=0;j<s.nvals[i];j++)if(s.alive[i]>>j&1)materialize(s.paths[i][j]);});
}
inline void end(){state().active=false;}
}
