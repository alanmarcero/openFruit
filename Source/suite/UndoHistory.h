#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>

namespace openfruit::suite {

class ParameterEditAction : public juce::UndoableAction {
public:
    ParameterEditAction(juce::RangedAudioParameter& parameter, float oldNormalised, float newNormalised);
    bool perform() override;
    bool undo() override;
    int getSizeInUnits() override { return 1; }

private:
    juce::RangedAudioParameter& parameter_;
    float oldNormalised_;
    float newNormalised_;
};

// Groups parameter drags into one undo step. Host automation does not open a
// gesture, so it stays out of the history.
class UndoHistory : public juce::UndoManager, private juce::AudioProcessorParameter::Listener {
public:
    UndoHistory() = default;
    ~UndoHistory() override;

    void attach(juce::AudioProcessor& processor);
    void detach();
    void suspend(bool shouldSuspend) noexcept { suspended_ = shouldSuspend; }
    [[nodiscard]] bool keyPressed(const juce::KeyPress& key);

private:
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    void parameterValueChanged(int, float) override {}

    juce::AudioProcessor* processor_ = nullptr;
    bool suspended_ = false;
    std::unordered_map<int, float> gestureStart_;
    std::unordered_map<int, bool> gestureOpen_;
};

}
