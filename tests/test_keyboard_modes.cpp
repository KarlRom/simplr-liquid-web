#include "LiquidEngine.h"
#include <cassert>
#include <cmath>
#include <vector>
int main(){
  liquid::Engine engine;
  std::vector<liquid::Stereo> sample(48000);
  for(size_t i=0;i<sample.size();++i)sample[i]={float(std::sin(i*.02)),float(std::sin(i*.02))};
  engine.setSample(std::move(sample));engine.setAmbientPlaying(false);
  liquid::Params p;p.keyboardMode=0;p.sliceSize=.5f;p.position=.4f;p.regionStart=.1f;p.regionEnd=.9f;
  engine.setParams(p);
  assert(engine.noteOn(0));assert(engine.noteOn(12));
  liquid::GrainVisual visual[128];auto n=engine.grainVisuals(visual,128);
  assert(n==2);assert(std::abs(visual[0].position-visual[1].position)>.2f);
  engine.noteOff(0);engine.noteOff(12);engine.reset();
  p.keyboardMode=1;engine.setParams(p);
  assert(engine.noteOn(0));assert(engine.noteOn(12));
  n=engine.grainVisuals(visual,128);assert(n==2);
  assert(std::abs(visual[0].position-visual[1].position)<.04f); // both centered at one position
  std::vector<liquid::Stereo> output(480);engine.render(output.data(),output.size());
  for(auto x:output)assert(std::isfinite(x.l)&&std::isfinite(x.r));
  return 0;
}
