#include "AudioImport.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#endif

namespace {
bool isWav(const std::vector<uint8_t>& b) {
  return b.size() >= 12 && !std::memcmp(b.data(),"RIFF",4) && !std::memcmp(b.data()+8,"WAVE",4);
}
bool isMp3(const std::vector<uint8_t>& b) {
  if(b.size()<4) return false;
  if(!std::memcmp(b.data(),"ID3",3)) return true;
  // MPEG Layer III frame header, excluding reserved version/rate and invalid bitrate.
  for(size_t i=0;i+4<=b.size()&&i<4096;++i){
    if(b[i]!=0xFF || (b[i+1]&0xE0)!=0xE0)continue;
    int version=(b[i+1]>>3)&3,layer=(b[i+1]>>1)&3;
    int rate=(b[i+2]>>2)&3, bitrate=(b[i+2]>>4)&15;
    if(version!=1 && layer==1 && rate!=3 && bitrate!=0 && bitrate!=15)return true;
  }
  return false;
}
#ifdef __APPLE__
bool decodeMp3Mac(const std::vector<uint8_t>& bytes,DecodedWav& out,std::string& error){
  // ExtAudioFile accepts URLs, so write the upload to a securely created private
  // temporary file and delete it immediately after opening the decoder.
  char path[]="/tmp/simplr_mp3_XXXXXX.mp3";
  int fd=mkstemps(path,4);
  if(fd<0){error="Unable to create temporary MP3 file";return false;}
  size_t written=0;
  while(written<bytes.size()){
    ssize_t n=write(fd,bytes.data()+written,bytes.size()-written);
    if(n<=0){close(fd);unlink(path);error="Unable to write temporary MP3 file";return false;}
    written+=static_cast<size_t>(n);
  }
  close(fd);
  CFURLRef url=CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(path),std::strlen(path),false);
  ExtAudioFileRef file=nullptr;
  OSStatus status=url?ExtAudioFileOpenURL(url,&file):-1;
  if(url)CFRelease(url);
  // Keep the file available until CoreAudio has finished decoding; some codecs
  // lazily open additional stream resources while reading.
  if(status!=noErr||!file){unlink(path);error="macOS could not open this audio stream (unsupported, protected, or damaged file; OSStatus "+std::to_string(status)+")";return false;}
  AudioStreamBasicDescription source{};
  UInt32 size=sizeof(source);
  status=ExtAudioFileGetProperty(file,kExtAudioFileProperty_FileDataFormat,&size,&source);
  if(status!=noErr||source.mSampleRate<8000||source.mSampleRate>192000||source.mChannelsPerFrame<1||source.mChannelsPerFrame>2){
    ExtAudioFileDispose(file);unlink(path);error="MP3 must have mono or stereo audio at 8–192 kHz";return false;
  }
  const UInt32 channels=source.mChannelsPerFrame;
  AudioStreamBasicDescription client{};
  client.mSampleRate=source.mSampleRate;
  client.mFormatID=kAudioFormatLinearPCM;
  client.mFormatFlags=kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagsNativeEndian;
  client.mBitsPerChannel=32;
  client.mChannelsPerFrame=channels;
  client.mFramesPerPacket=1;
  client.mBytesPerFrame=channels*sizeof(float);
  client.mBytesPerPacket=client.mBytesPerFrame;
  status=ExtAudioFileSetProperty(file,kExtAudioFileProperty_ClientDataFormat,sizeof(client),&client);
  if(status!=noErr){ExtAudioFileDispose(file);unlink(path);error="Unable to configure macOS MP3 decoder";return false;}
  out.audio.clear();out.sampleRate=static_cast<unsigned>(source.mSampleRate+0.5);
  constexpr UInt32 chunk=4096;
  std::vector<float> samples(chunk*channels);
  const size_t maxFrames=static_cast<size_t>(source.mSampleRate*1800); // 30 minute safety cap
  for(;;){
    AudioBufferList buffers{};
    buffers.mNumberBuffers=1;
    buffers.mBuffers[0].mNumberChannels=channels;
    buffers.mBuffers[0].mDataByteSize=static_cast<UInt32>(samples.size()*sizeof(float));
    buffers.mBuffers[0].mData=samples.data();
    UInt32 frames=chunk;
    status=ExtAudioFileRead(file,&frames,&buffers);
    if(status!=noErr){ExtAudioFileDispose(file);unlink(path);out.audio.clear();error="MP3 decoding failed (CoreAudio error "+std::to_string(status)+")";return false;}
    if(frames==0)break;
    if(out.audio.size()+frames>maxFrames){ExtAudioFileDispose(file);unlink(path);out.audio.clear();error="MP3 exceeds the 30-minute safety limit";return false;}
    for(UInt32 i=0;i<frames;++i){
      float l=samples[i*channels],r=channels==2?samples[i*channels+1]:l;
      out.audio.push_back({std::isfinite(l)?std::clamp(l,-1.f,1.f):0.f,
                           std::isfinite(r)?std::clamp(r,-1.f,1.f):0.f});
    }
  }
  ExtAudioFileDispose(file);
  unlink(path);
  if(out.audio.empty()){error="MP3 contains no decodable audio";return false;}
  return true;
}
#endif
}

bool decodeAudio(const std::vector<uint8_t>& bytes,DecodedWav& out,
                 std::string& error,std::string& format){
  if(isWav(bytes)){
    format="WAV";
    return decodeWav(bytes,out,error);
  }
  if(isMp3(bytes)){
    format="MP3";
#ifdef __APPLE__
    return decodeMp3Mac(bytes,out,error);
#else
    error="MP3 decoding requires the native macOS AudioToolbox backend in this build";
    return false;
#endif
  }
#ifdef __APPLE__
  // Some legitimate MP3s have unusual ID3 headers or metadata before the
  // first MPEG frame. Let macOS determine the stream type as a fallback.
  format="CoreAudio";
  if(decodeMp3Mac(bytes,out,error))return true;
  error="macOS could not decode this file. It may be protected, truncated, or not actually MP3/WAV. "+error;
#else
  error="Unsupported audio file. Please select a WAV or MP3.";
#endif
  return false;
}
