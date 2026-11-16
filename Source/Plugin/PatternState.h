#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "FruitEngine.h"

namespace openfruit::plugin {

juce::ValueTree makePatternState(const core::FruitEngine& engine);
void restorePatternState(const juce::ValueTree& state, core::FruitEngine& engine);
void restorePatternStateIfPresent(const juce::ValueTree& state, core::FruitEngine& engine);

}
