#include "Wav.h"
#include <cmath>
#include <cstring>
#include <algorithm>
namespace { uint16_t u16(const uint8_t*p){return uint16_t(p[0]|(p[1]<<8));} uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);} }
bool decodeWav(const std::vector<uint8_t>& b, DecodedWav& out,std::string& err){
  if(b.size()<44||memcmp(b.data(),"RIFF",4)||memcmp(b.data()+8,"WAVE",4)){err="Not a RIFF/WAVE file";return false;}
  uint16_t format=0,channels=0,bits=0,align=0;uint32_t sr=0;size_t dataOff=0,dataLen=0;
  for(size_t pos=12;pos+8<=b.size();){uint32_t n=u32(b.data()+pos+4);size_t start=pos+8;if(n>b.size()-start){err="Truncated WAV chunk";return false;}
    if(!memcmp(b.data()+pos,"fmt ",4)&&n>=16){format=u16(b.data()+start);channels=u16(b.data()+start+2);sr=u32(b.data()+start+4);align=u16(b.data()+start+12);bits=u16(b.data()+start+14);}
    if(!memcmp(b.data()+pos,"data",4)){dataOff=start;dataLen=n;}
    pos=start+n+(n&1);}
  if((format!=1&&format!=3)||(channels!=1&&channels!=2)||sr<8000||sr>192000||!((format==1&&(bits==16||bits==24||bits==32))||(format==3&&bits==32))||align!=channels*(bits/8)||!dataLen){err="Supported: mono/stereo PCM 16/24/32-bit or float32 WAV";return false;}
  size_t frames=dataLen/align;if(frames>19200000){err="File too long (100 seconds at 192 kHz max)";return false;}
  out.audio.resize(frames);out.sampleRate=sr;
  auto sample=[&](const uint8_t*p){if(format==3){float f;memcpy(&f,p,4);return std::isfinite(f)?std::clamp(f,-1.f,1.f):0.f;} if(bits==16)return static_cast<float>(static_cast<int16_t>(u16(p))/32768.f);if(bits==24){int32_t v=int32_t(p[0]|(p[1]<<8)|(p[2]<<16));if(v&0x800000)v|=~0xffffff;return v/8388608.f;}return static_cast<float>(static_cast<int32_t>(u32(p))/2147483648.0);};
  for(size_t i=0;i<frames;++i){auto*p=b.data()+dataOff+i*align;float l=sample(p);float r=channels==2?sample(p+bits/8):l;out.audio[i]={l,r};}
  return true;
}
