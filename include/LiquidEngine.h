#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace liquid {
struct Stereo { float l=0.0f, r=0.0f; };
enum class Direction { Forward=0, Reverse=1, Outward=2, Inward=3, Pingpong=4, Random=5 };
struct Params {
  Direction direction=Direction::Forward;
  float travel=0.0f; // 0 retains historical playback; 1 traverses slice/region
  float speed=0.5f; // rate of movement for directional read head
  float spread=0.0f; // controlled per-grain source-position deviation
  float attack=0.16f;       // fraction of grain duration (0..1)
  float release=0.24f;      // fraction of grain duration (0..1)
  int shape=0;              // 0 smooth, 1 linear, 2 exponential
  float densityJitter=0.0f;
  float pitchRandom=0.0f;  // semitones
  int densityMode=0; // 0 per-note, 1 global shared note scheduler
  int maxActiveGrains=128; // 1..128 preallocated voices
  int bloomCurve=2; // 0 linear, 1 exponential, 2 organic
  float bloomVariation=0.25f; // 0..1 per-trigger bloom-time variation
  float grainTiming=0.65f; // 0 regular, 1 organic arrival timing
  float grainBloomMs=0.0f; // 0..3000ms rate ramp per note
  float density=20.0f;       // grains per second
  float durationMs=90.0f;    // duration of each grain
  float pitchSemitones=0.0f;
  float position=0.5f;       // normalized sample position
  float spray=0.10f;         // random position spread normalized to total sample length
  float regionStart=0.0f;  // normalized loop start
  float regionEnd=1.0f;    // normalized loop end
  int keyboardMode=0; // 0 sliced positions, 1 chromatic pitches from fixed source position
  float sliceSize=1.0f; // 0.01 to 8x nominal width // fraction of the nominal slice width: 0.1 .. 2
  float noteReleaseMs=500.0f;
  float noteAttackMs=5.0f;
  int fxType=0; // 0 off, 1 hall, 2 plate, 3 room, 4 delay, 5 pingpong, 6 shimmer
  float bpm=120.0f;
  bool sequencerPlaying=false, metronome=false;
  float clickVolume=0.2f;
  int delayDivision=2; // 0 quarter 1 eighth 2 sixteenth 3 dotted eighth 4 eighth triplet
  float fxMix=0.0f, fxTime=0.45f, fxFeedback=0.38f;
  float shimmerAmount=0.60f, shimmerTone=0.65f;
  int shimmerInterval=12; // 7, 12, or 19 semitones
  float panSpread=0.6f;      // stereo spread 0..1
  float gain=0.5f;
  bool freeze=false;         // disables position drift, not grain playback
};
struct GrainVisual { float position=0, amplitude=0, phase=0, duration=0; uint32_t slot=0; };
class Engine {
 public:
  static constexpr size_t kMaxGrains=128;
  explicit Engine(float sampleRate=48000.0f);
  void setSample(const std::vector<Stereo>& sample); // COPY on control thread only
  void setSample(std::vector<Stereo>&& sample);       // MOVE on control thread only
  void setParams(Params p);                          // call between audio blocks
  Params params() const { return params_; }
  void reset();
  void beginLive(size_t frames); // preallocate ring once, outside callback
  void pushLive(const Stereo* input, size_t frames); // callback-safe when serialized with render
  void stopLive();
  std::vector<Stereo> snapshot() const; // ordered snapshot; caller synchronizes with callbacks
  bool live() const { return live_; }
  size_t capturedFrames() const { return captured_; }
  bool triggerSlice(unsigned index, unsigned count=13);
  bool noteOn(unsigned index);
  void noteOff(unsigned index);
  void setSequenceStep(unsigned step, uint16_t mask);
  std::array<uint16_t,16> sequence() const {return sequence_;}
  uint64_t processedFrames() const {return processedFrames_;}
  void setAmbientPlaying(bool enabled) { ambientPlaying_=enabled; }
  bool noteHeld(unsigned index) const { return index<13 && held_[index]; } // one-shot chromatic slice pad; audio callback serialized externally
  void render(Stereo* output, size_t frames);         // allocation-free audio callback
  size_t activeGrains() const;
  size_t maxActiveGrains() const { return static_cast<size_t>(params_.maxActiveGrains); }
  size_t sampleFrames() const { return sample_.size(); }
  size_t grainVisuals(GrainVisual* out, size_t capacity) const; // UI thread, synchronized by caller
  uint64_t grainsStarted() const { return started_; }
  uint64_t grainsDropped() const { return dropped_; }
  uint64_t grainsStolen() const {return stolen_;}
  uint64_t preLimiterOvers() const {return preLimiterOvers_;}
  float preLimiterPeak() const {return preLimiterPeak_;}
  float mixReduction() const {return mixGain_;}
  float sampleRate() const { return sampleRate_; }
 private:
  struct Grain { double pos=0, step=1; uint32_t age=0, length=0; float gl=0, gr=0; bool active=false, sliceVoice=false; int note=-1; double sliceEnd=0, origin=0, lower=0, upper=0, movement=0; float directionSign=1; float envAttack=0,envRelease=0; int envShape=0; };
  float rand01();
  void applyGrainTone(Grain& g);
  float envelope(const Grain& g) const;
  Grain* allocateGrain();
  void saveStealTail(const Grain& grain);
  void startGrain();
  bool startSliceGrain(unsigned index, bool sustained);
  Stereo interpolate(double position) const;
  Stereo readGrain(const Grain& grain) const;
  double readPosition(const Grain& grain) const;
  float sampleRate_;
  Params params_;
  std::vector<Stereo> sample_;
  std::array<Grain,kMaxGrains> grains_{};
  struct StealTail { Grain grain{}; uint32_t age=0; bool active=false; };
  std::array<StealTail,16> stealTails_{};
  static constexpr uint32_t kStealFadeFrames=144; // 3 ms at 48 kHz, scaled in render
  bool live_=false;
  size_t writeHead_=0, captured_=0;
  double phase_=0.0; double nextAmbientInterval_=1.0;
  bool ambientPlaying_=true;
  std::array<bool,13> held_{};
  std::array<uint16_t,16> sequence_{};
  std::array<bool,13> seqHeld_{};
  double seqClock_=0;
  unsigned seqStep_=0;
  int seqGateFrames_=0;
  int clickFrames_=0;
  uint64_t processedFrames_=0;
  std::array<float,13> noteGain_{};
  std::array<uint32_t,13> noteBloomAge_{};
  std::array<float,13> bloomMultiplier_{};
  std::array<uint32_t,13> arrivalRng_{};
  uint32_t globalArrivalRng_=0xCAB12345u;
  double nextGlobalInterval_=1.0;
  unsigned globalRoundRobin_=0;
  float bloomEnvelope(unsigned k) const;
  double arrivalInterval(uint32_t& state);
  void retriggerBloom(unsigned k);
  float mixGain_=1.f, preLimiterPeak_=0.f;
  uint64_t preLimiterOvers_=0;
  std::array<bool,13> noteGate_{};
  std::array<Stereo,96000> fxBuffer_{};
  size_t fxWrite_=0;
  std::array<float,16384> shimmerRing_{};
  size_t shimmerWrite_=0;
  uint64_t shimmerFrame_=0;
  float shimmerLowpass_=0.f;
  Stereo shimmerPitchInput(Stereo dry);
  Stereo processFx(Stereo dry);
  std::array<double,13> notePhase_{}; std::array<double,13> nextNoteInterval_{};
  size_t lastSliceSlot_=0;
  uint32_t rng_=0x12345678u;
  uint64_t started_=0, dropped_=0, stolen_=0;
  double globalNotePhase_=0;
};
}
