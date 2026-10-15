#pragma once
#include "LiquidEngine.h"
#include <vector>
#include <cstdint>
#include <string>
struct DecodedWav { std::vector<liquid::Stereo> audio; unsigned sampleRate=0; };
bool decodeWav(const std::vector<uint8_t>& bytes, DecodedWav& result, std::string& error);
