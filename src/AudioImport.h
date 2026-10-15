#pragma once
#include "Wav.h"
#include <cstdint>
#include <string>
#include <vector>

// Dispatches WAV to the existing PCM decoder; MP3 to macOS AudioToolbox.
// Runs on the upload/control thread, never in the real-time audio callback.
bool decodeAudio(const std::vector<uint8_t>& data, DecodedWav& out,
                 std::string& error, std::string& format);
