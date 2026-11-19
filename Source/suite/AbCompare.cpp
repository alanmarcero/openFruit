#include "AbCompare.h"

namespace openfruit::suite {

const juce::MemoryBlock& AbCompare::slot(int index) const {
    return slots_[index == 1 ? 1 : 0];
}

void AbCompare::seed(const juce::MemoryBlock& state) {
    slots_[0] = state;
    slots_[1] = state;
    active_ = 0;
    sendChangeMessage();
}

void AbCompare::rememberActive(const juce::MemoryBlock& state) {
    slots_[active_] = state;
}

void AbCompare::setActive(int index) {
    const int next = index == 1 ? 1 : 0;
    if (next == active_)
        return;
    active_ = next;
    sendChangeMessage();
}

void AbCompare::copy(int from, int to) {
    if (from == to || from < 0 || from > 1 || to < 0 || to > 1)
        return;
    slots_[to] = slots_[from];
    sendChangeMessage();
}

void AbCompare::install(int activeIndex, const juce::MemoryBlock& activeState, const juce::MemoryBlock& otherState) {
    active_ = activeIndex == 1 ? 1 : 0;
    slots_[active_] = activeState;
    slots_[1 - active_] = otherState.getSize() > 0 ? otherState : activeState;
    sendChangeMessage();
}

juce::ValueTree AbCompare::toTree() const {
    juce::ValueTree tree { "AbSlots" };
    tree.setProperty("active", active_, nullptr);
    const auto& other = slots_[1 - active_];
    tree.setProperty("other", other.getSize() == 0
        ? juce::String()
        : juce::Base64::toBase64(other.getData(), other.getSize()), nullptr);
    return tree;
}

}
