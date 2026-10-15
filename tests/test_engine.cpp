#include "LiquidEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
int main(){
  constexpr int sr=48000;
  liquid::Engine engine(sr);
  std::vector<liquid::Stereo> output(sr);
  engine.render(output.data(),output.size());
  for(auto s:output)assert(s.l==0 && s.r==0); // no sample -> silence
  std::vector<liquid::Stereo> waveform(sr);
  for(size_t i=0;i<waveform.size();++i){float x=static_cast<float>(std::sin(i*6.283185307179586*220.0/sr)*.75);waveform[i]={x,x};}
  engine.setSample(waveform);
  liquid::Params p;p.density=40;p.durationMs=75;p.position=.5;p.spray=.2;p.gain=.5;
  engine.setParams(p);
  engine.render(output.data(),output.size());
  assert(engine.grainsStarted()==40); // 40 grains/s at 48 kHz
  assert(engine.grainsDropped()==0);
  double energy=0;
  for(auto s:output){assert(std::isfinite(s.l)&&std::isfinite(s.r));assert(std::abs(s.l)<=1&&std::abs(s.r)<=1);energy+=s.l*s.l+s.r*s.r;}
  assert(energy>1.0);
  engine.reset();
  p.density=1000;p.durationMs=2000;p.spray=1;p.pitchSemitones=36;p.gain=2;
  engine.setParams(p);
  engine.render(output.data(),output.size());
  assert(engine.activeGrains()<=liquid::Engine::kMaxGrains);
  assert(engine.grainsDropped()>0);
  for(auto s:output)assert(std::isfinite(s.l)&&std::isfinite(s.r));
  engine.reset();p.density=0;engine.setParams(p);engine.render(output.data(),output.size());
  for(auto s:output)assert(s.l==0 && s.r==0);
  // Deterministic render after reset
  p.density=20;p.durationMs=90;p.pitchSemitones=-12;p.gain=.3;engine.setParams(p);
  engine.render(output.data(),output.size());
  const auto first=output;
  engine.reset();engine.render(output.data(),output.size());
  for(size_t i=0;i<first.size();++i)assert(first[i].l==output[i].l && first[i].r==output[i].r);
  // One-shot piano keys are positioned in 13 slices of the selected region.
  liquid::Engine keys(sr);keys.setSample(waveform);
  liquid::Params kp;kp.density=0;kp.gain=.5;kp.regionStart=.10f;kp.regionEnd=.90f;
  keys.setParams(kp);
  assert(!keys.triggerSlice(13,13));
  assert(keys.triggerSlice(0,13) && keys.triggerSlice(12,13));
  assert(keys.activeGrains()==2);
  std::vector<liquid::Stereo> keyAudio(512);
  keys.render(keyAudio.data(),keyAudio.size());
  double keyEnergy=0;for(auto v:keyAudio)keyEnergy+=v.l*v.l+v.r*v.r;
  assert(keyEnergy>0.001);
  assert(keys.activeGrains()<=liquid::Engine::kMaxGrains);
  // Live ring buffer preserves chronological capture on wrap and freeze/export snapshots.
  liquid::Engine live(sr);live.beginLive(4);
  liquid::Stereo a[6]={{1,1},{2,2},{3,3},{4,4},{5,5},{6,6}};
  live.pushLive(a,6);auto snap=live.snapshot();assert(snap.size()==4);
  for(int i=0;i<4;i++)assert(snap[i].l==float(i+3));
  live.stopLive();auto stopped=live.snapshot();assert(stopped.size()==4);
  for(int i=0;i<4;i++)assert(stopped[i].l==snap[i].l);
  std::cout<<"PASS: silence, scheduling, bounds, output, saturation, capacity, reset, determinism\n";
}
