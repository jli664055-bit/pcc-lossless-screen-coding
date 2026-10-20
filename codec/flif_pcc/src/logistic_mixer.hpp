// 门控 logistic 混合与 APM/SSE。
// 三路混合器按比特位置与上下文状态选择权重，末级混合合并三路输出，两级 APM 校正。
// SMEM_LMIX 控制层数：0 为前序串联编码器，4 为三路门控混合加末级混合。
#pragma once
namespace lmix {
inline int env_int(const char*name,int fallback){const char*p=std::getenv(name);return p?std::atoi(p):fallback;}
inline double env_double(const char*name,double fallback){const char*p=std::getenv(name);return p?std::atof(p):fallback;}
// 0 关闭（v12），1 单混合器，2 混合器加 APM，3 两路混合器加末级混合与 APM
inline int mode(){static int m=env_int("SMEM_LMIX",0);return m;}
inline int clip(int x,int a,int b){return std::max(a,std::min(b,x));}

struct Tables {
    float st[4096];      // ln(p/(1−p))，p=i/4096
    int sti[4096];       // 256·ln(p/(1−p))，截断至 ±2047（APM 域）
    Tables(){
        for(int i=0;i<4096;i++){
            double p=(std::max(1,std::min(4095,i)))/4096.;
            double s=std::log(p/(1-p));st[i]=float(s);
            sti[i]=clip(int(std::lround(256*s)),-2047,2047);
        }
    }
};
inline const Tables&tables(){static Tables t;return t;}
inline float stretch(int p){return tables().st[clip(p,1,4095)];}
// squash 用 [-16,16] 上 4097 项查表加线性插值，逐比特不调用 exp
inline int squash_f(float x){
    static const auto t=[](){std::array<float,4098> a{};for(int i=0;i<4098;i++){double v=(i-2048)/128.;a[i]=float(4096./(1.+std::exp(-v)));}return a;}();
    if(x>15.99f)x=15.99f;if(x<-15.99f)x=-15.99f;
    float f=(x+16.f)*128.f;int i=int(f);float w=f-i;
    return clip(int(t[i]+(t[i+1]-t[i])*w+.5f),1,4095);
}
inline int squash_i(int x){ // x 以 256·ln 为单位
    if(x>2047)x=2047;if(x<-2047)x=-2047;
    return clip(int(4096./(1.+std::exp(-x/256.))+.5),1,4095);
}

enum {IN_BIAS,IN_BASE,IN_CHAIN,IN_STRUCT,IN_MATCH,IN_SOFT,IN_PAL,IN_ONLINE,IN_CM,IN_N=IN_CM+64};

struct Mixer {
    int nsets=0;std::vector<float> w;std::vector<uint32_t> n;
    float lr0=0.02f,lrmin=0.002f;
    int set=0,p=2048;float dot=0;
    void init(int sets,const float*initial){
        nsets=sets;w.assign(size_t(sets)*IN_N,0.f);n.assign(sets,0);
        for(int s=0;s<sets;s++)for(int i=0;i<IN_N;i++)w[size_t(s)*IN_N+i]=initial[i];
    }
    int mix(const float*x,int s){
        set=s;const float*ww=&w[size_t(s)*IN_N];float d=0;
        for(int i=0;i<IN_N;i++)d+=ww[i]*x[i];
        dot=d;p=squash_f(d);return p;
    }
    void update(const float*x,bool bit){
        float err=(bit?1.f:0.f)-p/4096.f;
        uint32_t &c=n[set];
        float lr=lr0/(1.f+float(c)/1024.f);if(lr<lrmin)lr=lrmin;
        if(c<0xffffffffu)c++;
        float*ww=&w[size_t(set)*IN_N];
        for(int i=0;i<IN_N;i++)ww[i]+=lr*err*x[i];
    }
};

// 对首层输出（拉伸域）做小型末级混合，结构类似 lpaq 的 mixer2。
struct Final {
    std::vector<float> w;int set=0,p=2048,k=3;float x[5];
    void init(int sets,int inputs){k=inputs+1;w.assign(size_t(sets)*k,0.f);for(int s=0;s<sets;s++)for(int i=0;i<inputs;i++)w[s*k+i]=1.f/inputs;}
    int mix(const float*in,int s){set=s;for(int i=0;i<k-1;i++)x[i]=in[i];x[k-1]=.3f;float d=0;for(int i=0;i<k;i++)d+=w[s*k+i]*x[i];p=squash_f(d);return p;}
    void update(bool bit,float lr){float err=(bit?1.f:0.f)-p/4096.f;for(int i=0;i<k;i++)w[set*k+i]+=lr*err*x[i];}
};

// 自适应概率映射，33 个插值档位。
struct APM {
    std::vector<uint16_t> t;int index=0,rate=7;
    void init(int n,int r){rate=r;t.resize(size_t(n)*33);
        for(int i=0;i<n;i++)for(int j=0;j<33;j++)t[size_t(i)*33+j]=uint16_t(squash_i((j-16)*128)*16);}
    int pp(int pr,int cx){
        int s=tables().sti[clip(pr,1,4095)]+2048; // 取值 1 至 4095
        int lo=s>>7,wt=s&127;index=cx*33+lo+(wt>>6);
        return clip(int((t[size_t(cx)*33+lo]*(128-wt)+t[size_t(cx)*33+lo+1]*wt)>>11),1,4095);
    }
    void update(bool bit){int g=(bit<<16)+(bit<<rate)-bit-bit;t[index]+=(g-t[index])>>rate;}
};

inline double env_final_lr(){static double v=env_double("SMEM_LMIX_FLR",0.002);return v;}
inline int bit_context(int type,int index){
    if(type==0)return 0;if(type==1)return 1;
    if(type==2)return 2+clip(index,0,15);
    return 18+clip(index,0,13);
}

struct BitStats {
    double cost[32]={0},chain[32]={0},base[32]={0};uint64_t n[32]={0};
    ~BitStats(){const char*f=std::getenv("SMEM_LMIX_STATS");if(!f)return;std::ofstream o(f);
        o<<"bitctx,n,final_bits,chain_bits,base_bits\n";for(int i=0;i<32;i++)if(n[i])o<<i<<","<<n[i]<<","<<cost[i]<<","<<chain[i]<<","<<base[i]<<"\n";}
};
inline BitStats&bitstats(){static BitStats b;return b;}
struct State {
    bool ready=false;Mixer m1,m2,m3;Final fin;APM a1,a2;
    int last_base=2048,last_chain=2048,sigbank=0;float x[IN_N];int bitctx=0,mask=0,ctx2=0,p_mix=2048,p_final=2048,apm_ctx=0;
    void init(){
        float initial[IN_N]={0};initial[IN_CHAIN]=1.f; // 初始为 v12 输出
        m1.lr0=float(env_double("SMEM_LMIX_LR",0.02));m1.lrmin=float(env_double("SMEM_LMIX_LRMIN",0.002));
        m2.lr0=m3.lr0=m1.lr0;m2.lrmin=m3.lrmin=m1.lrmin;
        m1.init(32*64*2,initial);m2.init(32*16*4,initial);m3.init(32*256,initial);fin.init(32*4,mode()>=4?3:2);
        a1.init(32*64,7);a2.init(32*256,7);ready=true;
    }
};
inline State&state(){static State s;if(!s.ready)s.init();return s;}

// 每个二值判决调用一次，输入全部专家概率。
// 激活位：0 结构、1 匹配、2 软上下文、3 调色板、4 在线；概率取值 1 至 4095（满量程 4096）。
inline int mix(int base,int chained,const int*pe,int active,int type,int index,int run_bucket,int online_key,const int*cm=nullptr,int ncm=0,int ctx3=0){
    auto&s=state();
    s.bitctx=bit_context(type,index);s.mask=active;s.last_base=base;s.last_chain=chained;
    s.x[IN_BIAS]=.25f;s.x[IN_BASE]=stretch(base);s.x[IN_CHAIN]=stretch(chained);
    for(int e=0;e<5;e++)s.x[IN_STRUCT+e]=(active>>e&1)?stretch(pe[e]):0.f;
    for(int i=0;i<64;i++)s.x[IN_CM+i]=(i<ncm&&cm[i]!=2048)?stretch(cm[i]):0.f;
    // 第 1 组：比特位置 × 专家激活模式（5 位）× 链式输出置信标志
    // v18：PCC 模式 2 下深底像素选用独立权重组（偏移设计使 PCC=0 时输出完全一致）
    int p1=s.m1.mix(s.x,s.bitctx*64+(active&31)*2+(std::abs(s.x[IN_CHAIN])>4.f)+s.sigbank*2048);
    int pm=p1;
    if(mode()>=3){
        int p2=s.m2.mix(s.x,(s.bitctx*16+clip(online_key,0,15))*4+clip(run_bucket,0,3));
        float in[3]={stretch(p1),stretch(p2),0.f};
        if(mode()>=4)in[2]=stretch(s.m3.mix(s.x,s.bitctx*256+(ctx3&255)));
        pm=s.fin.mix(in,s.bitctx*4+clip(run_bucket,0,3));
    }
    s.p_mix=pm;
    if(mode()<2)return s.p_final=pm;
    // SSE 级：上下文为比特位置 × 激活模式；二级为比特位置 × 量化基准概率
    int q1=s.a1.pp(pm,s.bitctx*64+(active&31)*2+(run_bucket>0));
    int bq=clip(tables().sti[clip(base,1,4095)]/256+8,0,15);
    int q2=s.a2.pp(pm,s.bitctx*256+bq*16+clip(online_key,0,15));
    int out=(2*pm+3*q1+3*q2+4)>>3;
    return s.p_final=clip(out,1,4095);
}
inline void update(bool bit){
    auto&s=state();
    static const bool st=std::getenv("SMEM_LMIX_STATS")!=nullptr;
    if(st){auto&b=bitstats();auto c=[&](int p){return -std::log2((bit?p:4096-p)/4096.);};b.n[s.bitctx]++;b.cost[s.bitctx]+=c(s.p_final);b.chain[s.bitctx]+=c(s.last_chain);b.base[s.bitctx]+=c(s.last_base);}
    s.m1.update(s.x,bit);
    if(mode()>=3){s.m2.update(s.x,bit);if(mode()>=4)s.m3.update(s.x,bit);s.fin.update(bit,float(env_final_lr()));}
    if(mode()>=2){s.a1.update(bit);s.a2.update(bit);}
}
} // namespace lmix
