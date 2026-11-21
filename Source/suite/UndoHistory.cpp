#include "UndoHistory.h"

namespace openfruit::suite {

ParameterEditAction::ParameterEditAction(juce::RangedAudioParameter& parameter, float oldNormalised, float newNormalised)
    : parameter_(parameter), oldNormalised_(oldNormalised), newNormalised_(newNormalised) {}

bool ParameterEditAction::perform() {
    parameter_.setValueNotifyingHost(newNormalised_);
    return true;
}

bool ParameterEditAction::undo() {
    parameter_.setValueNotifyingHost(oldNormalised_);
    return true;
}

UndoHistory::~UndoHistory() {
    detach();
}

void UndoHistory::attach(juce::AudioProcessor& processor) {
    detach();
    processor_ = &processor;
    for (auto* parameter : processor_->getParameters())
        if (parameter != nullptr)
            parameter->addListener(this);
}

void UndoHistory::detach() {
    if (processor_ == nullptr)
        return;
    for (auto* parameter : processor_->getParameters())
        if (parameter != nullptr)
            parameter->removeListener(this);
    processor_ = nullptr;
    gestureStart_.clear();
    gestureOpen_.clear();
}

bool UndoHistory::keyPressed(const juce::KeyPress& key) {
    if (!key.getModifiers().isCommandDown())
        return false;
    const auto lower = [](juce::juce_wchar character) {
        return juce::CharacterFunctions::toLowerCase(character);
    };
    const bool isZ = lower(static_cast<juce::juce_wchar>(key.getKeyCode())) == 'z'
        || lower(key.getTextCharacter()) == 'z';
    if (!isZ)
        return false;
    if (key.getModifiers().isShiftDown())
        redo();
    else
        undo();
    return true;
}

void UndoHistory::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {
    if (suspended_ || isPerformingUndoRedo() || processor_ == nullptr)
        return;
    auto& parameters = processor_->getParameters();
    if (parameterIndex < 0 || parameterIndex >= parameters.size())
        return;
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters[parameterIndex]);
    if (parameter == nullptr)
        return;

    if (gestureIsStarting) {
        gestureStart_[parameterIndex] = parameter->getValue();
        gestureOpen_[parameterIndex] = true;
        return;
    }
    if (!gestureOpen_[parameterIndex])
        return;
    gestureOpen_[parameterIndex] = false;
    const float now = parameter->getValue();
    const float before = gestureStart_[parameterIndex];
    if (std::abs(now - before) < 1.0e-6f)
        return;
    beginNewTransaction();
    perform(new ParameterEditAction(*parameter, before, now));
    if (onEdit)
        onEdit();
}

}
