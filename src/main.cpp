#include "LiquidEngine.h"
#include "Wav.h"
#include "AudioImport.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#endif
static std::mutex audioMutex;
static liquid::Engine engine(48000);
static liquid::Params settings;
static std::string sampleName="Generated test tone";
static size_t sampleFrames=0;
static double sourceDuration=0;
static std::FILE* pcmBacking=nullptr;
static size_t pcmTotalFrames=0,cacheStartFrame=0;
static std::vector<float> peaks;
static float outputLevel=0, inputLevel=0;
static std::vector<liquid::Stereo> lastCapture;
static std::atomic<bool> playing{true};
static std::atomic<bool> recording{false};
static bool captured=false;
static bool inputReady=false;
static std::atomic<bool> freezeInput{false};
static std::string captureError;
static std::vector<liquid::Stereo> processedCapture;
static bool capturingOutput=false;
static bool completedOutput=false;
static uint64_t callbackMaxUs=0,callbackTotalUs=0,callbackCount=0,callbackMisses=0;


static std::string root="web";
static std::string jsonEscape(const std::string&s){std::string v;for(char c:s){if(c=='"'||c=='\\')v+='\\';if(static_cast<unsigned char>(c)>=32)v+=c;}return v;}
// Control-thread cache management. Disk I/O never occurs in render().
static constexpr size_t kCacheFrames=48000*20;
static std::vector<liquid::Stereo> readCacheWindow(size_t start){
  std::vector<liquid::Stereo> result;
  if(!pcmBacking || !pcmTotalFrames)return result;
  start=std::min(start,pcmTotalFrames-1);
  const size_t count=std::min(kCacheFrames,pcmTotalFrames-start);
  result.resize(count);
  std::fseek(pcmBacking,static_cast<long>(start*4),SEEK_SET);
  int16_t pair[2];
  for(size_t i=0;i<count;++i){
    if(std::fread(pair,sizeof(int16_t),2,pcmBacking)!=2){result.resize(i);break;}
    result[i]={pair[0]/32768.f,pair[1]/32768.f};
  }
  return result;
}
static std::vector<float> cachePeaks(const std::vector<liquid::Stereo>& converted){
  std::vector<float> p(8192,0);
  for(size_t i=0;i<converted.size();++i){size_t k=std::min(size_t(8191),i*8192/std::max(size_t(1),converted.size()));p[k]=std::max(p[k],std::max(std::abs(converted[i].l),std::abs(converted[i].r)));}
  return p;
}
static bool seekCache(double seconds){
  if(!pcmBacking||!std::isfinite(seconds)||seconds<0)return false;
  // Disk read happens before acquiring audio mutex; no callback disk I/O.
  const size_t start=std::min(static_cast<size_t>(seconds*48000),pcmTotalFrames-1);
  auto converted=readCacheWindow(start);
  if(converted.size()<2)return false;
  auto p=cachePeaks(converted);
  std::lock_guard<std::mutex> lock(audioMutex);
  engine.setSample(std::move(converted));cacheStartFrame=start;
  sampleFrames=engine.sampleFrames();peaks=std::move(p);
  return true;
}
static void loadSample(DecodedWav&& wav,const std::string& name){
  if(wav.audio.empty()||wav.sampleRate<1)return;
  const double originalDuration=static_cast<double>(wav.audio.size())/wav.sampleRate;
  const size_t frames=static_cast<size_t>(wav.audio.size()*48000.0/wav.sampleRate);
  // Store resampled PCM16 in temporary disk backing, not resident float samples.
  std::FILE* backing=std::tmpfile();
  if(!backing){std::cerr<<"Unable to open temporary PCM cache\n";return;}
  for(size_t i=0;i<frames;++i){
    double pos=i*static_cast<double>(wav.sampleRate)/48000.;
    size_t a=std::min(static_cast<size_t>(pos),wav.audio.size()-1),b=std::min(a+1,wav.audio.size()-1);
    float t=static_cast<float>(pos-a);
    float l=wav.audio[a].l*(1-t)+wav.audio[b].l*t;
    float r=wav.audio[a].r*(1-t)+wav.audio[b].r*t;
    int16_t pair[2]={static_cast<int16_t>(std::lrint(std::clamp(l,-1.f,1.f)*32767.f)),static_cast<int16_t>(std::lrint(std::clamp(r,-1.f,1.f)*32767.f))};
    if(std::fwrite(pair,sizeof(int16_t),2,backing)!=2){std::fclose(backing);return;}
  }
  // Decoder's full-size floating point allocation goes away after this function.
  if(pcmBacking)std::fclose(pcmBacking);
  pcmBacking=backing;pcmTotalFrames=frames;
  auto converted=readCacheWindow(0);auto p=cachePeaks(converted);
  std::lock_guard<std::mutex> lock(audioMutex);
  engine.setSample(std::move(converted));cacheStartFrame=0;sampleFrames=engine.sampleFrames();
  sourceDuration=originalDuration;peaks=std::move(p);sampleName=name;
  captured=false;recording=false;lastCapture.clear();
}
static void tone(){DecodedWav w;w.sampleRate=48000;w.audio.resize(48000*4);for(size_t i=0;i<w.audio.size();++i){float v=.28f*std::sin(2*3.141592653589793*220*i/48000);w.audio[i]={v,v};}loadSample(std::move(w),"Generated test tone");}
#ifdef __APPLE__
static AudioQueueRef queueRef=nullptr, inputQueue=nullptr;
static void inputFill(void*,AudioQueueRef q,AudioQueueBufferRef b,const AudioTimeStamp*,UInt32,const AudioStreamPacketDescription*) {
  if(recording.load(std::memory_order_relaxed) && !freezeInput.load(std::memory_order_relaxed) && b->mAudioDataByteSize>=4){
    auto* x=static_cast<const int16_t*>(b->mAudioData);
    const size_t count=b->mAudioDataByteSize/4;
    liquid::Stereo mono[1024];
    for(size_t i=0;i<count;i+=1024){size_t n=std::min(size_t(1024),count-i);
      for(size_t j=0;j<n;j++)mono[j]={x[(i+j)*2]/32768.f,x[(i+j)*2+1]/32768.f};
      std::lock_guard<std::mutex> lock(audioMutex);engine.pushLive(mono,n);
      float energy=0;for(size_t k=0;k<n;k++)energy+=.5f*(mono[k].l*mono[k].l+mono[k].r*mono[k].r);
      inputLevel=.78f*inputLevel+.22f*std::sqrt(energy/std::max(size_t(1),n));
    }
  }
  AudioQueueEnqueueBuffer(q,b,0,nullptr);
}
static bool startInput() {
  AudioStreamBasicDescription f{};f.mSampleRate=48000;f.mFormatID=kAudioFormatLinearPCM;
  f.mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked;
  f.mBitsPerChannel=16;f.mChannelsPerFrame=2;f.mFramesPerPacket=1;f.mBytesPerFrame=4;f.mBytesPerPacket=4;
  OSStatus st=AudioQueueNewInput(&f,inputFill,nullptr,nullptr,nullptr,0,&inputQueue);
  if(st){captureError="Audio input initialization error "+std::to_string(st);return false;}
  for(int i=0;i<4;i++){AudioQueueBufferRef b=nullptr;if(AudioQueueAllocateBuffer(inputQueue,4096,&b))return false;AudioQueueEnqueueBuffer(inputQueue,b,0,nullptr);}
  st=AudioQueueStart(inputQueue,nullptr);
  if(st){captureError="Microphone unavailable (AudioQueue error "+std::to_string(st)+"). Check macOS microphone permission for Terminal.";return false;}
  inputReady=true;return true;
}

