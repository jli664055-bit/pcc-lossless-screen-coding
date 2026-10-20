// FLIF 扫描线预测器的结构记忆扩展，不使用 DCT 系数；
// 全部训练目标均为当前光栅位置之前的已解码像素。
#pragma once
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include "probability_transport.hpp"

#ifndef STRUCT_CONTEXT_MODE
#define STRUCT_CONTEXT_MODE 3
#endif

namespace structure_memory {
struct Descriptor {
    uint64_t key;
    int low, span, direction;
    int transport(int q) const {
        int u=(q*span+(q>=0?2048:-2048))/4096;
        return direction>0 ? low+u : low+span-u;
    }
    int canonical(int x) const {
        return ((direction>0?x-low:low+span-x)*4096)/span;
    }
};

template<class Plane>
bool describe(const Plane &plane,int r,int c,int width,int level,Descriptor &d) {
    if(r<2 || c<2 || c+2>=width)return false;
    const int dy[10]={0,-1,-1,-1,0,-2,-2,-2,-1,-1};
    const int dx[10]={-1,0,-1,1,-2,0,-1,1,-2,2};
    int v[10],n=level?10:6;
    for(int i=0;i<n;i++)v[i]=plane.get(r+dy[i],c+dx[i]);
    int lo=*std::min_element(v,v+n),hi=*std::max_element(v,v+n);
    if(hi-lo<4)return false; // MED 已能较好预测平坦邻域。
    d.low=lo;d.span=hi-lo;d.direction=1;
    if(STRUCT_CONTEXT_MODE==1) {
        if(level)return false;
        d.low=0;d.span=4096;d.key=0;
        for(int i=0;i<n;i++)d.key=(d.key<<8)|uint64_t(v[i]&255);
        return true;
    }
    for(int i=1;i<n;i++)if(v[i]!=v[0]){d.direction=v[0]<v[i]?1:-1;break;}
    d.key=uint64_t(level+1)<<56;
    for(int i=0;i<n;i++) {
        int u=d.direction>0?v[i]-lo:hi-v[i];
        d.key=(d.key&0xff00000000000000ULL)|((d.key&0x00ffffffffffffffULL)<<4)|uint64_t((u*15+d.span/2)/d.span);
    }
    return true;
}

struct Node {
    int count=0;
    int64_t sum=0,transfer_loss=0,median_loss=0;
    int values[16]={0},frequencies[16]={0};
    int estimate() const {return int(sum/std::max(1,count));}
    void observe(int target,int median) {
        if(count) {
            transfer_loss+=std::abs(target-estimate());
            median_loss+=std::abs(target-median);
        }
        sum+=target;count++;
        int found=-1,empty=-1;
        for(int i=0;i<16;i++){if(frequencies[i] && values[i]==target)found=i;if(!frequencies[i])empty=i;}
        if(found>=0)frequencies[found]++;
        else if(empty>=0){values[empty]=target;frequencies[empty]=1;}
        else for(int &frequency:frequencies)frequency--;
        if(count==128){sum/=2;count/=2;transfer_loss/=2;median_loss/=2;for(int &frequency:frequencies)frequency=(frequency+1)/2;}
    }
    bool reliable() const {return count>=4 && transfer_loss*100<median_loss*95;}
};

struct Memory {
    const void *identity=nullptr;
    int width=0,height=0;
    int64_t last=-1,next=0;
    std::unordered_map<uint64_t,Node> nodes;
    void reset(const void *id,int w,int h) {
        identity=id;width=w;height=h;last=-1;next=0;nodes.clear();
        probability_transport::reset_history();
    }
};

template<class Plane>
int prediction(const Plane &plane,int r,int c,int width,int height,int median) {
    static const bool no_norm=std::getenv("SMEM_NO_NORM")!=nullptr;
    if(no_norm){probability_transport::prepare(0,{});return median;}
    static Memory memory;
    int64_t pos=int64_t(r)*width+c;
    if(memory.identity!=&plane || memory.width!=width || memory.height!=height || pos<=memory.last)
        memory.reset(&plane,width,height);
    memory.last=pos;
    int levels=STRUCT_CONTEXT_MODE==3?2:1;
    // 同时填充被跳过的光栅位置；所有目标均已解码。
    for(;memory.next<pos;memory.next++) {
        int rr=memory.next/width,cc=memory.next%width;
        if(rr<2 || cc<2 || cc+2>=width)continue;
        int left=plane.get(rr,cc-1),top=plane.get(rr-1,cc),tl=plane.get(rr-1,cc-1);
        int base=std::max(std::min(left,top),std::min(std::max(left,top),left+top-tl));
        int actual=plane.get(rr,cc);
        for(int level=0;level<levels;level++) {
            Descriptor d;
            if(!describe(plane,rr,cc,width,level,d))continue;
            memory.nodes[d.key].observe(d.canonical(actual),d.canonical(base));
        }
        if(memory.nodes.size()>200000)memory.nodes.clear();
    }
    int guess=median;int64_t best=INT64_MAX;
    static std::vector<std::pair<int,int>> distribution;distribution.clear(); // 复用，避免逐像素堆分配
    uint64_t distribution_key=0;
    for(int level=0;level<levels;level++) {
        Descriptor d;
        if(!describe(plane,r,c,width,level,d))continue;
        auto it=memory.nodes.find(d.key);
        if(it==memory.nodes.end())continue;
        const Node &node=it->second;
        if(node.count>=8) {
            distribution.clear();distribution_key=d.key;
            for(int i=0;i<16;i++)if(node.frequencies[i])distribution.push_back({d.transport(node.values[i]),node.frequencies[i]});
        }
        if(!node.reliable())continue;
        // 优先历史误差较低者；误差相同时取精细模板。
        int64_t risk=node.transfer_loss*4096/std::max(1,node.count-1);
        if(risk<=best){best=risk;guess=d.transport(node.estimate());}
    }
    probability_transport::prepare(distribution_key,distribution);
    return guess;
}
} // namespace structure_memory
