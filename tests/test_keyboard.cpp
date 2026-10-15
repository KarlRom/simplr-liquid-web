#include "LiquidEngine.h"
#include <vector>
#include <cassert>
#include <cmath>
int main(){
 liquid::Engine e;std::vector<liquid::Stereo> samples(48000);
 for(size_t i=0;i<samples.size();++i){float x=std::sin(i*.073);samples[i]={x,x};}
 e.setSample(samples);liquid::Params p;p.density=32;p.durationMs=80;e.setParams(p);
 e.setAmbientPlaying(false);std::vector<liquid::Stereo> out(4800);
 e.render(out.data(),out.size());assert(e.grainsStarted()==0);
 assert(e.noteOn(0));assert(e.noteOn(7));
 // A short, wet-only key tap must terminate on the grain duration, not
 // continue playing the original full-length waveform slice.
 assert(e.grainsStarted()==2);
 assert(e.activeGrains()==2);assert(e.noteHeld(0)&&e.noteHeld(7));
 e.render(out.data(),out.size());assert(e.grainsStarted()>=8);
 const auto before=e.grainsStarted();e.noteOff(0);assert(!e.noteHeld(0)&&e.noteHeld(7));
 e.render(out.data(),out.size());assert(e.grainsStarted()>before);
 e.noteOff(7);e.render(out.data(),out.size());const auto stopped=e.grainsStarted();
 e.render(out.data(),out.size());assert(e.grainsStarted()==stopped);
 assert(!e.noteOn(13));return 0;
}
