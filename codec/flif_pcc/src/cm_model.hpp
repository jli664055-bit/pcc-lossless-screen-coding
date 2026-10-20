// 直接上下文模型：将因果邻域与已编码判决前缀哈希为自适应比特概率，
// 其拉伸值作为混合器输入。v17 含 20 个直接上下文与 4 个邻域一致性上下文；
// v18 另在规范坐标系中维护副本。
#pragma once
namespace cm_model {
inline int clip(int x,int a,int b){return std::max(a,std::min(b,x));}
inline bool enabled(){static bool b=[](){const char*p=std::getenv("SMEM_CM");return p&&std::atoi(p)>0;}();return b;}
inline int count_models(){static int n=[](){const char*p=std::getenv("SMEM_CM");return p?clip(std::atoi(p),0,20):0;}();return n;}
constexpr int MAXM=20;
inline int env_bits(const char*n,int d){const char*p=std::getenv(n);return p?std::max(12,std::min(24,std::atoi(p))):d;}
static const int BITS=env_bits("SMEM_CM_BITS",21);static const uint32_t MASK=(1u<<BITS)-1;constexpr int NCM=4;constexpr int CMX=20;constexpr int TOTAL=MAXM+NCM+CMX+NCM;
// v18 双坐标系：主上下文的规范坐标系副本，按优先级排列（PCC 模式 2）
constexpr int CSEL[CMX]={0,1,3,4,5,7,10,11,2,6,8,9,12,13,14,15,16,17,18,19};
inline int cmx(){static int n=[](){const char*p=std::getenv("SMEM_PCC_CMX");return p?clip(std::atoi(p),0,CMX):8;}();return n;}
inline bool dual(){return polarity_model::mode()==2;}
inline bool ncm_enabled(){static bool b=[](){const char*p=std::getenv("SMEM_NCM");return p&&std::atoi(p)>0;}();return b;}inline uint32_t mixh(uint64_t x){x^=x>>31;x*=0x7fb5d329728ea185ULL;x^=x>>27;x*=0x81dadef4bc2dd44dULL;x^=x>>33;return uint32_t(x);}
inline int recip(int n){static const auto t=[](){std::array<int,1100> a{};for(int i=0;i<1100;i++)a[i]=65536/(i+2);return a;}();return t[n<1099?n:1099];}
struct Slot{uint16_t p=32768;uint16_t n=0;};
struct State {
    bool ready=false,pending=false,active=false;
    std::vector<Slot> table;           // MAXM × 2^BITS
    uint64_t ctx[MAXM]={0},cctx[CMX]={0};uint32_t path=0,cpath=0;uint32_t idx[TOTAL]={0};int pr[TOTAL]={0};
    int limit=255;
    // 邻域颜色一致性状态（对比度不变上下文）
    int nbv[8]={0},guess=0,act=0,eq=0,pos=0;Path nbp[8];uint32_t alive=0;
    void init(){table.assign(size_t(polarity_model::mode()==2?TOTAL:MAXM+NCM)<<BITS,Slot());ready=true;const char*p=std::getenv("SMEM_CM_LIMIT");limit=p?std::atoi(p):255;}
};
inline State&state(){static State s;if(!s.ready&&enabled())s.init();return s;}
inline void reset(){auto&s=state();std::fill(s.table.begin(),s.table.end(),Slot());s.pending=s.active=false;}
inline void reset_pixel(){auto&s=state();s.pending=false;s.active=false;}
// 20 个直接上下文的键公式，在给定极性坐标系中求值。
inline void build_keys(int sigma,int W,int N,int NW,int NE,int WW,int NN,int NNE,int NWW,int NEE,int guess,int minimum,int maximum,
                       int online_pred,int match_value,int match_run,int palette_top,int c,uint64_t*out,int&act_out){
    if(sigma<0){auto f=[](int&v){v=255-v;};f(W);f(N);f(NW);f(NE);f(WW);f(NN);f(NNE);f(NWW);f(NEE);
        if(online_pred>=0)online_pred=255-online_pred;if(match_value>=0)match_value=255-match_value;if(palette_top>=0)palette_top=255-palette_top;
        int lo=minimum,hi=maximum;minimum=255-hi;maximum=255-lo;guess=255-guess;}
    int g=guess;auto q=[](int d){int a=std::abs(d);int v=a==0?0:a<=1?1:a<=3?2:a<=7?3:a<=15?4:a<=31?5:a<=63?6:7;return d<0?-v:v;};
    int gh=std::abs(W-WW)+std::abs(N-NW)+std::abs(NE-N),gv=std::abs(N-NN)+std::abs(W-NW)+std::abs(NE-NNE);
    int act=q(gh+gv);act_out=act;
    uint64_t range=uint64_t(minimum-g+300)<<9|uint64_t(maximum-g+300);
    uint64_t k[MAXM]={
        uint64_t(W)<<8|uint64_t(N),
        uint64_t(W)<<24|uint64_t(N)<<16|uint64_t(NW)<<8|uint64_t(NE),
        uint64_t(q(W-g)+8)<<12|uint64_t(q(N-g)+8)<<8|uint64_t(q(NW-g)+8)<<4|uint64_t(q(NE-g)+8),
        uint64_t(g)<<8|uint64_t(W),
        uint64_t(N)<<16|uint64_t(NN)<<8|uint64_t(NE),
        uint64_t(W)<<16|uint64_t(WW)<<8|uint64_t(NW),
        uint64_t(g)<<8|uint64_t(act+8),
        uint64_t(g)<<16|uint64_t(N)<<8|uint64_t(NE),
        uint64_t(q(W-N)+8)<<20|uint64_t(q(N-NE)+8)<<16|uint64_t(q(NW-N)+8)<<12|uint64_t(q(WW-W)+8)<<8|uint64_t(q(NN-N)+8)<<4|uint64_t(q(NNE-NE)+8),
        uint64_t(g),
        uint64_t(W)<<24|uint64_t(N)<<16|uint64_t(NWW)<<8|uint64_t(NEE),
        uint64_t((W+NE)/2)<<8|uint64_t(q(N-NW)+8),
        // 预测差上下文（其他预测器相对当前编码预测值之差）
        uint64_t(online_pred<0?255:q(online_pred-g)+8)<<8|uint64_t(q(W-g)+8)<<4|uint64_t(q(N-g)+8),
        uint64_t(match_value<0?255:clip(match_value-g,-128,127)+128)<<4|uint64_t(clip(match_run,0,15)),
        uint64_t(palette_top<0?511:clip(palette_top-g,-255,255)+256)<<8|uint64_t(q(W-N)+8),
        uint64_t(q(W+N-NW-g)+8)<<12|uint64_t(q(N+NE-NNE-g)+8)<<8|uint64_t(q(2*W-WW-g)+8)<<4|uint64_t(q(2*N-NN-g)+8),
        uint64_t(NN>>3)<<20|uint64_t(N>>3)<<15|uint64_t(W>>3)<<10|uint64_t(NW>>3)<<5|uint64_t(NE>>3),
        uint64_t(online_pred<0?255:clip(online_pred-g,-128,127)+128)<<8|uint64_t(act+8),
        uint64_t(match_value<0?255:clip(match_value-g,-128,127)+128)<<16|uint64_t(online_pred<0?255:q(online_pred-g)+8)<<8|uint64_t(palette_top<0?255:q(palette_top-g)+8),
        uint64_t(c)<<16|uint64_t(N)<<8|uint64_t(q(NN-N)+8)
    };
    for(int i=0;i<MAXM;i++)out[i]=(k[i]*0x9E3779B97F4A7C15ULL+uint64_t(i+1)*0x632BE59BD9B4E019ULL)^(range*0xD6E8FEB86659FD93ULL);
}
template<class Plane>void prepare(const Plane&p,int r,int c,int width,int guess,int minimum,int maximum,int online_pred=-1,int match_value=-1,int match_run=0,int palette_top=-1,int sigma=1){
    if(!enabled())return;auto&s=state();
    auto px=[&](int dy,int dx)->int{int rr=r+dy,cc=c+dx;if(rr<0||cc<0||cc>=width)return -1;return int(p.get(rr,cc));};
    int W=px(0,-1),N=px(-1,0),NW=px(-1,-1),NE=px(-1,1),WW=px(0,-2),NN=px(-2,0),NNE=px(-2,1),NWW=px(-1,-2),NEE=px(-1,2);
    if(W<0)W=N<0?128:N;if(N<0)N=W;if(NW<0)NW=N;if(NE<0)NE=N;if(WW<0)WW=W;if(NN<0)NN=N;if(NNE<0)NNE=NE;if(NWW<0)NWW=NW;if(NEE<0)NEE=NE;
    {int v[8]={W,N,NW,NE,WW,NN,NNE,NWW};for(int i=0;i<8;i++)s.nbv[i]=v[i];}   // 实际灰度值，用于残差路径
    s.guess=guess;
    // v18：PCC 模式 1 在极性规范坐标系中求主上下文；
    // 模式 2 保留在实际坐标系，并增加规范坐标系副本（双坐标系）。
    int act=0;
    build_keys(polarity_model::mode()==1?sigma:1,W,N,NW,NE,WW,NN,NNE,NWW,NEE,guess,minimum,maximum,online_pred,match_value,match_run,palette_top,c,s.ctx,act);
    if(dual()){uint64_t t[MAXM];int a2=0;build_keys(sigma,W,N,NW,NE,WW,NN,NNE,NWW,NEE,guess,minimum,maximum,online_pred,match_value,match_run,palette_top,c,t,a2);
        for(int i=0;i<cmx();i++)s.cctx[i]=t[CSEL[i]]^0x5bd1e9955bd1e995ULL;}
    s.act=clip(act,0,7);s.eq=(W==N)|(N==NE)<<1|(W==NW)<<2|(N==NW)<<3|(W==WW)<<4|(N==NN)<<5;
    s.pending=true;
}
inline void begin(int minimum,int maximum){auto&s=state();s.active=s.pending;s.pending=false;s.path=1;s.cpath=1;s.pos=0;s.alive=0;
    if(s.active&&ncm_enabled()&&minimum<maximum){for(int k=0;k<8;k++){s.nbp[k]=residual_head(minimum,maximum,clip(s.nbv[k]-s.guess,minimum,maximum));}s.alive=255;}}
inline int nb_state(const State&s,int k){if(!(s.alive>>k&1))return 0;const Path&p=s.nbp[k];if(s.pos>=p.length)return 0;return 1+p.code[s.pos].bit;}
// 返回写入 out 的输入个数（拉伸域浮点值，经调用方查表）。
inline int chance(int bitctx,int*out,bool flip=false,bool cflip=false,int cbitctx=0){
    auto&s=state();if(!s.active)return 0;int n=count_models();
    for(int i=0;i<n;i++){
        uint32_t h=mixh(s.ctx[i]^(uint64_t(s.path)<<40)^uint64_t(bitctx)<<32);
        s.idx[i]=(uint32_t(i)<<BITS)|(h&MASK);
        out[i]=s.pr[i]=clip(s.table[s.idx[i]].p>>4,1,4095);
    }
    if(ncm_enabled()){
        int st[8],ones=0,zeros=0;for(int k=0;k<8;k++){st[k]=nb_state(s,k);if(flip&&st[k])st[k]=3-st[k];ones+=st[k]==2;zeros+=st[k]==1;}
        uint64_t a=uint64_t(st[0])|uint64_t(st[1])<<2|uint64_t(st[2])<<4|uint64_t(st[3])<<6;
        uint64_t b=a|uint64_t(st[4])<<8|uint64_t(st[5])<<10|uint64_t(st[6])<<12|uint64_t(st[7])<<14;
        uint64_t k4[NCM]={a,b,a|uint64_t(s.eq)<<8|uint64_t(s.act)<<14,uint64_t(ones)<<8|uint64_t(zeros)<<4|uint64_t(s.act)|uint64_t(std::min(s.pos,15))<<12};
        for(int i=0;i<NCM;i++){
            uint32_t h=mixh((k4[i]+1)*0x9E3779B97F4A7C15ULL^uint64_t(i+101)<<56^uint64_t(bitctx)<<48);
            int j=MAXM+i;s.idx[j]=(uint32_t(j)<<BITS)|(h&MASK);out[n++]=s.pr[j]=clip(s.table[s.idx[j]].p>>4,1,4095);
        }
    }
    if(dual()){
        // 规范坐标系副本：表内保存规范比特统计，输出时换算回实际坐标系
        auto cv=[&](int q){return cflip?4096-q:q;};
        for(int i=0;i<cmx();i++){
            uint32_t h=mixh(s.cctx[i]^(uint64_t(s.cpath)<<40)^uint64_t(cbitctx)<<32);
            int j=MAXM+NCM+i;s.idx[j]=(uint32_t(j)<<BITS)|(h&MASK);s.pr[j]=clip(s.table[s.idx[j]].p>>4,1,4095);out[n++]=cv(s.pr[j]);
        }
        if(ncm_enabled()){
            int st[8],ones=0,zeros=0;for(int k=0;k<8;k++){st[k]=nb_state(s,k);if(cflip&&st[k])st[k]=3-st[k];ones+=st[k]==2;zeros+=st[k]==1;}
            uint64_t a=uint64_t(st[0])|uint64_t(st[1])<<2|uint64_t(st[2])<<4|uint64_t(st[3])<<6;
            uint64_t b=a|uint64_t(st[4])<<8|uint64_t(st[5])<<10|uint64_t(st[6])<<12|uint64_t(st[7])<<14;
            uint64_t k4[NCM]={a,b,a|uint64_t(s.eq)<<8|uint64_t(s.act)<<14,uint64_t(ones)<<8|uint64_t(zeros)<<4|uint64_t(s.act)|uint64_t(std::min(s.pos,15))<<12};
            for(int i=0;i<NCM;i++){
                uint32_t h=mixh((k4[i]+1)*0x9E3779B97F4A7C15ULL^uint64_t(i+201)<<56^uint64_t(cbitctx)<<48);
                int j=MAXM+NCM+CMX+i;s.idx[j]=(uint32_t(j)<<BITS)|(h&MASK);s.pr[j]=clip(s.table[s.idx[j]].p>>4,1,4095);out[n++]=cv(s.pr[j]);
            }
        }
    }
    return n;
}
inline void upd(State&s,int i,bool bit){Slot&sl=s.table[s.idx[i]];int target=bit?65535:0;   // 1/(n+2) 自适应，经倒数表实现并受上限约束
    sl.p=uint16_t(int(sl.p)+(((target-int(sl.p))*recip(sl.n))>>16));if(sl.n<s.limit)sl.n++;}
// bit：主模型比特（PCC 模式 1 下为规范比特）；abit：实际编码比特；cbit/cbitctx：规范坐标系（PCC 模式 2）
inline void observe(bool bit,int bitctx,bool abit,bool cbit=false,int cbitctx=0){
    auto&s=state();if(!s.active)return;int n=count_models();
    for(int t=0;t<n+(ncm_enabled()?NCM:0);t++)upd(s,t<n?t:MAXM+(t-n),bit);
    if(dual()){for(int i=0;i<cmx();i++)upd(s,MAXM+NCM+i,cbit);if(ncm_enabled())for(int i=0;i<NCM;i++)upd(s,MAXM+NCM+CMX+i,cbit);
        s.cpath=s.cpath*0x2545F491u+uint32_t(cbitctx*2+cbit+1);}
    s.path=s.path*0x2545F491u+uint32_t(bitctx*2+bit+1);
    if(ncm_enabled()){for(int k=0;k<8;k++)if(s.alive>>k&1){const Path&p=s.nbp[k];if(s.pos>=p.length||p.code[s.pos].bit!=abit)s.alive&=~(1u<<k);}s.pos++;
        if(s.pos==1&&!abit)for(int k=0;k<8;k++)if(s.alive>>k&1)materialize(s.nbp[k]);}
}
inline void end(){state().active=false;}
}
