#pragma once

#include "FruitEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace openfruit::plugin {

void processMidiBlock(core::FruitEngine& engine,
                      const juce::MidiBuffer& midiMessages,
                      float* left,
                      float* right,
                      int numSamples,
                      double bpm,
                      double ppqPosition,
                      bool isHostPlaying,
                      double sampleRate);

} // namespace openfruit::plugin
