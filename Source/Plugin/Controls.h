#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "FruitEngine.h"
#include "SequencerTypes.h"
#include <functional>

namespace openfruit::ui {

class CyberStepPad : public juce::Component {
public:
    CyberStepPad(int trackIdx, int stepIdx);
    ~CyberStepPad() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void setStepData(const openfruit::core::StepData& data);
    void setIsCurrentPlayhead(bool isPlayhead);

    std::function<void(bool newActive)> onToggle;
    std::function<void(int newTune)> onTuneChange;
    std::function<void(float newVelocity)> onVelocityChange;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    [[maybe_unused]] int trackIdx_;
    const int stepIdx_;
    openfruit::theme::Palette palette_;
    openfruit::core::StepData data_{};
    bool isPlayhead_ = false;
    juce::Point<int> dragStartPos_;
};

class TrackRowComponent : public juce::Component {
public:
    TrackRowComponent(int trackIdx,
                      openfruit::core::FruitEngine& engine,
                      juce::AudioProcessorValueTreeState* apvts = nullptr);
    ~TrackRowComponent() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromEngine();
    void updatePlayhead(int currentStep);

    std::function<void(int trackIdx)> onOpenBrowser;
    std::function<void(int trackIdx)> onSampleAudition;

    void paint(juce::Graphics& g) override;
    void resized() override;

    [[nodiscard]] int getTrackIndex() const noexcept { return trackIdx_; }

private:
    const int trackIdx_;
    openfruit::core::FruitEngine& engine_;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    openfruit::theme::Palette palette_;

    juce::ComboBox midiChannelBox_;
    juce::TextButton samplePadBtn_;
    juce::TextButton loadBtn_;
    juce::ComboBox timingBox_;
    juce::Slider tuneSlider_;
    juce::Slider volSlider_;
    juce::Slider panSlider_;
    juce::ToggleButton muteBtn_{ "M" };
    juce::ToggleButton soloBtn_{ "S" };

    std::array<std::unique_ptr<CyberStepPad>, openfruit::core::kStepsPerPage> stepPads_;
};

} // namespace openfruit::ui