static void fill(void*,AudioQueueRef q,AudioQueueBufferRef b){constexpr size_t frames=512;liquid::Stereo temp[frames];{
  const auto tic=std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(audioMutex);engine.render(temp,frames);
  if(capturingOutput){size_t take=std::min(frames,processedCapture.capacity()-processedCapture.size());processedCapture.insert(processedCapture.end(),temp,temp+take);if(take<frames){capturingOutput=false;completedOutput=true;}}
  const auto us=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-tic).count());
  callbackMaxUs=std::max(callbackMaxUs,us);callbackTotalUs+=us;++callbackCount;if(us>10667)++callbackMisses;
  float energy=0;for(const auto &v:temp)energy+=.5f*(v.l*v.l+v.r*v.r);
  outputLevel=.72f*outputLevel+.28f*std::sqrt(energy/frames);
}auto* dst=static_cast<int16_t*>(b->mAudioData);for(size_t i=0;i<frames;++i){dst[i*2]=int16_t(std::clamp(temp[i].l,-1.f,1.f)*32767);dst[i*2+1]=int16_t(std::clamp(temp[i].r,-1.f,1.f)*32767);}b->mAudioDataByteSize=frames*4;AudioQueueEnqueueBuffer(q,b,0,nullptr);}
static bool startAudio(){AudioStreamBasicDescription f{};f.mSampleRate=48000;f.mFormatID=kAudioFormatLinearPCM;f.mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked;f.mBitsPerChannel=16;f.mChannelsPerFrame=2;f.mFramesPerPacket=1;f.mBytesPerFrame=4;f.mBytesPerPacket=4;
  OSStatus st=AudioQueueNewOutput(&f,fill,nullptr,nullptr,nullptr,0,&queueRef);if(st){std::cerr<<"AudioQueueNewOutput error "<<st<<"\n";return false;}for(int i=0;i<4;++i){AudioQueueBufferRef b=nullptr;if(AudioQueueAllocateBuffer(queueRef,512*4,&b))return false;fill(nullptr,queueRef,b);}st=AudioQueueStart(queueRef,nullptr);if(st){std::cerr<<"AudioQueueStart error "<<st<<"\n";return false;}return true;}
