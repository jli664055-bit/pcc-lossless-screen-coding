// 因果极性估计：以当前行邻近长水平恒值游程（或同列历史）的灰度为背景，
// 阈值 136 与 120 加 120 至 136 的滞回区间得到 σ∈{+1,−1}。仅读取已解码像素，
// 不传输侧信息。
#pragma once
namespace polarity_model {
inline int env_i(const char*n,int d){const char*p=std::getenv(n);return p?std::atoi(p):d;}
inline int mode(){static int m=env_i("SMEM_PCC",0);return m;}
inline int run_min(){static int v=env_i("SMEM_PCC_RUN",8);return v;}
inline int near_cols(){static int v=env_i("SMEM_PCC_NEAR",32);return v;}
inline int hi(){static int v=env_i("SMEM_PCC_HI",136);return v;}
inline int lo(){static int v=env_i("SMEM_PCC_LO",120);return v;}
struct State {
    const void*identity=nullptr;int width=0,height=0;int64_t next=0,last=-1;
    std::vector<int16_t> bgcol;int run_val=-1,run_len=0,run_start=0,rowbg=-1,rowbg_end=-1;
    int sig=1,cur=1;int8_t sat[4096];
    uint64_t flips=0,negative=0,total=0;
    void reset(const void*id,int w,int h){identity=id;width=w;height=h;next=0;last=-1;bgcol.assign(w,-1);
        run_val=-1;run_len=0;run_start=0;rowbg=-1;rowbg_end=-1;sig=1;cur=1;for(auto&x:sat)x=1;}
    ~State(){const char*f=std::getenv("SMEM_PCC_STATS");if(!f||!total)return;std::ofstream o(f);
        o<<"{\"pixels\":"<<total<<",\"negative\":"<<negative<<",\"switches\":"<<flips<<"}\n";}
};
inline State&state(){static State s;return s;}
// 计算位置 pos 的 σ，在其自身取值未知之前完成（幂等）。
inline int estimate(State&s,int64_t pos){
    int c=int(pos%s.width),B=-1;
    if(s.rowbg>=0&&c-s.rowbg_end<=near_cols())B=s.rowbg;
    else if(s.bgcol[c]>=0)B=s.bgcol[c];
    else if(s.rowbg>=0)B=s.rowbg;
    if(B>=hi())s.sig=1;else if(B>=0&&B<=lo())s.sig=-1;
    return s.sig;
}
inline void process(State&s,int64_t pos,int v){
    int c=int(pos%s.width);
    if(c==0){s.run_val=-1;s.run_len=0;s.rowbg=-1;s.rowbg_end=-1;}
    if(v==s.run_val)s.run_len++;else{s.run_val=v;s.run_len=1;s.run_start=c;}
    if(s.run_len>=run_min()){
        if(s.run_len==run_min())for(int cc=s.run_start;cc<=c;cc++)s.bgcol[cc]=int16_t(v);else s.bgcol[c]=int16_t(v);
        s.rowbg=v;s.rowbg_end=c;
    }
}
template<class Plane>void prepare(const Plane&p,int r,int c,int width,int height){
    if(!mode())return;auto&s=state();int64_t pos=int64_t(r)*width+c;
    if(s.identity!=&p||s.width!=width||s.height!=height||pos<=s.last)s.reset(&p,width,height);
    for(;s.next<pos;s.next++){int8_t g=int8_t(estimate(s,s.next));s.sat[s.next&4095]=g;
        process(s,s.next,int(p.get(int(s.next/width),int(s.next%width))));}
    int prev=s.cur;s.cur=estimate(s,pos);s.sat[pos&4095]=int8_t(s.cur);s.last=pos;
    s.total++;s.negative+=s.cur<0;s.flips+=s.cur!=prev;
}
// SMEM_PCC_FORCEPOS=1：保留全部双坐标系模型但强制 σ=+1，用于纯容量对照。
inline bool forcepos(){static bool b=env_i("SMEM_PCC_FORCEPOS",0)!=0;return b;}
inline int current(){return (mode()&&!forcepos())?state().cur:1;}
inline int at(int64_t pos){return (mode()&&!forcepos())?int(state().sat[pos&4095]):1;}
inline int flip(int v,int sigma){return (sigma<0&&v>=0&&v<=255)?255-v:v;}
} // namespace polarity_model
