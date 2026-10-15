#include "LiquidEngine.h"
#include <vector>
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
  std::vector<liquid::Stereo> source(48000);for(size_t i=0;i<source.size();++i){float v=.3f*std::sin(i*.013);source[i]={v,v};}
  auto run=[&](int mode){liquid::Engine e; e.setSample(source);auto p=e.params();p.density=40;p.densityMode=mode;p.durationMs=100;p.maxActiveGrains=128;e.setParams(p);e.setAmbientPlaying(false);e.noteOn(0);e.noteOn(2);e.noteOn(4);std::vector<liquid::Stereo> block(128);for(int i=0;i<375;++i)e.render(block.data(),block.size());return e.grainsStarted();};
  auto shared=run(1),perNote=run(0);std::cout<<"global="<<shared<<" per_note="<<perNote<<"\n";
  assert(shared<perNote);assert(shared>=30);assert(perNote>80);
}
