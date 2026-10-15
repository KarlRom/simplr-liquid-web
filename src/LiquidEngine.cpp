#include "LiquidEngine.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace liquid {
namespace { constexpr double pi=3.14159265358979323846; }
Engine::Engine(float sr):sampleRate_((sr>=8000 && sr<=192000)?sr:48000.0f) { noteGain_.fill(0); }
void Engine::setSample(const std::vector<Stereo>& data) { sample_=data; reset(); }
void Engine::setSample(std::vector<Stereo>&& data) { sample_=std::move(data); reset(); }
void Engine::setParams(Params p) {
  p.travel=std::clamp(p.travel,0.0f,1.0f);
  p.speed=std::clamp(p.speed,0.0f,1.0f);
  p.spread=std::clamp(p.spread,0.0f,1.0f);
  if(static_cast<int>(p.direction)<0||static_cast<int>(p.direction)>5)p.direction=Direction::Forward;
  p.attack=std::clamp(p.attack,0.f,1.f);
  p.release=std::clamp(p.release,0.f,1.f);
  p.shape=std::clamp(p.shape,0,2);
  p.densityJitter=std::clamp(p.densityJitter,0.f,1.f);
  p.pitchRandom=std::clamp(p.pitchRandom,0.f,12.f);
  p.densityMode=std::clamp(p.densityMode,0,1);
  p.maxActiveGrains=std::clamp(p.maxActiveGrains,1,static_cast<int>(kMaxGrains));
  p.density=std::clamp(p.density,0.0f,1000.0f);
  p.grainBloomMs=std::clamp(p.grainBloomMs,0.f,3000.f);
  p.bloomCurve=std::clamp(p.bloomCurve,0,2);
  p.bloomVariation=std::clamp(p.bloomVariation,0.f,1.f);
  p.grainTiming=std::clamp(p.grainTiming,0.f,1.f);
  p.durationMs=std::clamp(p.durationMs,1.0f,2000.0f);
  p.pitchSemitones=std::clamp(p.pitchSemitones,-36.0f,36.0f);
  p.position=std::clamp(p.position,0.0f,1.0f);
  p.spray=std::clamp(p.spray,0.0f,1.0f);
  p.regionStart=std::clamp(p.regionStart,0.0f,0.99f);
  p.regionEnd=std::clamp(p.regionEnd,p.regionStart+0.001f,1.0f);
  p.keyboardMode=std::clamp(p.keyboardMode,0,1);
  p.sliceSize=std::clamp(p.sliceSize,0.01f,8.0f);
  p.panSpread=std::clamp(p.panSpread,0.0f,1.0f);
  p.noteReleaseMs=std::clamp(p.noteReleaseMs,0.f,5000.f);
  p.noteAttackMs=std::clamp(p.noteAttackMs,0.f,100.f);
  p.fxType=std::clamp(p.fxType,0,6);
  p.shimmerAmount=std::clamp(p.shimmerAmount,0.f,1.f);
  p.shimmerTone=std::clamp(p.shimmerTone,0.f,1.f);
  p.shimmerInterval=(p.shimmerInterval==7||p.shimmerInterval==19)?p.shimmerInterval:12;
  p.bpm=std::clamp(p.bpm,40.f,240.f);
  p.clickVolume=std::clamp(p.clickVolume,0.f,.7f);
  p.delayDivision=std::clamp(p.delayDivision,0,4);
  if(!p.sequencerPlaying && params_.sequencerPlaying) {for(unsigned k=0;k<13;++k){seqHeld_[k]=false;/* envelope decays naturally */}seqClock_=0;}
  p.fxMix=std::clamp(p.fxMix,0.f,1.f);
  p.fxTime=std::clamp(p.fxTime,0.f,1.f);
  p.fxFeedback=std::clamp(p.fxFeedback,0.f,.92f);
  p.gain=std::clamp(p.gain,0.0f,2.0f);
  if(params_.keyboardMode!=p.keyboardMode) {held_.fill(false);notePhase_.fill(0);}
  params_=p;
}
void Engine::beginLive(size_t frames) {
  sample_.assign(std::max<size_t>(frames, 2), Stereo{});
  live_=true; writeHead_=captured_=0; reset();
}
void Engine::pushLive(const Stereo* input, size_t frames) {
  if(!live_ || !input || sample_.empty()) return;
  for(size_t i=0;i<frames;i++) {sample_[writeHead_]=input[i];writeHead_=(writeHead_+1)%sample_.size();captured_=std::min(captured_+1,sample_.size());}
}
std::vector<Stereo> Engine::snapshot() const {
  if(!live_) return sample_;
  std::vector<Stereo> result; result.reserve(captured_);
  const size_t first=(writeHead_+sample_.size()-captured_)%sample_.size();
  for(size_t i=0;i<captured_;++i) result.push_back(sample_[(first+i)%sample_.size()]);
  return result;
}
void Engine::stopLive() {
  if(!live_) return;
  // Rotate circular buffer into chronological order, once outside audio callback.
  if(captured_<sample_.size()) sample_.resize(captured_);
  else std::rotate(sample_.begin(),sample_.begin()+writeHead_,sample_.end());
  live_=false;writeHead_=0;reset();
}
void Engine::setSequenceStep(unsigned step,uint16_t mask){if(step<16)sequence_[step]=mask&0x1fff;}
void Engine::reset() { for(auto& g:grains_) g={}; for(auto& t:stealTails_) t={}; held_.fill(false); noteGain_.fill(0); noteGate_.fill(false);noteBloomAge_.fill(0);bloomMultiplier_.fill(1.f);for(unsigned k=0;k<13;++k)arrivalRng_[k]=0x9E3779B9u^(0x85EBCA6Bu*(k+1));globalArrivalRng_=0xCAB12345u;nextGlobalInterval_=1.;globalRoundRobin_=0;mixGain_=1.f;preLimiterPeak_=0.f;preLimiterOvers_=0; notePhase_.fill(0); phase_=0;nextAmbientInterval_=1.0;nextNoteInterval_.fill(1.0); rng_=0x12345678u; started_=dropped_=stolen_=0;globalNotePhase_=0;seqHeld_.fill(false);seqClock_=0;seqStep_=0;seqGateFrames_=0;clickFrames_=0;processedFrames_=0; }
float Engine::rand01() { rng_^=rng_<<13; rng_^=rng_>>17; rng_^=rng_<<5; return static_cast<float>((rng_>>8)*(1.0/16777216.0)); }
void Engine::applyGrainTone(Grain& g) {
  const double deviation=(rand01()*2.0-1.0)*params_.pitchRandom;
  g.step=std::pow(2.0,(params_.pitchSemitones+deviation)/12.0);
  g.envAttack=params_.attack;g.envRelease=params_.release;g.envShape=params_.shape;
}
float Engine::envelope(const Grain& g) const {
  const float u=static_cast<float>(g.age)/std::max(1u,g.length-1);
  // Normalize attack and release so they cannot overlap beyond the full grain.
  const float total=std::max(1.f,g.envAttack+g.envRelease);
  const float a=g.envAttack/total,r=g.envRelease/total;
  float rise=a>0.0001f?std::min(1.f,u/a):1.f;
  float fall=r>0.0001f?std::min(1.f,(1.f-u)/r):1.f;
  float e=std::clamp(std::min(rise,fall),0.f,1.f);
  if(g.envShape==0) e=e*e*(3.f-2.f*e);
  else if(g.envShape==2) e=e*e;
  const float minFade=std::min(0.49f, (sampleRate_*.002f)/std::max(2u,g.length));
  if(minFade>0){const float fadeIn=std::clamp(u/minFade,0.f,1.f);const float fadeOut=std::clamp((1.f-u)/minFade,0.f,1.f);e*=std::min(fadeIn,fadeOut);}
  return e;
}
void Engine::saveStealTail(const Grain& grain){
  // Reserve one of the fixed transition slots; never allocate in the callback.
  for(auto& t:stealTails_) if(!t.active){t.grain=grain;t.age=0;t.active=true;return;}
}
Engine::Grain* Engine::allocateGrain(){
  const auto end=grains_.begin()+params_.maxActiveGrains;
  auto free=std::find_if(grains_.begin(),end,[](const Grain& g){return !g.active;});
  if(free!=end) return &*free;
  // Prefer a grain already at the end of its amplitude envelope.
  // Its old contribution is very small, reducing click risk on replacement.
  Grain* victim=nullptr; float best=1e9f;
  for(auto it=grains_.begin();it!=end;++it){
    const float note=it->note>=0?noteGain_[static_cast<size_t>(it->note)]:1.f;
    const float score=envelope(*it)*note*(std::abs(it->gl)+std::abs(it->gr));
    if(score<best){best=score;victim=&*it;}
  }
  // Avoid stealing an audible grain: reject if no sufficiently quiet tail.
  if(victim && best<0.003f){saveStealTail(*victim);++stolen_;return victim;}
  ++dropped_;return nullptr;
}
void Engine::startGrain() {
  if(sample_.size()<2 || (live_ && captured_<256)) return;
  Grain* target=allocateGrain(); if(!target)return;
  Grain& g=*target;
  g.length=static_cast<uint32_t>(std::max(2.0f,std::round(params_.durationMs*0.001f*sampleRate_)));
  const size_t n=live_?captured_:sample_.size();
  const double lo=params_.regionStart*(n-1), hi=params_.regionEnd*(n-1);
  double start=lo+params_.position*(hi-lo);
  start+=(static_cast<double>(rand01())*2-1)*params_.spray*(hi-lo);
  g.pos=std::clamp(start,lo,hi);
  if(live_) g.pos=std::fmod(static_cast<double>(writeHead_+sample_.size()-captured_)+g.pos,static_cast<double>(sample_.size()));
  g.origin=g.pos;g.lower=lo;g.upper=hi;g.directionSign=rand01()<.5f?-1.0f:1.0f;
  g.movement=(rand01()*2.0-1.0)*params_.spread*(hi-lo)*0.25;
  if(live_){const double shift=static_cast<double>(writeHead_+sample_.size()-captured_);g.lower=std::fmod(lo+shift,static_cast<double>(sample_.size()));g.upper=std::fmod(hi+shift,static_cast<double>(sample_.size()));}
  applyGrainTone(g);
  const float pan=(rand01()*2.0f-1.0f)*params_.panSpread;
  const double angle=(static_cast<double>(pan)+1.0)*pi/4.0;
  g.gl=static_cast<float>(std::cos(angle)); g.gr=static_cast<float>(std::sin(angle));
  g.age=0; g.active=true; g.sliceVoice=false;g.note=-1; ++started_;
}
bool Engine::triggerSlice(unsigned index,unsigned count) {
  if(count!=13)return false;
  return startSliceGrain(index,false);
}
bool Engine::startSliceGrain(unsigned index, bool sustained) {
  (void)sustained;  // All keyboard notes are wet granular; never play an entire slice.
  if(index>=13 || sample_.size()<2 || (live_ && captured_<256)) return false;
  Grain* target=allocateGrain(); if(!target)return false;
  const size_t n=live_?captured_:sample_.size();
  const double lo=params_.regionStart*(n-1),hi=params_.regionEnd*(n-1);
  const double nominalWidth=(hi-lo)/13.0;
  if(nominalWidth<2)return false;
  // SLICE: 13 evenly spaced centers, with user-adjustable region width.
  // PITCH: all keys sample one region centered on POSITION; the 13 keys transpose it.
  const double center=params_.keyboardMode==0
    ? lo+nominalWidth*(static_cast<double>(index)+0.5)
    : lo+(hi-lo)*params_.position;
  const double width=std::min(hi-lo,nominalWidth*params_.sliceSize);
  const double sliceLo=std::clamp(center-width*0.5,lo,hi);
  const double sliceHi=std::clamp(center+width*0.5,lo,hi);
  const double sliceWidth=sliceHi-sliceLo;
  if(sliceWidth<2)return false;
  const size_t offset=live_?(writeHead_+sample_.size()-captured_)%sample_.size():0;
  auto& g=*target;
  const double source=sliceLo+(0.02+0.96*rand01())*std::max(1.,sliceHi-sliceLo);
  g.pos=std::fmod(source+offset,static_cast<double>(sample_.size()));
  g.origin=g.pos;
  g.lower=std::fmod(sliceLo+offset,static_cast<double>(sample_.size()));
  g.upper=std::fmod(sliceHi+offset,static_cast<double>(sample_.size()));
  g.sliceEnd=g.upper;g.movement=(rand01()*2.-1.)*params_.spread*sliceWidth*.15;
  g.directionSign=rand01()<.5f?-1.f:1.f;
  g.length=static_cast<uint32_t>(std::max(2.f,params_.durationMs*.001f*sampleRate_));
  applyGrainTone(g);
  if(params_.keyboardMode==1) {
    // C4 through C5, key A as root, maintaining global PITCH and PITCH RANDOM.
    g.step*=std::pow(2.0,static_cast<double>(index)/12.0);
  }
  const float pan=(rand01()*2.f-1.f)*params_.panSpread;
  g.gl=std::cos((pan+1.)*pi/4.);g.gr=std::sin((pan+1.)*pi/4.);
  g.age=0;g.active=true;g.sliceVoice=true;g.note=static_cast<int>(index);++started_;
  return true;
}
bool Engine::noteOn(unsigned index) {
  if(index>=held_.size() || sample_.size()<2 || (live_ && captured_<256)) return false;
  if(held_[index])return true;
  held_[index]=true;noteGate_[index]=true;retriggerBloom(index);
  return startSliceGrain(index,true);
}
double Engine::arrivalInterval(uint32_t& state) {
  // Blend deterministic spacing with a bounded exponential-like interarrival.
  // Minimum is 0.25 of the nominal interval; maximum 3.5, avoiding pathological bursts.
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  const double u=(static_cast<double>(state)+1.)/4294967297.;
  const double randomInterval=std::clamp(-std::log(u),0.25,3.5);
  const double mixed=(1.-params_.grainTiming)+params_.grainTiming*randomInterval;
  const double jitterFactor=1.+params_.densityJitter*(2.*u-1.)*.35;
  return std::clamp(mixed*jitterFactor,0.25,3.5);
}
float Engine::bloomEnvelope(unsigned k) const {
  const double duration=sampleRate_*params_.grainBloomMs*.001*bloomMultiplier_[k];
  const double x=duration<=0.?1.:std::clamp(static_cast<double>(noteBloomAge_[k])/duration,0.,1.);
  if(params_.bloomCurve==0)return static_cast<float>(x);
  if(params_.bloomCurve==1)return static_cast<float>(x*x*x);
  return static_cast<float>(x*x*(3.-2.*x));
}
void Engine::retriggerBloom(unsigned k) {
  noteBloomAge_[k]=0;
  arrivalRng_[k] ^= (arrivalRng_[k]<<13); arrivalRng_[k]^=(arrivalRng_[k]>>17); arrivalRng_[k]^=(arrivalRng_[k]<<5);
  const float u=static_cast<float>(arrivalRng_[k])/4294967295.f;
  bloomMultiplier_[k]=std::max(.25f,1.f+(u*2.f-1.f)*params_.bloomVariation*.75f);
  notePhase_[k]=0.;
  nextNoteInterval_[k]=arrivalInterval(arrivalRng_[k]);
}
void Engine::noteOff(unsigned index) { if(index<held_.size()) { held_[index]=false; } }
double Engine::readPosition(const Grain& g) const {
  if(params_.travel<=0.0001f) return g.pos; // backwards-compatible playback
  const double n=static_cast<double>(sample_.size());
  if(n<2) return 0;
  // Playback region is unwrapped relative to the start of each slice.
  double width=g.sliceVoice?std::fmod(g.upper-g.lower+n,n):
    (live_?static_cast<double>(captured_):g.upper-g.lower);
  width=std::max(1.0,width);
  const double elapsed=static_cast<double>(g.age)*g.step;
  const double duration=std::max(1.0,static_cast<double>(g.length)*g.step);
  const double u=std::clamp(elapsed/duration,0.0,1.0);
  const double cycles=0.15+params_.speed*3.85;
  const double phase=std::fmod(u*cycles,1.0);
  double m=0;
  switch(params_.direction){
    case Direction::Forward: m=phase;break;
    case Direction::Reverse: m=1.0-phase;break;
    case Direction::Outward: m=g.directionSign*phase;break;
    case Direction::Inward: m=g.directionSign*(1.0-phase);break;
    case Direction::Pingpong: m=g.directionSign*(1.0-std::abs(2.0*phase-1.0));break;
    case Direction::Random: m=g.directionSign*(2.0*phase-1.0);break;
  }
  // Bound the offset within source region. Use circular wrap for live capture.
  double center=g.origin;
  if(params_.direction==Direction::Reverse) center=g.sliceVoice?g.upper:g.origin;
  const double intended=center+(params_.travel*params_.travel)*width*0.12*m+g.movement;
  if(live_) {double v=std::fmod(intended,n);return v<0?v+n:v;}
  const double low=g.sliceVoice?g.lower:g.lower, high=g.sliceVoice?g.upper:g.upper;
  return std::clamp(intended,std::min(low,high),std::max(low,high));
}
Stereo Engine::interpolate(double pos) const {
  const double n=static_cast<double>(sample_.size());
  pos=std::fmod(pos,n); if(pos<0) pos+=n;
  const size_t a=static_cast<size_t>(pos);
  const size_t b=(a+1)%sample_.size();
  const float t=static_cast<float>(pos-a);
  return {sample_[a].l+(sample_[b].l-sample_[a].l)*t,
          sample_[a].r+(sample_[b].r-sample_[a].r)*t};
}
Stereo Engine::readGrain(const Grain& g) const {
  const double position=readPosition(g);
  if(!g.sliceVoice || live_)return interpolate(position);
  const double low=std::min(g.lower,g.upper), high=std::max(g.lower,g.upper);
  const double width=high-low;
  if(width<4)return interpolate(position);
  // Fade toward the exact *next* read location at a source-region wrap.
  // The old implementation faded toward low+fade, then restarted at low:
  // that mismatch created a new discontinuity at the end of the fade.
  const double step=std::max(.0001,std::abs(g.step));
  const double fade=std::min(width*.25,std::max(2.*step,static_cast<double>(sampleRate_)*.006*step));
  const bool reverse=g.step<0.0 || (params_.travel>0.0001f &&
    (params_.direction==Direction::Reverse || params_.direction==Direction::Inward));
  if(!reverse && position>high-fade && position<=high){
    const double u=std::clamp((position-(high-fade))/fade,0.,1.);
    const double w=u*u*(3.-2.*u);
    const Stereo a=interpolate(position),b=interpolate(low);
    return {static_cast<float>(a.l*(1.-w)+b.l*w),static_cast<float>(a.r*(1.-w)+b.r*w)};
  }
  if(reverse && position>=low && position<low+fade){
    const double u=std::clamp((low+fade-position)/fade,0.,1.);
    const double w=u*u*(3.-2.*u);
    const Stereo a=interpolate(position),b=interpolate(high);
    return {static_cast<float>(a.l*(1.-w)+b.l*w),static_cast<float>(a.r*(1.-w)+b.r*w)};
  }
  return interpolate(position);
}
void Engine::render(Stereo* out,size_t frames){
  if(!out) return;
  for(size_t i=0;i<frames;++i){
    ++processedFrames_;
    if(params_.sequencerPlaying){
      const double stepSamples=sampleRate_*60.0/(params_.bpm*4.0);
      if(seqClock_<=0){
        for(unsigned k=0;k<13;++k){
          bool on=(sequence_[seqStep_]&(1u<<k))!=0;
          if(on) {retriggerBloom(k);seqHeld_[k]=true;noteGate_[k]=true;startSliceGrain(k,true);}
          if(!on)seqHeld_[k]=false;
        }
        if(params_.metronome && seqStep_%4==0)clickFrames_=static_cast<int>(sampleRate_*.012f);
        seqStep_=(seqStep_+1)%16;
        seqGateFrames_=static_cast<int>(stepSamples*.65);
        seqClock_+=stepSamples;
      }
      seqClock_-=1;
      if(--seqGateFrames_<=0)seqHeld_.fill(false);
    }
    Stereo sum{};
    float envelopePower=0.f;
    for(size_t k=0;k<held_.size();++k){
      const bool gate=held_[k] || seqHeld_[k];
      const float attackSamples=std::max(1.f,params_.noteAttackMs*.001f*sampleRate_);
      const float releaseSamples=std::max(1.f,params_.noteReleaseMs*.001f*sampleRate_);
      // Attack is a click-safe linear ramp. Release decays smoothly toward zero.
      if(gate) noteGain_[k]=std::min(1.f,noteGain_[k]+1.f/attackSamples);
      else if(noteGain_[k]>0.f){
        noteGain_[k]*=std::exp(-6.9f/releaseSamples);
        if(noteGain_[k]<.001f)noteGain_[k]=0.f;
      }
      if(gate){if(!noteGate_[k])noteBloomAge_[k]=0; if(noteBloomAge_[k]<UINT32_MAX)++noteBloomAge_[k];}else noteBloomAge_[k]=0;
      noteGate_[k]=gate;
    }
    if(sample_.size()>=2){
      if(ambientPlaying_) {
        phase_+=static_cast<double>(params_.density)/sampleRate_;
        if(phase_>=nextAmbientInterval_){startGrain();phase_-=nextAmbientInterval_;nextAmbientInterval_=std::max(.15,1.+(rand01()*2.-1.)*params_.densityJitter*.8);}
      }
      if(params_.densityMode==1){
        // Global density: one clock shared across all currently playable notes.
        // Round-robin selection avoids multiplying grain rate with polyphony.
        double bloomFactor=0.;unsigned bloomNotes=0;
        for(unsigned k=0;k<13;++k) if(held_[k]||seqHeld_[k]){
          bloomFactor+=bloomEnvelope(k);++bloomNotes;
        }
        if(bloomNotes)bloomFactor/=bloomNotes;
        globalNotePhase_+=static_cast<double>(params_.density)*bloomFactor/sampleRate_;
        if(globalNotePhase_>=nextGlobalInterval_){
          globalNotePhase_-=nextGlobalInterval_;
          nextGlobalInterval_=arrivalInterval(globalArrivalRng_);
          const unsigned offset=globalRoundRobin_++%13;
          for(unsigned j=0;j<13;++j){const unsigned k=(j+offset)%13;
            if(held_[k]||seqHeld_[k]){startSliceGrain(k,true);break;}}
        }
      }else{
        for(unsigned k=0;k<held_.size();++k) if(held_[k] || seqHeld_[k]) {
          const double bloom=bloomEnvelope(k);
          notePhase_[k]+=static_cast<double>(params_.density)*bloom/sampleRate_;
          if(notePhase_[k]>=nextNoteInterval_[k]) {startSliceGrain(k,true);notePhase_[k]-=nextNoteInterval_[k];nextNoteInterval_[k]=arrivalInterval(arrivalRng_[k]);}
        }
      }
      for(auto& g:grains_){
        if(!g.active) continue;
        const double u=static_cast<double>(g.age)/(g.length-1);
        const float env=envelope(g)*(g.note>=0?noteGain_[static_cast<size_t>(g.note)]:1.f);
        const Stereo s=readGrain(g);
        sum.l+=s.l*env*g.gl; sum.r+=s.r*env*g.gr;
        envelopePower+=env*env*0.5f*(g.gl*g.gl+g.gr*g.gr);
        g.pos+=g.step;
        if(g.sliceVoice && !live_) {
          const double loSlice=std::min(g.lower,g.upper),hiSlice=std::max(g.lower,g.upper);
          if(hiSlice>loSlice+1.0 && (g.pos>hiSlice || g.pos<loSlice)) {
            double wrapped=std::fmod(g.pos-loSlice,hiSlice-loSlice);
            if(wrapped<0)wrapped+=hiSlice-loSlice;
            g.pos=loSlice+wrapped;
          }
        }
        const double lo=params_.regionStart*(sample_.size()-1), hi=params_.regionEnd*(sample_.size()-1);
        if(!g.sliceVoice && !live_ && g.pos>hi) g.pos=lo+std::fmod(g.pos-lo,hi-lo+1.0);
        if(++g.age>=g.length)g.active=false;
      }
    }
    // Short crossfade on stolen grain, evaluated with fixed storage and no allocation.
    for(auto& t:stealTails_){
      if(!t.active)continue;
      const float fade=1.f-static_cast<float>(t.age)/std::max(1.f,sampleRate_*.003f);
      const Grain& g=t.grain;
      if(fade>0.f && g.age<g.length && sample_.size()>1){
        const Stereo v=readGrain(g);
        const float a=envelope(g)*fade*(g.note>=0?noteGain_[static_cast<size_t>(g.note)]:1.f);
        sum.l+=v.l*a*g.gl;sum.r+=v.r*a*g.gr;
        envelopePower+=a*a*0.5f*(g.gl*g.gl+g.gr*g.gr);
        ++t.grain.age;t.grain.pos+=t.grain.step;
      }
      if(++t.age>=static_cast<uint32_t>(std::max(1.f,sampleRate_*.003f)) || t.grain.age>=t.grain.length)t.active=false;
    }
    // Soft saturation controls high-density summing, output remains bounded.
    if(clickFrames_>0 && params_.metronome){
      const float click=std::sin(2.f*static_cast<float>(pi)*1350.f*clickFrames_/sampleRate_)*params_.clickVolume*static_cast<float>(clickFrames_)/(sampleRate_*.012f);
      sum.l+=click;sum.r+=click;--clickFrames_;
    }
    // Constant-power overlap compensation, with asymmetric smoothing to avoid pumping.
    const float target=1.f/std::sqrt(std::max(1.f,envelopePower));
    const float smoothing=(target<mixGain_)?0.008f:0.00035f;
    mixGain_+=smoothing*(target-mixGain_);
    const float left=sum.l*params_.gain*mixGain_,right=sum.r*params_.gain*mixGain_;
    preLimiterPeak_=std::max(preLimiterPeak_,std::max(std::abs(left),std::abs(right)));
    if(std::abs(left)>1.f||std::abs(right)>1.f)++preLimiterOvers_;
    out[i]=processFx({std::tanh(left),std::tanh(right)});
  }
}
// Lightweight pitch-shifted shimmer feed using two windowed, overlapping read heads.
// This is an intentionally characterful time-domain pitch shifter, not FFT.
Stereo Engine::shimmerPitchInput(Stereo dry){
  const float mono=0.5f*(dry.l+dry.r);
  shimmerRing_[shimmerWrite_]=mono;
  constexpr float window=2048.f;
  const float ratio=std::pow(2.f,static_cast<float>(params_.shimmerInterval)/12.f);
  const float step=1.f-ratio; // decreasing delay distance raises playback pitch
  auto tap=[&](float offset){
    float phase=std::fmod((static_cast<float>(shimmerFrame_%65536)*step+offset)/window,1.f);
    if(phase<0)phase+=1.f;
    const float distance=128.f+phase*window;
    float at=static_cast<float>(shimmerWrite_)+shimmerRing_.size()-distance;
    while(at>=shimmerRing_.size())at-=shimmerRing_.size();
    while(at<0)at+=shimmerRing_.size();
    const size_t a=static_cast<size_t>(at), b=(a+1)%shimmerRing_.size();
    const float raw=shimmerRing_[a]*(1.f-(at-a))+shimmerRing_[b]*(at-a);
    const float fade=0.5f-0.5f*std::cos(2.f*3.14159265358979323846f*phase);
    return std::pair<float,float>{raw*fade,fade};
  };
  const auto a=tap(0.f), b=tap(window*0.5f);
  float pitched=(a.first+b.first)/std::max(0.01f,a.second+b.second);
  const float coefficient=.015f+.35f*params_.shimmerTone;
  shimmerLowpass_+=coefficient*(pitched-shimmerLowpass_);
  shimmerWrite_=(shimmerWrite_+1)%shimmerRing_.size();
  ++shimmerFrame_;
  return {shimmerLowpass_,shimmerLowpass_};
}
Stereo Engine::processFx(Stereo dry){
  if(params_.fxType==0 || params_.fxMix<=0.f) return dry;
  const int mode=params_.fxType;
  const float sr=sampleRate_;
  float seconds=(mode==4?(.09f+params_.fxTime*1.5f):(.035f+params_.fxTime*.18f));
  if(mode==5){static constexpr float beats[]={1.f,.5f,.25f,.75f,1.f/3.f};seconds=60.f/params_.bpm*beats[params_.delayDivision];}
  if(mode==6) seconds=.085f+params_.fxTime*.26f;
  size_t delay=std::clamp<size_t>(static_cast<size_t>(seconds*sr),1,fxBuffer_.size()-1);
  auto read=[&](size_t d){return fxBuffer_[(fxWrite_+fxBuffer_.size()-d)%fxBuffer_.size()];};
  Stereo wet=read(delay);
  if(mode!=4 && mode!=5){
    auto a=read(std::max<size_t>(1,delay*3/5));
    auto b=read(std::max<size_t>(1,delay*4/5));
    auto cc=read(std::min(fxBuffer_.size()-1,delay+delay/3));
    const float diffusion=(mode==1?.34f:mode==2?.48f:mode==6?.54f:.22f);
    wet.l=wet.l*(1-diffusion)+diffusion*(a.r+b.l+cc.r)/3.f;
    wet.r=wet.r*(1-diffusion)+diffusion*(a.l+b.r+cc.l)/3.f;
  }
  const float fb=std::min(.92f,params_.fxFeedback*(mode==1?1.25f:mode==2?1.05f:mode==3?.82f:1.f));
  Stereo feed=dry;
  Stereo shifted{};
  if(mode==6){
    // Shimmer must be driven by an audible source, not only delayed recirculation.
    // Use dry + a restrained portion of wet as pitch-shifter input; the shifted
    // signal is present both in the output and in the delay feedback network.
    const Stereo source={dry.l+wet.l*.3f,dry.r+wet.r*.3f};
    shifted=shimmerPitchInput(source);
    feed.l+=shifted.l*params_.shimmerAmount*.7f;
    feed.r+=shifted.r*params_.shimmerAmount*.7f;
  }
  fxBuffer_[fxWrite_]={std::clamp(feed.l+wet.r*fb,-2.f,2.f),std::clamp(feed.r+wet.l*fb,-2.f,2.f)};
  if(mode==5){float left=wet.l;wet.l=wet.r;wet.r=left;}
  fxWrite_=(fxWrite_+1)%fxBuffer_.size();
  const float mix=params_.fxMix;
  if(mode==6){
    // Explicit pitch-shifted component in audible wet mix. Setting amount to
    // zero gives a conventional reverb; setting 1 maximizes pitched wet signal.
    const float a=params_.shimmerAmount;
    const Stereo shimmerWet={wet.l*(1.f-a*.7f)+shifted.l*a*.95f,
                             wet.r*(1.f-a*.7f)+shifted.r*a*.95f};
    return {std::tanh(dry.l*(1-mix)+shimmerWet.l*mix),
            std::tanh(dry.r*(1-mix)+shimmerWet.r*mix)};
  }
  return {std::tanh(dry.l*(1-mix)+wet.l*mix),std::tanh(dry.r*(1-mix)+wet.r*mix)};
}
size_t Engine::activeGrains() const {size_t n=0;for(const auto& g:grains_)n+=g.active?1u:0u;return n;}
}

namespace liquid {
size_t Engine::grainVisuals(GrainVisual* out, size_t capacity) const {
 if(!out || !capacity || sample_.size()<2) return 0;
 size_t written=0;
 const size_t n=live_?captured_:sample_.size();
 if(n<2) return 0;
 const size_t first=live_?(writeHead_+sample_.size()-captured_)%sample_.size():0;
 for(size_t slot=0;slot<grains_.size();++slot) {
  const auto& g=grains_[slot];
  if(!g.active || written>=capacity) continue;
  double logical=readPosition(g);
  if(live_) {
   logical=std::fmod(g.pos-first+sample_.size(),static_cast<double>(sample_.size()));
   if(logical>=static_cast<double>(n)) continue;
  }
  const float phase=static_cast<float>(g.age)/std::max(1u,g.length-1);
  const float env=envelope(g);
  const Stereo smp=interpolate(readPosition(g));
  const float amp=std::clamp(std::max(std::abs(smp.l),std::abs(smp.r))*env,0.0f,1.0f);
  out[written++]={static_cast<float>(logical/std::max<size_t>(1,n-1)),amp,phase,static_cast<float>(g.length/sampleRate_),static_cast<uint32_t>(slot)};
 }
 return written;
}
}
