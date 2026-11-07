#pragma once

#include "SampleVoice.h"
#include <vector>
#include <memory>
#include <string>

namespace openfruit::kit {

// Generates 16 default electronic & drum samples (Kick, Snare, Claps, Hats, 808s, Stabs)
std::vector<std::shared_ptr<openfruit::core::AudioSampleBuffer>> createDefaultKit(double sampleRate = 44100.0);

} // namespace openfruit::kit
