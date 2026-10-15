#include "LiquidEngine.h"
#include <vector>
#include <cassert>
#include <cmath>
int main(){
 liquid::Engine e(48000);std::vector<liquid::Stereo> sample(48000);
 for(size_t i=0;i<sample.size();++i){float v=0.2f*std::sin(6.283185307*220*i/48000);sample[i]={v,v};}
 e.setSample(std::move(sample));auto p=e.params();p.maxActiveGrains=16;p.density=200;p.durationMs=1000;p.sequencerPlaying=false;e.setParams(p);
 std::vector<liquid::Stereo> out(128);for(int b=0;b<400;b++){e.render(out.data(),out.size());assert(e.activeGrains()<=16);}assert(e.grainsDropped()>0);
 p.maxActiveGrains=64;e.setParams(p);for(int b=0;b<400;b++){e.render(out.data(),out.size());assert(e.activeGrains()<=64);}assert(e.activeGrains()>16);
 return 0;
}
