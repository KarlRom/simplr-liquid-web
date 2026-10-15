#include "AudioImport.h"
#include <cassert>
#include <string>
#include <vector>
int main(){
  DecodedWav wav; std::string error,format;
  std::vector<uint8_t> invalid={'n','o','p','e'};
  assert(!decodeAudio(invalid,wav,error,format));
  assert(error.find("WAV or MP3")!=std::string::npos);
  std::vector<uint8_t> mp3={'I','D','3',4,0,0,0,0,0,0};
  assert(!decodeAudio(mp3,wav,error,format));
  assert(format=="MP3" && !error.empty());
}
