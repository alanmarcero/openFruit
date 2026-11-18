#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "FruitEngine.h"
#include "SequencerTypes.h"
#include <functional>

namespace openfruit::ui {

inline constexpr int kLaneWidth = 382;

class CyberStepPad : public juce::Component, public juce::SettableTooltipClient {
public:
    CyberStepPad(int trackIdx, int stepIdx);
    ~CyberStepPad() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void setStepData(const openfruit::core::StepData& data);
    void setIsCurrentPlayhead(bool isPlayhead);

    std::function<void(bool newActive)> onToggle;
    std::function<void(int newTune)> onTuneChange;
    std::function<void(float newVelocity)> onVelocityChange;
    std::function<void(openfruit::core::StepData)> onStepDataChange;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    [[maybe_unused]] int trackIdx_;
    const int stepIdx_;
    openfruit::theme::Palette palette_;
    openfruit::core::StepData data_{};
    bool isPlayhead_ = false;
    bool wasActive_ = false;
    bool velocityDragged_ = false;
    int dragStartTune_ = 0;
    bool tuneDragged_ = false;
    bool tuneGesture_ = false;
    juce::Point<int> dragStartPos_;
    float dragStartVelocity_ = 0.85f;

    void showStepMenu();
    void selectStepMenuItem(int itemId);
};

class TrackVoiceSettingsPanel : public juce::Component {
public:
    TrackVoiceSettingsPanel(int trackIdx,
                            juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    int track_;
    juce::TextButton close_ {"Done"};
    juce::ComboBox midi_, timing_;
    std::array<juce::Label, 2> routingLabels_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timingAttachment_;
    std::unique_ptr<juce::ParameterAttachment> midiAttachment_;
    std::array<juce::Slider, 12> sliders_;
    std::array<juce::Label, 12> labels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 12> attachments_;
public:
    std::function<void()> onClose;
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

    std::function<void(int trackIdx)> onOpenBrowser, onOpenSettings;
    std::function<void(int trackIdx)> onSampleAudition;

    void paint(juce::Graphics& g) override;
    void resized() override;

    [[nodiscard]] int getTrackIndex() const noexcept { return trackIdx_; }

private:
    const int trackIdx_;
    openfruit::core::FruitEngine& engine_;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    openfruit::theme::Palette palette_;

    juce::TextButton samplePadBtn_, loadBtn_;
    juce::TextButton voiceSettingsBtn_ {"Settings"};
    juce::ToggleButton muteBtn_{ "Mute" };
    juce::ToggleButton soloBtn_{ "Solo" };

    std::array<std::unique_ptr<CyberStepPad>, openfruit::core::kStepsPerPage> stepPads_;
};

} // namespace openfruit::ui
