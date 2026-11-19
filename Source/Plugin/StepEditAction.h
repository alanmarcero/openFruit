#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "FruitEngine.h"

namespace openfruit::plugin {

class StepEditAction : public juce::UndoableAction {
public:
    StepEditAction(core::FruitEngine& engine, int track, int page, int step,
                   core::StepData before, core::StepData after)
        : engine_(engine), track_(track), page_(page), step_(step), before_(before), after_(after) {}

    bool perform() override {
        engine_.setStepData(track_, static_cast<core::PageIndex>(page_), step_, after_);
        return true;
    }

    bool undo() override {
        engine_.setStepData(track_, static_cast<core::PageIndex>(page_), step_, before_);
        return true;
    }

private:
    core::FruitEngine& engine_;
    int track_ = 0;
    int page_ = 0;
    int step_ = 0;
    core::StepData before_;
    core::StepData after_;
};

}
