#include "LiquidEngine.h"
#include <cassert>
#include <algorithm>
#include <utility>
#include <cmath>
#include <vector>
int main(){
 liquid::Engine e(48000);
 std::vector<liquid::Stereo> audio(48000);
 for(size_t i=0;i<audio.size();++i) audio[i]={static_cast<float>(0.4*std::sin(2*3.14159265*220*i/48000)),static_cast<float>(0.4*std::sin(2*3.14159265*220*i/48000))};
 e.setSample(std::move(audio)); auto p=e.params();p.density=80;p.durationMs=85;p.noteReleaseMs=800;p.fxMix=.4;p.fxType=1;p.fxTime=.3;p.fxFeedback=.35;e.setParams(p);e.setAmbientPlaying(false);
 assert(e.noteOn(0));std::vector<liquid::Stereo> block(4800);
 e.render(block.data(),block.size());auto before=e.grainsStarted();assert(before>0);
 e.noteOff(0);e.render(block.data(),block.size());assert(e.grainsStarted()>before);
 for(auto mode:{1,2,3,4}){p.fxType=mode;e.setParams(p);e.render(block.data(),block.size());for(auto v:block) assert(std::isfinite(v.l)&&std::isfinite(v.r));}
 p.sliceSize=8.f;p.durationMs=2000;p.noteReleaseMs=5000;e.setParams(p);assert(e.params().sliceSize==8.f&&e.params().durationMs==2000);
}
