#include "LiquidEngine.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>
int main(){
 constexpr int sr=48000,block=128;
 for(float sliceSize: {0.05f, 0.25f, 1.0f})for(int mode=0;mode<=1;++mode)for(int density: {8,32,64,128}){
  liquid::Engine engine(sr);
  std::vector<liquid::Stereo> wave(sr*2);
  for(size_t i=0;i<wave.size();++i){float x=.4f*std::sin(2*3.141592653589793*193.17*i/sr)+.12f*std::sin(2*3.141592653589793*997.3*i/sr);wave[i]={x,x};}
  engine.setSample(std::move(wave));
  liquid::Params p;p.fxType=0;p.fxMix=0;p.density=density;p.densityMode=mode;p.durationMs=175;p.maxActiveGrains=128;p.densityJitter=0;p.pitchRandom=0;p.gain=.45f;p.noteReleaseMs=100;p.sliceSize=sliceSize;
  engine.setParams(p);engine.setAmbientPlaying(false);for(unsigned n=2;n<6;++n)engine.noteOn(n);
  liquid::Stereo out[block];float last=0,peak=0,maxJump=0;uint64_t jumps=0;
  for(int b=0;b<1500;++b){engine.render(out,block);for(auto s:out){peak=std::max(peak,std::abs(s.l));float jump=std::abs(s.l-last);maxJump=std::max(jump,maxJump);jumps+=(jump>.15f);last=s.l;}}
  std::cout<<"slice_size="<<sliceSize<<" mode="<<(mode?"global":"per-note")<<" density="<<density<<" peak="<<peak<<" max_jump="<<maxJump<<" jumps_gt_0.15="<<jumps<<" active="<<engine.activeGrains()<<" started="<<engine.grainsStarted()<<" dropped="<<engine.grainsDropped()<<" stolen="<<engine.grainsStolen()<<" prelimit_overs="<<engine.preLimiterOvers()<<" prelimit_peak="<<engine.preLimiterPeak()<<"\n";
 }
}
