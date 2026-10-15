#include "LiquidEngine.h"
#include <vector>
#include <cstdint>
using liquid::Engine;using liquid::Stereo;using liquid::Params;using liquid::GrainVisual;
static Engine* engine=nullptr;
static std::vector<Stereo> staged;
static Stereo audio[128];
static GrainVisual visuals[128];
static Params config;
// Worklet owns all calls; no UI/audio cross-thread locking is necessary.
extern "C" {
__attribute__((visibility("default"))) int app_init(float rate){if(engine)return 1;static Engine instance(rate);engine=&instance;engine->setAmbientPlaying(false);config=engine->params();return 1;}
__attribute__((visibility("default"))) uintptr_t app_prepare_sample(int frames){if(!engine||frames<2||frames>960000)return 0;staged.resize(frames);return (uintptr_t)staged.begin();}
__attribute__((visibility("default"))) void app_commit_sample(){if(engine && staged.size()>1){engine->setSample(std::move(staged));staged=std::vector<Stereo>();}}
__attribute__((visibility("default"))) void app_param(int id, float value){if(!engine)return;switch(id){
 case 0:config.density=value;break;case 1:config.grainBloomMs=value;break;case 2:config.grainTiming=value;break;case 3:config.bloomVariation=value;break;
 case 4:config.maxActiveGrains=(int)value;break;case 5:config.durationMs=value;break;case 6:config.spray=value;break;case 7:config.pitchSemitones=value;break;
 case 8:config.position=value;break;case 9:config.gain=value;break;case 10:config.attack=value;break;case 11:config.release=value;break;
 case 12:config.noteReleaseMs=value;break;case 13:config.densityJitter=value;break;case 14:config.pitchRandom=value;break;
 case 15:config.travel=value;break;case 16:config.speed=value;break;case 17:config.spread=value;break;
 case 18:config.fxType=(int)value;break;case 19:config.fxMix=value;break;case 20:config.fxTime=value;break;case 21:config.fxFeedback=value;break;
 case 22:config.shimmerAmount=value;break;case 23:config.shimmerTone=value;break;case 24:config.shimmerInterval=(int)value;break;
 case 25:config.regionStart=value;break;case 26:config.regionEnd=value;break;case 27:config.keyboardMode=(int)value;break;case 28:config.sliceSize=value;break;
 case 29:config.bpm=value;break;case 30:config.sequencerPlaying=(value!=0);break;case 31:config.metronome=(value!=0);break;case 32:config.clickVolume=value;break;
 case 33:config.delayDivision=(int)value;break;case 34:config.densityMode=(int)value;break;case 35:config.freeze=(value!=0);break;
 case 36:engine->setAmbientPlaying(value!=0);break;case 37:config.direction=(liquid::Direction)(int)value;break;
 case 38:config.shape=(int)value;break;case 39:config.bloomCurve=(int)value;break;
 }if(id!=36)engine->setParams(config);}
__attribute__((visibility("default"))) void app_note(int index,int on){if(!engine||index<0||index>=13)return;if(on)engine->noteOn(index);else engine->noteOff(index);}
__attribute__((visibility("default"))) void app_step(int step,int mask){if(engine&&step>=0&&step<16)engine->setSequenceStep(step,(uint16_t)mask);}
__attribute__((visibility("default"))) uintptr_t app_audio_ptr(){return (uintptr_t)audio;}
__attribute__((visibility("default"))) void app_render(){if(engine)engine->render(audio,128);}
__attribute__((visibility("default"))) uintptr_t app_visual_ptr(){return (uintptr_t)visuals;}
__attribute__((visibility("default"))) int app_visual_count(){return engine?(int)engine->grainVisuals(visuals,128):0;}
__attribute__((visibility("default"))) int app_active(){return engine?(int)engine->activeGrains():0;}
__attribute__((visibility("default"))) int app_dropped(){return engine?(int)engine->grainsDropped():0;}
__attribute__((visibility("default"))) int app_started(){return engine?(int)engine->grainsStarted():0;}
__attribute__((visibility("default"))) int app_stolen(){return engine?(int)engine->grainsStolen():0;}
__attribute__((visibility("default"))) int app_overs(){return engine?(int)engine->preLimiterOvers():0;}
__attribute__((visibility("default"))) float app_peak(){return engine?engine->preLimiterPeak():0;}
}
