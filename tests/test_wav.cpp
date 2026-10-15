#include "Wav.h"
#include <cassert>
#include <iostream>
int main(){DecodedWav w;std::string err;std::vector<uint8_t> bad(44);assert(!decodeWav(bad,w,err));std::vector<uint8_t> b={
'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x80,0xbb,0,0,0,0x77,1,0,2,0,16,0,'d','a','t','a',4,0,0,0,0,0,0xff,0x7f};
assert(decodeWav(b,w,err));assert(w.audio.size()==2&&w.sampleRate==48000);assert(w.audio[0].l==0&&w.audio[1].l>.99f);std::cout<<"PASS WAV header, mono PCM16, invalid file\n";}
