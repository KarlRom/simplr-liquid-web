#include "LiquidEngine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(){
  constexpr int sr=48000;
  liquid::Engine engine(sr);
  std::vector<liquid::Stereo> sample(sr);
  // Nonconstant, harmonically rich signal, intentionally not a zero-crossing at slice boundaries.
  for(int i=0;i<sr;++i){float v=.55f*std::sin(2*3.141592653589793*173.3*i/sr)+.12f*std::sin(2*3.141592653589793*1309*i/sr);sample[i]={v,v};}
  engine.setSample(std::move(sample));
  liquid::Params p; p.fxType=0;p.fxMix=0;p.noteAttackMs=5;p.noteReleaseMs=30;
  p.maxActiveGrains=8;p.density=65;p.durationMs=210;p.gain=.4f;
  engine.setParams(p);engine.setAmbientPlaying(false);
  for(int step=0;step<16;++step)engine.setSequenceStep(step,1u<<4);
  float previous=0,peak=0;int spikes=0;liquid::Stereo output[128];
  engine.noteOn(4);
  for(int b=0;b<1500;++b){
    if(b%63==0)engine.noteOff(4);
    if(b%63==2)engine.noteOn(4);
    if(b==450){p.sequencerPlaying=true;engine.setParams(p);}
    engine.render(output,128);
    for(auto sampleOut:output){float jump=std::abs(sampleOut.l-previous);peak=std::max(peak,jump);if(jump>.15f)++spikes;previous=sampleOut.l;}
  }
  std::cout<<"dry retrigger diagnostic peak_jump="<<peak<<" spikes_above_0.15="<<spikes<<" stolen="<<engine.grainsStolen()<<" dropped="<<engine.grainsDropped()<<"\n"<<std::flush;
  if(spikes>0)throw std::runtime_error("large output discontinuity in FX-off retrigger regression");
}
