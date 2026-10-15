#include "LiquidEngine.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
using namespace liquid;
int main(){
 std::vector<Stereo> source(48000);for(size_t i=0;i<source.size();++i){float v=.2f*std::sin(i*.031);source[i]={v,v};}
 auto run=[&](float timing, int curve,int mode){Engine e;e.setSample(source);auto p=e.params();p.density=64;p.densityMode=mode;p.durationMs=60;p.maxActiveGrains=128;p.grainBloomMs=600;p.grainTiming=timing;p.bloomCurve=curve;p.bloomVariation=.5f;e.setParams(p);e.setAmbientPlaying(false);e.noteOn(0);e.noteOn(2);std::vector<Stereo> buf(128);unsigned mid=0;for(int i=0;i<375;++i){e.render(buf.data(),buf.size());if(i==187)mid=e.grainsStarted();}return std::pair<unsigned,unsigned>{mid,e.grainsStarted()};};
 for(int mode=0;mode<2;++mode){auto regular=run(0.f,0,mode);auto organic=run(1.f,2,mode);auto exponential=run(0.f,1,mode);std::cout<<"mode="<<mode<<" regular="<<regular.second<<" organic="<<organic.second<<" exponential="<<exponential.second<<"\n";assert(regular.second>regular.first);assert(organic.second>organic.first);assert(exponential.second>exponential.first);assert(regular.first<regular.second-regular.first);}
}
