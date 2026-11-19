#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace openfruit::suite {

// Two full session slots. The active slot is the sound you hear.
class AbCompare : public juce::ChangeBroadcaster {
public:
    [[nodiscard]] int active() const noexcept { return active_; }
    [[nodiscard]] const juce::MemoryBlock& slot(int index) const;

    void seed(const juce::MemoryBlock& state);
    void rememberActive(const juce::MemoryBlock& state);
    void setActive(int index);
    void copy(int from, int to);
    void install(int activeIndex, const juce::MemoryBlock& activeState, const juce::MemoryBlock& otherState);

    [[nodiscard]] juce::ValueTree toTree() const;

private:
    int active_ = 0;
    juce::MemoryBlock slots_[2];
};

}
