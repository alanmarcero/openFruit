#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "FruitEngine.h"
#include <vector>

namespace openfruit::plugin {

struct StepChange {
    int track = 0;
    core::PageIndex page = core::PageIndex::PageA;
    int step = 0;
    core::StepData before;
    core::StepData after;
};

class StepEditAction : public juce::UndoableAction {
public:
    StepEditAction(core::FruitEngine& engine, std::vector<StepChange> changes)
        : engine_(engine), changes_(std::move(changes)) {}

    bool perform() override {
        engine_.beginPatternBatch();
        for (const auto& change : changes_)
            engine_.setStepData(change.track, change.page, change.step, change.after);
        engine_.endPatternBatch();
        return true;
    }

    bool undo() override {
        engine_.beginPatternBatch();
        for (const auto& change : changes_)
            engine_.setStepData(change.track, change.page, change.step, change.before);
        engine_.endPatternBatch();
        return true;
    }

private:
    core::FruitEngine& engine_;
    std::vector<StepChange> changes_;
};

}
