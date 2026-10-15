#include "LiquidEngine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
static double projection(const std::vector<liquid::Stereo>& x,int sr,double hz) {
 double re=0,im=0;
 for(size_t i=0;i<x.size();++i){double phase=6.283185307179586*hz*i/sr;re+=x[i].l*std::cos(phase);im+=x[i].l*std::sin(phase);}
 return std::hypot(re,im)/x.size();
}
static std::vector<liquid::Stereo> run(float amount){
 liquid::Engine e(48000);
 std::vector<liquid::Stereo> source(48000*3);
 for(size_t i=0;i<source.size();++i){float v=.3f*std::sin(6.283185307179586*220*i/48000.);source[i]={v,v};}
 e.setSample(std::move(source));auto p=e.params();p.density=24;p.durationMs=1000;p.position=.3f;p.spray=0;p.pitchRandom=0;p.fxType=6;p.fxMix=1.f;p.fxFeedback=.35f;p.fxTime=.4f;p.shimmerAmount=amount;p.shimmerTone=1.f;e.setParams(p);
 std::vector<liquid::Stereo> block(256),all;all.reserve(48000*3);
 for(int i=0;i<48000*3/256;i++){e.render(block.data(),block.size());all.insert(all.end(),block.begin(),block.end());}
 for(auto v:all)if(!std::isfinite(v.l)||!std::isfinite(v.r))return {};
 return std::vector<liquid::Stereo>(all.begin()+48000,all.begin()+48000*2);
}
int main(){auto off=run(0),on=run(1);if(off.empty()||on.empty())return 1;
 double fundamental=projection(on,48000,220),octaveOff=projection(off,48000,440),octaveOn=projection(on,48000,440);
 std::cout<<"shimmer octave(0)="<<octaveOff<<" octave(1)="<<octaveOn<<" fundamental="<<fundamental<<"\n";
 if(!(octaveOn>octaveOff*1.15 && octaveOn>0.00003))return 2;
 return 0;}
