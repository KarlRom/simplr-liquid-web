#include "LiquidEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <numeric>
#include <string>
#include <cstdint>
#include <stdexcept>

struct Options {int seconds=30, buffer=128, grainLimit=64, density=64, effect=6;bool all=false;};
static void usage(){std::cout<<"Usage: liquid_benchmark [--seconds N] [--buffer 64|128|256|512] [--limit 16..128] [--density N] [--fx off|hall|pingpong|shimmer] [--all]\n";}
static int parseInt(const char* str,int lo,int hi){int n=std::stoi(str);if(n<lo||n>hi)throw std::runtime_error("Argument out of range");return n;}
static void run(const Options& o){
 constexpr double sr=48000.;
 std::vector<liquid::Stereo> sample(48000*8);
 for(size_t i=0;i<sample.size();++i){const float x=.25f*std::sin(2*3.141592653589793*220*i/sr);sample[i]={x,x};}
 liquid::Engine e(static_cast<float>(sr));e.setSample(std::move(sample));
 auto p=e.params();p.density=o.density;p.durationMs=300;p.maxActiveGrains=o.grainLimit;p.gain=.5;p.fxType=o.effect;p.fxMix=.35;p.fxFeedback=.5;p.bpm=120;p.sequencerPlaying=true;e.setParams(p);
 for(int step=0;step<16;++step)e.setSequenceStep(step,static_cast<uint16_t>(1u<<((step/2)%8)));
 std::vector<liquid::Stereo> output(o.buffer);
 const size_t totalBlocks=static_cast<size_t>(o.seconds)*48000/o.buffer;
 std::vector<double> times;times.reserve(totalBlocks);
 size_t peakActive=0,misses=0;const double deadlineUs=1e6*o.buffer/sr;
 for(size_t i=0;i<totalBlocks;++i){
  auto before=std::chrono::steady_clock::now();e.render(output.data(),output.size());auto after=std::chrono::steady_clock::now();
  const double usec=std::chrono::duration<double,std::micro>(after-before).count();times.push_back(usec);misses+=(usec>deadlineUs);peakActive=std::max(peakActive,e.activeGrains());
 }
 std::sort(times.begin(),times.end());auto quant=[&](double x){return times[static_cast<size_t>(x*(times.size()-1))];};
 const double average=std::accumulate(times.begin(),times.end(),0.)/times.size();
 const char* names[]={"off","hall","plate","room","delay","pingpong","shimmer"};
 std::cout<<"fx="<<names[o.effect]<<" density="<<o.density<<" limit="<<o.grainLimit<<" buffer="<<o.buffer<<" seconds="<<o.seconds
 <<" average_us="<<average<<" p95_us="<<quant(.95)<<" p99_us="<<quant(.99)<<" max_us="<<times.back()<<" deadline_us="<<deadlineUs<<" misses="<<misses
 <<" max_active="<<peakActive<<" dropped="<<e.grainsDropped()<<" sample_pcm_bytes="<<48000*8*sizeof(liquid::Stereo)<<"\n";
}
int main(int argc,char** argv){try{Options o;for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--help"){usage();return 0;}if(a=="--all"){o.all=true;continue;}if(i+1>=argc)throw std::runtime_error("Missing argument after "+a);std::string v=argv[++i];if(a=="--seconds")o.seconds=parseInt(v.c_str(),1,3600);else if(a=="--buffer")o.buffer=parseInt(v.c_str(),32,2048);else if(a=="--limit")o.grainLimit=parseInt(v.c_str(),1,128);else if(a=="--density")o.density=parseInt(v.c_str(),1,1000);else if(a=="--fx"){if(v=="off")o.effect=0;else if(v=="hall")o.effect=1;else if(v=="pingpong")o.effect=5;else if(v=="shimmer")o.effect=6;else throw std::runtime_error("Unknown FX: "+v);}else throw std::runtime_error("Unknown argument: "+a);}if(o.all){for(int size:{64,128,256,512})for(int fx:{0,5,6}){Options x=o;x.buffer=size;x.effect=fx;run(x);}}else run(o);return 0;}catch(const std::exception& e){std::cerr<<"benchmark: "<<e.what()<<"\n";usage();return 1;}}
