#include "LiquidEngine.h"
#include <stdexcept>
#define assert(expr) do { if(!(expr)) throw std::runtime_error("Failed: " #expr); } while(0)
#include <cmath>
#include <vector>
#include <iostream>
int main(){
  liquid::Engine e(48000);
  std::vector<liquid::Stereo> sample(48000,{0.65f,0.65f});
  e.setSample(std::move(sample));
  liquid::Params p; p.gain=.5f; p.durationMs=200; p.density=10;
  p.fxMix=0; p.noteAttackMs=5; p.noteReleaseMs=30;
  p.maxActiveGrains=16; e.setParams(p); e.setAmbientPlaying(false);
  liquid::Stereo output[512]{};
  assert(e.noteOn(4));
  e.render(output,512);
  // Note-on / grain attack must start at silence even for constant full-scale sample.
  assert(std::abs(output[0].l)<0.00001f);
  float maxJump=0;
  for(int i=1;i<512;++i) maxJump=std::max(maxJump,std::abs(output[i].l-output[i-1].l));
  assert(maxJump<0.05f);
  e.noteOff(4);
  e.render(output,512);
  for(int i=1;i<512;++i) maxJump=std::max(maxJump,std::abs(output[i].l-output[i-1].l));
  assert(maxJump<0.05f);
  liquid::Engine seq(48000);seq.setSample(std::vector<liquid::Stereo>(48000,{0.65f,0.65f}));
  p.sequencerPlaying=true; p.bpm=200; seq.setParams(p); seq.setAmbientPlaying(false);
  for(unsigned step=0;step<16;++step)seq.setSequenceStep(step,1<<4);
  float prev=0; float peakJump=0;
  for(int j=0;j<75;++j){seq.render(output,512);for(auto v:output){peakJump=std::max(peakJump,std::abs(v.l-prev));prev=v.l;}}
  assert(peakJump<0.05f);
  std::cout<<"click envelope OK: key max jump "<<maxJump<<" sequencer max jump "<<peakJump<<"\n";
}
