#include "LiquidDisplay.h"
#include <cstdint>
#include <cstddef>
using liquid_display::Renderer;
using liquid_display::Frame;
using liquid_display::Grain;
using liquid_display::Profile;
static uint8_t sharpPixels[12000];
static uint16_t sharpBody[52400];
static uint8_t sharpFx[52400];
static uint8_t ipsPixels[614400];
static uint16_t ipsBody[53120];
static uint8_t ipsFx[53120];
alignas(Renderer) static unsigned char sharpStorage[sizeof(Renderer)];
alignas(Renderer) static unsigned char ipsStorage[sizeof(Renderer)];
static Renderer* sharp=nullptr;
static Renderer* ips=nullptr;
static float packet[24+256+64*8];
static Grain grains[64];
inline void* operator new(size_t,void*p) noexcept {return p;}
extern "C" {
__attribute__((visibility("default"))) int display_init(){
  if(!sharp){sharp=new(sharpStorage) Renderer(Profile::Sharp,sharpPixels,sizeof(sharpPixels),sharpBody,52400,sharpFx,52400);
             ips=new(ipsStorage) Renderer(Profile::Waveshare,ipsPixels,sizeof(ipsPixels),ipsBody,53120,ipsFx,53120);}
  return sharp->valid()&&ips->valid();
}
__attribute__((visibility("default"))) uintptr_t display_input_ptr(){return (uintptr_t)packet;}
__attribute__((visibility("default"))) uintptr_t display_output_ptr(int profile){return (uintptr_t)(profile?ipsPixels:sharpPixels);}
__attribute__((visibility("default"))) int display_output_bytes(int profile){return profile?sizeof(ipsPixels):sizeof(sharpPixels);}
__attribute__((visibility("default"))) int display_render(int profile){
  if(!sharp||!ips)return 0;
  int count=(int)packet[0];if(count<0||count>64)return 0;
  Frame f;
  f.now=packet[1];f.start=packet[2];f.end=packet[3];f.scale=packet[4];
  f.vertical=packet[5];f.cohesion=packet[6];f.tension=packet[7];f.viscosity=packet[8];
  f.effect=(int)packet[9];f.wet=packet[10];f.time=packet[11];f.feedback=packet[12];
  f.playing=packet[13]>0;f.frozen=packet[15]>0;f.peaks=packet+24;f.peakCount=256;
  for(int j=0;j<8;j++)f.macros[j]=packet[16+j];
  for(int j=0;j<count;j++){
    const float* p=packet+280+j*8;
    grains[j]={p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]};
  }
  f.grains=grains;f.count=count;
  Renderer* renderer=profile?ips:sharp;
  if(packet[14]>0)renderer->reset();
  return renderer->render(f)?1:0;
}
}
