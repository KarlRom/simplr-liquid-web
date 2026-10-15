#include "LiquidEngine.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
int main(){
  std::vector<liquid::Stereo> sample(48000);
  for(size_t i=0;i<sample.size();++i)sample[i]={float(i)/48000.f,float(i)/48000.f};
  liquid::Engine e; e.setSample(sample);
  liquid::Params p; p.density=0; p.gain=.5f; p.regionStart=.1f;p.regionEnd=.9f;p.travel=1;p.speed=.65f;
  std::vector<liquid::Stereo> out(512);
  float first=-1;
  for(int m=0;m<6;m++){
    e.reset(); p.direction=static_cast<liquid::Direction>(m);e.setParams(p);
    assert(e.triggerSlice(6,13));e.render(out.data(),out.size());
    liquid::GrainVisual g[128];auto n=e.grainVisuals(g,128);assert(n==1);
    assert(std::isfinite(g[0].position));assert(g[0].position>=0&&g[0].position<=1);
    if(m==0)first=g[0].position;
    if(m==1)assert(std::abs(g[0].position-first)>0.001f);
    for(auto v:out)assert(std::isfinite(v.l)&&std::isfinite(v.r));
  }
  p.travel=0;p.direction=liquid::Direction::Forward;e.reset();e.setParams(p);assert(e.triggerSlice(0,13));e.render(out.data(),out.size());
  std::cout<<"direction modes and zero-travel compatibility passed\n";
}
