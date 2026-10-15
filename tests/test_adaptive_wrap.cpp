#include "LiquidEngine.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
int main(){
 constexpr int sr=48000;
 for(float slice:{.05f,.25f,1.f})for(int density:{8,32,64,128})for(int mode:{0,1}){
  liquid::Engine e(sr);std::vector<liquid::Stereo> wave(sr*2);
  for(size_t i=0;i<wave.size();++i){float x=.4f*std::sin(2*3.141592653589793*193.17*i/sr)+.12f*std::sin(2*3.141592653589793*997.3*i/sr);wave[i]={x,x};}
  e.setSample(std::move(wave));liquid::Params p;p.fxType=0;p.fxMix=0;p.density=density;p.densityMode=mode;p.durationMs=175;p.maxActiveGrains=128;p.densityJitter=0;p.pitchRandom=0;p.gain=.45f;p.noteReleaseMs=100;p.sliceSize=slice;e.setParams(p);e.setAmbientPlaying(false);
  for(unsigned n=2;n<6;++n)e.noteOn(n);
  liquid::Stereo out[128];float last=0,maxJump=0;
  for(int b=0;b<300;++b){e.render(out,128);for(auto s:out){maxJump=std::max(maxJump,std::abs(s.l-last));last=s.l;}}
  if(maxJump>.15f){std::cerr<<"excessive discontinuity slice="<<slice<<" density="<<density<<" mode="<<mode<<" jump="<<maxJump<<"\n";return 1;}
 }
}