#else
static bool startInput(){captureError="Live recording requires macOS";return false;}
static bool startAudio(){std::cerr<<"Audio playback requires macOS. HTTP UI runs in preview-only mode here.\n";return true;}
#endif
static std::vector<float> downsamplePeaks(const std::vector<liquid::Stereo>& data,size_t count=256){
 std::vector<float> v(count,0); if(data.empty())return v;
 for(size_t i=0;i<count;++i){size_t a=i*data.size()/count,b=std::min(data.size(),(i+1)*data.size()/count);for(size_t j=a;j<b;++j)v[i]=std::max(v[i],std::max(std::abs(data[j].l),std::abs(data[j].r)));}
 return v;
}
static std::string wavBytes(const std::vector<liquid::Stereo>& audio){
 const uint32_t dataSize=static_cast<uint32_t>(audio.size()*4);std::string out;out.reserve(44+dataSize);
 auto u16=[&](uint16_t x){out.push_back(char(x));out.push_back(char(x>>8));};
 auto u32=[&](uint32_t x){u16(uint16_t(x));u16(uint16_t(x>>16));};
 out+="RIFF";u32(36+dataSize);out+="WAVEfmt ";u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);out+="data";u32(dataSize);
 for(auto s:audio){auto enc=[&](float v){u16(static_cast<uint16_t>(static_cast<int16_t>(std::lrint(std::clamp(v,-1.f,1.f)*32767))));};enc(s.l);enc(s.r);}return out;
}
static std::string readFile(const std::string& path){std::ifstream in(path,std::ios::binary);return in?std::string((std::istreambuf_iterator<char>(in)),{}):"";}
static std::string response(int code,const std::string& mime,const std::string& body){std::string status=code==200?"200 OK":code==400?"400 Bad Request":"404 Not Found";return "HTTP/1.1 "+status+"\r\nContent-Type: "+mime+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: http://127.0.0.1:8765\r\n\r\n"+body;}
static double num(const std::string& s,const std::string&key,double fallback){auto start=s.find(key+"=");if(start==std::string::npos)return fallback;start+=key.size()+1;char*end=nullptr;double v=strtod(s.c_str()+start,&end);return end!=s.c_str()+start&&std::isfinite(v)?v:fallback;}
static std::string handle(const std::string&method,const std::string&path,const std::vector<uint8_t>&body){
 if(method=="GET"&&(path=="/"||path=="/index.html")){auto h=readFile(root+"/index.html");return response(h.empty()?404:200,"text/html; charset=utf-8",h);}
 if(method=="POST"&&path.rfind("/cache/seek?",0)==0){
   const double seconds=num(path.substr(12),"seconds",-1);
   bool ok=seekCache(seconds);
   if(!ok)return response(400,"application/json","{\"error\":\"Cannot seek cache: load a sample before seeking\"}");
   std::ostringstream reply;reply<<"{\"ok\":true,\"cacheStart\":"<<cacheStartFrame/48000.0<<",\"cacheSeconds\":"<<sampleFrames/48000.0<<"}";
   return response(200,"application/json",reply.str());
 }
 if(method=="GET"&&path=="/grains"){
  std::lock_guard<std::mutex> g(audioMutex);
  liquid::GrainVisual visual[liquid::Engine::kMaxGrains];
  const size_t n=engine.grainVisuals(visual,liquid::Engine::kMaxGrains);
  std::ostringstream os;os<<"{\"grains\":[";
  for(size_t i=0;i<n;i++){if(i)os<<",";os<<"["<<visual[i].position<<","<<visual[i].amplitude<<","<<visual[i].phase<<","<<visual[i].duration<<","<<visual[i].slot<<"]";}
  os<<"]}";return response(200,"application/json",os.str());
 }
 if(method=="GET"&&path=="/status"){std::lock_guard<std::mutex>g(audioMutex);std::ostringstream s;s<<"{\"name\":\""<<jsonEscape(sampleName)<<"\",\"duration\":"<<(recording?engine.capturedFrames():sampleFrames)/48000.0<<",\"sourceDuration\":"<<sourceDuration<<",\"cacheSeconds\":"<<sampleFrames/48000.0<<",\"cacheStart\":"<<cacheStartFrame/48000.0<<",\"active\":"<<engine.activeGrains()<<",\"started\":"<<engine.grainsStarted()<<",\"dropped\":"<<engine.grainsDropped()<<",\"stolen\":"<<engine.grainsStolen()<<",\"playing\":"<<(playing?"true":"false")<<",\"recording\":"<<(recording?"true":"false")<<",\"captureError\":\""<<jsonEscape(captureError)<<"\",\"inputLevel\":"<<inputLevel<<",\"callbackAvgUs\":"<<(callbackCount?callbackTotalUs/callbackCount:0)<<",\"callbackMaxUs\":"<<callbackMaxUs<<",\"callbackMisses\":"<<callbackMisses<<",\"outputLevel\":"<<outputLevel<<",\"canExport\":"<<(!lastCapture.empty()?"true":"false")<<",\"outputRecording\":"<<(capturingOutput?"true":"false")<<",\"outputReady\":"<<(!processedCapture.empty()?"true":"false")<<",\"outputFrames\":"<<processedCapture.size()<<",\"peaks\":[";auto displayPeaks=recording?downsamplePeaks(engine.snapshot(),8192):peaks;for(size_t i=0;i<displayPeaks.size();++i){if(i)s<<",";s<<displayPeaks[i];}s<<"],\"grains\":[";
 liquid::GrainVisual visual[liquid::Engine::kMaxGrains];
 size_t nv=engine.grainVisuals(visual,liquid::Engine::kMaxGrains);
 for(size_t i=0;i<nv;++i){if(i)s<<",";s<<"["<<visual[i].position<<","<<visual[i].amplitude<<","<<visual[i].phase<<","<<visual[i].duration<<","<<visual[i].slot<<"]";}
 s<<"]}";return response(200,"application/json",s.str());}
 if(method=="POST"&&path.rfind("/slice?",0)==0){
   const std::string args=path.substr(7);
   const double value=num(args,"index",-1);
   if(!std::isfinite(value)||value<0||value>=13||std::floor(value)!=value)return response(400,"application/json","{\"error\":\"Invalid slice index\"}");
   std::lock_guard<std::mutex> g(audioMutex);
   const std::string action = args.find("action=off")!=std::string::npos?"off":
                              (args.find("action=on")!=std::string::npos?"on":"tap");
   bool ok=true;
   if(action=="off") engine.noteOff(static_cast<unsigned>(value));
   else if(action=="on") ok=engine.noteOn(static_cast<unsigned>(value));
   else ok=engine.triggerSlice(static_cast<unsigned>(value),13);
   return response(ok?200:400,"application/json",ok?"{\"ok\":true}":"{\"error\":\"No sample or region too short\"}");
 }
 if(method=="POST"&&path.rfind("/step?",0)==0){
   unsigned step=static_cast<unsigned>(num(path,"step",99));
   unsigned note=static_cast<unsigned>(num(path,"note",99));
   bool on=num(path,"on",0)>.5;
   if(step>=16||note>=13)return response(400,"application/json","{\"error\":\"Invalid step/note\"}");
   std::lock_guard<std::mutex> lock(audioMutex);auto seq=engine.sequence();uint16_t mask=seq[step];if(on)mask|=(1u<<note);else mask&=~(1u<<note);engine.setSequenceStep(step,mask);
   return response(200,"application/json","{\"ok\":true}");
 }
 if(method=="POST"&&path=="/output/start"){
   std::lock_guard<std::mutex> lock(audioMutex);capturingOutput=false;processedCapture.clear();processedCapture.reserve(48000*30);completedOutput=false;capturingOutput=true;
   return response(200,"application/json","{\"ok\":true}");
 }
 if(method=="POST"&&path=="/output/stop"){
   std::lock_guard<std::mutex> lock(audioMutex);capturingOutput=false;completedOutput=!processedCapture.empty();
   return response(200,"application/json","{\"ok\":true}");
 }
 if(method=="GET"&&path=="/output.wav"){
   std::lock_guard<std::mutex> lock(audioMutex);if(processedCapture.empty())return response(400,"text/plain","No processed audio captured");
   return response(200,"audio/wav",wavBytes(processedCapture));
 }
 if(method=="POST"&&path.rfind("/params?",0)==0){std::string args=path.substr(8);std::lock_guard<std::mutex>g(audioMutex);auto p=settings;p.maxActiveGrains=static_cast<int>(num(args,"maxGrains",p.maxActiveGrains));p.densityMode=static_cast<int>(num(args,"densityMode",p.densityMode));p.density=num(args,"density",p.density);p.grainBloomMs=num(args,"grainBloom",p.grainBloomMs);p.bloomCurve=static_cast<int>(num(args,"bloomCurve",p.bloomCurve));p.bloomVariation=num(args,"bloomVariation",p.bloomVariation);p.grainTiming=num(args,"grainTiming",p.grainTiming);p.attack=num(args,"attack",p.attack);p.release=num(args,"release",p.release);p.shape=static_cast<int>(num(args,"shape",p.shape));p.densityJitter=num(args,"jitter",p.densityJitter);p.pitchRandom=num(args,"pitchrandom",p.pitchRandom);p.durationMs=num(args,"size",p.durationMs);p.keyboardMode=static_cast<int>(num(args,"keyboardMode",p.keyboardMode));p.sliceSize=num(args,"sliceSize",p.sliceSize);
    p.noteReleaseMs=num(args,"noteRelease",p.noteReleaseMs);
    p.fxType=static_cast<int>(num(args,"fxType",p.fxType));
    p.shimmerAmount=num(args,"shimmerAmount",p.shimmerAmount);p.shimmerTone=num(args,"shimmerTone",p.shimmerTone);p.shimmerInterval=static_cast<int>(num(args,"shimmerInterval",p.shimmerInterval));
    p.bpm=num(args,"bpm",p.bpm);
    p.sequencerPlaying=num(args,"seqPlay",p.sequencerPlaying?1:0)>.5;
    p.metronome=num(args,"metro",p.metronome?1:0)>.5;
    p.clickVolume=num(args,"clickVolume",p.clickVolume);
    p.delayDivision=static_cast<int>(num(args,"delayDivision",p.delayDivision));
    p.fxMix=num(args,"fxMix",p.fxMix);
    p.fxTime=num(args,"fxTime",p.fxTime);
    p.fxFeedback=num(args,"fxFeedback",p.fxFeedback);p.pitchSemitones=num(args,"pitch",p.pitchSemitones);p.position=num(args,"position",p.position);p.spray=num(args,"spray",p.spray);p.travel=num(args,"travel",p.travel);p.speed=num(args,"speed",p.speed);p.spread=num(args,"spread",p.spread);
 const int dir=static_cast<int>(num(args,"direction",static_cast<int>(p.direction)));
 if(dir>=0&&dir<=5)p.direction=static_cast<liquid::Direction>(dir);p.gain=num(args,"gain",p.gain);p.regionStart=num(args,"start",p.regionStart);p.regionEnd=num(args,"end",p.regionEnd);p.freeze=num(args,"freeze",p.freeze?1:0)>.5;playing=num(args,"play",playing?1:0)>.5;settings=p;freezeInput=p.freeze;engine.setAmbientPlaying(playing);engine.setParams(settings);return response(200,"application/json","{\"ok\":true}");}
 if(method=="POST"&&path=="/record/start"){
   if(recording)return response(200,"application/json","{\"ok\":true}");
   // 30 sec 48 kHz stereo = 11.5 MB. Preallocate only on UI thread.
   {std::lock_guard<std::mutex> g(audioMutex);engine.beginLive(48000*30);sampleFrames=0;sampleName="LIVE INPUT";peaks.assign(8192,0);lastCapture.clear();inputLevel=0;captured=false;}
   if(!inputReady && !startInput())return response(400,"application/json","{\"error\":\""+jsonEscape(captureError)+"\"}");
   captureError.clear();freezeInput=false;recording=true;
   return response(200,"application/json","{\"ok\":true}");
 }
 if(method=="POST"&&path=="/record/stop"){
   recording=false;
   std::lock_guard<std::mutex> g(audioMutex);
   sampleFrames=engine.capturedFrames();engine.stopLive();lastCapture=engine.snapshot();peaks=downsamplePeaks(lastCapture,2048);captured=true;sampleName="CAPTURED LIVE AUDIO";
   return response(200,"application/json","{\"ok\":true}");
 }
 if(method=="GET"&&path=="/capture.wav"){
   std::lock_guard<std::mutex> g(audioMutex);
   if(lastCapture.empty())return response(400,"text/plain","No finished capture. Press STOP CAPTURE first.");
   return response(200,"audio/wav",wavBytes(lastCapture));
 }
 if(method=="POST"&&path=="/upload"){DecodedWav w;std::string error;std::string format;if(!decodeAudio(body,w,error,format))return response(400,"application/json","{\"error\":\""+jsonEscape(error)+"\"}");if(w.audio.empty())return response(400,"application/json","{\"error\":\"Empty sample\"}");loadSample(std::move(w),"Imported "+format+" sample");return response(200,"application/json","{\"ok\":true}");}
 return response(404,"text/plain","Not found");
}
int main(int argc,char**argv){if(argc>1)root=argv[1];tone();
 {auto raw=readFile("assets/junior_mesa.mp3");if(!raw.empty()){
  DecodedWav decoded;std::string err,fmt;
  if(decodeAudio(std::vector<uint8_t>(raw.begin(),raw.end()),decoded,err,fmt)&&!decoded.audio.empty())loadSample(std::move(decoded),"junior_mesa.mp3");
  else std::cerr<<"Default MP3 decode failed: "<<err<<" (using test tone)\n";
 }}
 settings.density=28;settings.durationMs=110;settings.gain=.5;engine.setParams(settings);if(!startAudio())return 2;
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 3;int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(8765);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(bind(fd,(sockaddr*)&a,sizeof(a))||listen(fd,8)){perror("bind/listen");return 3;}
 std::cout<<"SIMPL_R Liquid v0.21.1: open http://127.0.0.1:8765 in Safari or Chrome\n"<<std::flush;
 for(;;){int c=accept(fd,nullptr,nullptr);if(c<0)continue;std::vector<uint8_t> req;req.reserve(8192);char buf[16384];size_t headerEnd=std::string::npos,contentLength=0;while(req.size()<70*1024*1024){ssize_t n=recv(c,buf,sizeof(buf),0);if(n<=0)break;req.insert(req.end(),buf,buf+n);if(headerEnd==std::string::npos){auto it=std::search(req.begin(),req.end(),"\r\n\r\n","\r\n\r\n"+4);if(it!=req.end()){headerEnd=(it-req.begin())+4;std::string head(req.begin(),req.begin()+headerEnd);auto pos=head.find("Content-Length:");if(pos!=std::string::npos)contentLength=strtoull(head.c_str()+pos+15,nullptr,10);}}if(headerEnd!=std::string::npos&&req.size()>=headerEnd+contentLength)break;}
 std::string res;if(headerEnd==std::string::npos||req.size()<headerEnd+contentLength||contentLength>65*1024*1024)res=response(400,"text/plain","Invalid or oversized request (max 65 MB)");else{std::string first(req.begin(),std::find(req.begin(),req.end(),'\n'));std::istringstream ss(first);std::string method,path;ss>>method>>path;std::vector<uint8_t> payload(req.begin()+headerEnd,req.begin()+headerEnd+contentLength);res=handle(method,path,payload);}size_t sent=0;while(sent<res.size()){ssize_t n=send(c,res.data()+sent,res.size()-sent,0);if(n<=0)break;sent+=n;}close(c);}
}
