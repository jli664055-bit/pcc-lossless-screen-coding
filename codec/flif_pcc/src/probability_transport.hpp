// 概率迁移扩展：在 FLIF v0.4 残差二值化上加入对比度与极性归一化的续接分布。
// 原残差二值化与符号编码来自 FLIF v0.4（Jon Sneyers、Pieter Wuille，Apache-2.0
// symbol_enc.hpp），本文件只处理概率来源与迁移。
#pragma once
#include <array>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cassert>

namespace probability_transport {
// v17：仅概率型专家只在最终编码轮需要（
// 编码器的 MANIAC 学习轮使用不带这些钩子的独立编码器）。
inline bool &coding_pass(){static bool b=true;return b;}
struct Event {uint8_t type,index,bit;};
struct Path {
    std::array<Event,40> code;
    int length=0,weight=0;
    int lazy_min=0,lazy_max=0,lazy_value=0;bool lazy=false;
    void put(int bit,int type,int index=0) {
        assert(length<int(code.size()));
        code[length++]={uint8_t(type),uint8_t(index),uint8_t(bit)};
    }
};
inline int logarithm(int x){int n=0;while(x>>=1)n++;return n;}
inline Path residual_path(int minimum,int maximum,int value) {
    assert(minimum<=value && value<=maximum && minimum<=0 && maximum>=0);
    Path p;
    if(minimum==maximum)return p;
    if(!value){p.put(1,0);return p;}
    p.put(0,0);int sign=value>0;
    if(maximum>0 && minimum<0)p.put(sign,1);
    if(sign)minimum=1;else maximum=-1;
    int a=std::abs(value),e=logarithm(a);
    int amin=sign?std::abs(minimum):std::abs(maximum);
    int amax=sign?std::abs(maximum):std::abs(minimum);
    int emax=logarithm(amax),i=logarithm(amin);
    while(i<emax) {
        if((1<<(i+1))>amax)break;
        p.put(i==e,2,(i<<1)+sign);
        if(i==e)break;
        i++;
    }
    int have=1<<e,left=have-1;
    for(int pos=e;pos>0;) {
        int bit=1;left^=1<<(--pos);
        int minabs1=have|(1<<pos),maxabs0=have|left;
        if(minabs1>amax)bit=0;
        else if(maxabs0>=amin){bit=(a>>pos)&1;p.put(bit,3,pos);}
        have|=bit<<pos;
    }
    return p;
}
// v17 提速：此刻只构造零标志事件，materialize() 再构造其余部分，
// 待确认残差非零后展开（多数屏幕像素停在比特 0）。
inline Path residual_head(int minimum,int maximum,int value){
    Path p;p.lazy_min=minimum;p.lazy_max=maximum;p.lazy_value=value;p.lazy=true;
    if(minimum==maximum){p.lazy=false;return p;}
    p.put(value==0,0);return p;
}
inline void materialize(Path &p){if(!p.lazy)return;int w=p.weight;p=residual_path(p.lazy_min,p.lazy_max,p.lazy_value);p.weight=w;}
#include "match_model.hpp"
#include "soft_model.hpp"
#include "palette_model.hpp"
#include "online_model.hpp"
#include "logistic_mixer.hpp"
#include "polarity_model.hpp"
#include "cm_model.hpp"
#include "hist_model.hpp"
struct Stats {
    bool enabled=std::getenv("SMEM_STATS_PATH")!=nullptr;
    uint64_t symbols=0,modeled_symbols=0,bits=0,modeled_bits=0;
    double baseline_bits=0,mixed_bits=0;
    ~Stats() {
        const char *path=std::getenv("SMEM_STATS_PATH");if(!path)return;
        std::ofstream f(path);
        f<<"{\"coded_symbols\":"<<symbols<<",\"modeled_symbols\":"<<modeled_symbols
         <<",\"bits\":"<<bits<<",\"modeled_bits\":"<<modeled_bits
         <<",\"shadow_baseline_bits\":"<<baseline_bits<<",\"mixed_bits\":"<<mixed_bits<<"}\n";
    }
};
inline Stats &stats(){static Stats s;return s;}
struct State {
    bool pending=false,active=false;
    uint64_t key=0;
    int guess=0,expected_min=0,expected_max=0,position=0;
    int posterior=0,prior=0,last_base=2048,last_structure=0,last_mixed=2048;
    std::vector<std::pair<int,int>> values;
    std::array<Path,32> paths;
    int count=0;
    std::unordered_map<uint64_t,int> priors;
};
inline State &state(){static State s;return s;}
inline void reset_pixel(){hist_model::reset_pixel();cm_model::reset_pixel();soft_model::reset_pixel();match_model::reset_pixel();palette_model::reset_pixel();online_model::reset_pixel();auto &s=state();s.pending=false;s.active=false;s.values.clear();}
inline void reset_history(){auto &s=state();s.priors.clear();reset_pixel();if(coding_pass()){if(lmix::mode())lmix::state().init();if(cm_model::enabled())cm_model::reset();}}
inline void prepare(uint64_t key,const std::vector<std::pair<int,int>> &values) {
    auto &s=state();s.key=key;s.values=values;s.pending=!values.empty();
}
inline void set_bounds(int guess,int minimum,int maximum) {
    auto &s=state();s.guess=guess;s.expected_min=minimum-guess;s.expected_max=maximum-guess;
}
inline void begin(int minimum,int maximum) {
    cm_model::begin(minimum,maximum);hist_model::begin(minimum,maximum);soft_model::begin(minimum,maximum);match_model::begin(minimum,maximum);palette_model::begin(minimum,maximum);online_model::begin(minimum,maximum);
    auto &s=state();stats().symbols++;s.active=false;s.position=0;s.count=0;
    static const bool disabled=std::getenv("SMEM_DISABLE_MIXTURE")!=nullptr;
    if(!s.pending || disabled || minimum!=s.expected_min || maximum!=s.expected_max){s.pending=false;return;}
    s.pending=false;
    for(auto value:s.values) {
        int residual=std::max(minimum,std::min(maximum,value.first-s.guess));
        if(value.second<=0)continue;
        assert(s.count<int(s.paths.size()));
        s.paths[s.count]=residual_head(minimum,maximum,residual);
        s.paths[s.count++].weight=value.second;
    }
    if(!s.count)return;
    if(s.priors.size()>200000)s.priors.clear();
    auto found=s.priors.find(s.key);s.prior=found==s.priors.end()?8192:found->second;
    s.posterior=s.prior;s.active=true;stats().modeled_symbols++;
}
// v17：从各专家汇集直接上下文模型所需的辅助信息。
template<class Plane>void cm_hook(const Plane&p,int r,int c,int width,int guess,int minimum,int maximum){
    if(!cm_model::enabled())return;
    auto&on=online_model::state();int op=on.pending?on.pred:-1;
    auto&m=match_model::state();int mv=-1,best=0;
    if(m.pending)for(int j=0;j<m.count;j++)if(m.weights[j]>best){best=m.weights[j];mv=m.targets[j];}
    int run=m.pending?(m.key&31):0;
    auto&pa=palette_model::state();int pt=-1;best=0;
    if(pa.pending)for(int j=0;j<pa.count;j++)if(pa.weights[j]>best){best=pa.weights[j];pt=pa.values[j];}
    cm_model::prepare(p,r,c,width,guess,minimum,maximum,op,mv,run,pt,polarity_model::current());
}
// v17：对全部专家比特概率做可选的门控 logistic 混合。
inline int expert_p(int p){return p<=0?1:(p>=4096?4095:p);}
// v18：当前编码判决为深底极性下的残差符号，
// 规范比特为实际比特的补；在 finish() 中设置，在 observe() 中使用。
inline bool &pcc_flip(){static bool f=false;return f;}
inline bool &pcc_sflip(){static bool f=false;return f;}
inline int &pcc_cbitctx(){static int c=0;return c;}
inline int finish(int base,int chained,int type,int index){
    if(!lmix::mode())return chained;
    const int sigma=polarity_model::current(),pmode=polarity_model::mode();
    const bool sflip=sigma<0&&type==1;            // 深底极性下的符号判决
    const int cindex=(sigma<0&&type==2)?index^1:index;   // 指数判决在编号中携带符号
    const bool flip=pmode==1&&sflip;pcc_flip()=flip;pcc_sflip()=pmode==2&&sflip;pcc_cbitctx()=lmix::bit_context(type,cindex);
    if(pmode==1)index=cindex;
    static const bool bank=polarity_model::env_i("SMEM_PCC_BANK",1)!=0;
    lmix::state().sigbank=(bank&&pmode==2&&sigma<0)?1:0;
    auto cv=[&](int p){return flip?4096-p:p;};    // 实际坐标系概率换算到规范坐标系
    auto &s=state();int pe[5]={2048,2048,2048,2048,2048};int active=0;
    // 候选路径全部被排除的专家后验为 0，视为未激活。
    if(s.active && s.posterior>0){pe[0]=expert_p(s.last_structure);active|=1;}
    auto &m=match_model::state();if(m.active && m.posterior>0){pe[1]=expert_p(m.last_model);active|=2;}
    auto &so=soft_model::state();if(so.active && so.posterior>0){pe[2]=expert_p(so.last_model);active|=4;}
    auto &pa=palette_model::state();if(pa.active && pa.posterior>0){pe[3]=expert_p(pa.last_model);active|=8;}
    auto &on=online_model::state();if(on.active){pe[4]=expert_p(on.last_model);active|=16;}
    int run=0;if(m.active){int b=m.key&31;run=b==0?0:(b<2?1:(b<3?2:3));}
    int key=on.active?on.key:12+(m.active?1:0);
    for(int e=0;e<5;e++)pe[e]=cv(pe[e]);base=cv(base);chained=cv(chained);
    int cm[64];int ncm=cm_model::chance(lmix::bit_context(type,index),cm,flip,pcc_sflip(),pcc_cbitctx());
    {int h[16],ha=0;int nh=hist_model::chance(h,ha);for(int i=0;i<nh;i++)cm[48+i]=(ha>>i&1)?cv(h[i]):2048;if(nh){for(int i=ncm;i<48;i++)cm[i]=2048;ncm=48+nh;}}
    int ctx3=0;{int ha=0;for(int i=0;i<4;i++)if(ncm>48+i&&cm[48+i]!=2048)ha|=1<<i;
        int ones=0,zeros=0;auto&cs=cm_model::state();if(cs.active&&cm_model::ncm_enabled())for(int k=0;k<8;k++){int t=cm_model::nb_state(cs,k);if(flip&&t)t=3-t;ones+=t==2;zeros+=t==1;}
        ctx3=ha|(ones>0)<<4|(zeros>0)<<5|(ones>zeros)<<6|((active&2)?1:0)<<7;}
    return cv(lmix::mix(base,chained,pe,active,type,index,run,key,cm,ncm,ctx3));
}
// 仅用于 v18 可视化（SMEM_COSTMAP 与 SMEM_SIGMAMAP）：逐像素码长与极性。
// 退出时写入每像素 float32 码长与 int8 极性，码流不变。
struct Viz{int w=0,h=0;int64_t pos=-1;int lastp=2048;std::vector<float> cost;std::vector<int8_t> sig;
    ~Viz(){const char*a=std::getenv("SMEM_COSTMAP");if(a&&!cost.empty()){std::ofstream f(a,std::ios::binary);f.write((const char*)&w,4);f.write((const char*)&h,4);f.write((const char*)cost.data(),cost.size()*4);}
           const char*b=std::getenv("SMEM_SIGMAMAP");if(b&&!sig.empty()){std::ofstream f(b,std::ios::binary);f.write((const char*)&w,4);f.write((const char*)&h,4);f.write((const char*)sig.data(),sig.size());}}};
inline Viz&viz(){static Viz v;return v;}
inline bool viz_on(){static bool b=std::getenv("SMEM_COSTMAP")||std::getenv("SMEM_SIGMAMAP");return b;}
inline void viz_pixel(int r,int c,int w,int h){if(!viz_on())return;auto&v=viz();if(v.w!=w||v.h!=h){v.w=w;v.h=h;v.cost.assign(size_t(w)*h,0.f);v.sig.assign(size_t(w)*h,1);}
    v.pos=int64_t(r)*w+c;v.sig[v.pos]=int8_t(polarity_model::current());}
inline int chance(int base,int type,int index) {
    auto &s=state();s.last_base=base;s.last_mixed=base;
    if(!s.active)return viz().lastp=finish(base,online_model::chance(palette_model::chance(soft_model::chance(match_model::chance(base,type,index),type,index),type,index),type,index),type,index);
    int total=0,ones=0;
    for(int j=0;j<s.count;j++) {
        const auto &p=s.paths[j];if(!p.weight)continue;
        assert(s.position<p.length);
        const auto &event=p.code[s.position];
        assert(event.type==type && event.index==index);
        total+=p.weight;if(event.bit)ones+=p.weight;
    }
    s.last_structure=total?(ones*4096+total/2)/total:0;
    if(!total)s.posterior=0;
    int mixed=int((int64_t(65536-s.posterior)*base+int64_t(s.posterior)*s.last_structure+32768)/65536);
    s.last_mixed=std::max(1,std::min(4095,mixed));
    return viz().lastp=finish(base,online_model::chance(palette_model::chance(soft_model::chance(match_model::chance(s.last_mixed,type,index),type,index),type,index),type,index),type,index);
}
inline void observe(bool bit) {
    if(viz_on()&&viz().pos>=0){int q=bit?viz().lastp:4096-viz().lastp;viz().cost[viz().pos]+=float(-std::log2(q/4096.));}
    if(lmix::mode()){bool cb=bit^pcc_flip();lmix::update(cb);cm_model::observe(cb,lmix::state().bitctx,bit,bit^pcc_sflip(),pcc_cbitctx());hist_model::observe(bit);}
    soft_model::observe(bit);match_model::observe(bit);palette_model::observe(bit);online_model::observe(bit);
    auto &s=state();auto &t=stats();t.bits++;
    int pb=bit?s.last_base:4096-s.last_base;
    int pm=bit?s.last_mixed:4096-s.last_mixed;
    if(t.enabled){t.baseline_bits-=std::log2(pb/4096.);t.mixed_bits-=std::log2(pm/4096.);}
    if(!s.active)return;
    t.modeled_bits++;
    int ps=bit?s.last_structure:4096-s.last_structure;
    int64_t denominator=int64_t(65536-s.posterior)*pb+int64_t(s.posterior)*ps;
    s.posterior=denominator?int((int64_t(s.posterior)*ps*65536+denominator/2)/denominator):0;
    for(int j=0;j<s.count;j++) {
        auto &p=s.paths[j];if(p.weight && p.code[s.position].bit!=bit)p.weight=0;
    }
    s.position++;
    if(s.position==1&&!bit)for(int j=0;j<s.count;j++)if(s.paths[j].weight)materialize(s.paths[j]);
}
inline void end() {
    hist_model::end();cm_model::end();soft_model::end();match_model::end();palette_model::end();online_model::end();
    auto &s=state();
    if(s.active)s.priors[s.key]=std::max(256,std::min(61440,(7*s.prior+s.posterior+4)/8));
    s.active=false;
}
} // namespace probability_transport
