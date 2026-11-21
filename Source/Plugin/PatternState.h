#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "FruitEngine.h"
#include "StepEditAction.h"

namespace openfruit::plugin {

using StepRow = std::array<core::StepData, core::kStepsPerPage>;
enum class RowsCommand { Paste, Clear, ShiftLeft, ShiftRight };

std::vector<StepRow> copyRows(const core::FruitEngine& engine, core::PageIndex page, int firstTrack, int count);
// The changes that apply a command to `count` tracks from `firstTrack` on one page. Shifts wrap around.
std::vector<StepChange> rowsCommandChanges(const core::FruitEngine& engine, core::PageIndex page, int firstTrack,
                                           int count, RowsCommand command, const std::vector<StepRow>& clipboard);

juce::ValueTree makePatternState(const core::FruitEngine& engine);
void restorePatternState(const juce::ValueTree& state, core::FruitEngine& engine);
void restorePatternStateIfPresent(const juce::ValueTree& state, core::FruitEngine& engine);

}
