#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "FruitEngine.h"
#include "SequencerTypes.h"
#include "PatternState.h"
#include <functional>

namespace openfruit::ui {

inline constexpr int kLaneWidth = 382;

// A button whose right-click opens a menu instead of clicking it.
class MenuTextButton : public juce::TextButton {
public:
    std::function<void()> onPopupMenu;

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu() && onPopupMenu) {
            onPopupMenu();
            return;
        }
        juce::TextButton::mouseDown(e);
    }
};

class CyberStepPad : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    CyberStepPad(int trackIdx, int stepIdx);
    ~CyberStepPad() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void setStepData(const openfruit::core::StepData& data);
    void setIsCurrentPlayhead(bool isPlayhead);
    void setSilencedNote(const juce::String& note);

    std::function<void(bool newActive)> onToggle;
    std::function<void(int newTune)> onTuneChange;
    std::function<void(float newVelocity)> onVelocityChange;
    std::function<void(openfruit::core::StepData)> onStepDataChange;
    std::function<void(int rowX, bool active)> onPaint;
    std::function<void()> onEditBegan;
    std::function<void()> onEditEnded;

    juce::String getTooltip() override;
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
    juce::String silencedNote_;
    bool wasActive_ = false;
    bool velocityDragged_ = false;
    bool painting_ = false;
    bool paintActive_ = false;
    int dragStartTune_ = 0;
    bool tuneDragged_ = false;
    bool tuneGesture_ = false;
    juce::Point<int> dragStartPos_;
    float dragStartVelocity_ = 0.85f;

    void timerCallback() override;
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

class TrackRowComponent : public juce::Component, public juce::SettableTooltipClient {
public:
    TrackRowComponent(int trackIdx,
                      openfruit::core::FruitEngine& engine,
                      juce::AudioProcessorValueTreeState* apvts = nullptr);
    ~TrackRowComponent() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromEngine();
    void updatePlayhead(int currentStep);
    void setAnySolo(bool anySolo);

    std::function<void(int trackIdx)> onOpenBrowser, onOpenSettings;
    std::function<void(int trackIdx)> onSampleAudition;
    std::function<void(std::vector<openfruit::plugin::StepChange>)> onStepsEdited;
    std::function<void(juce::Component& target, openfruit::core::PageIndex page, int trackIdx)> onRowMenu;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

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
    bool editOpen_ = false;
    int paintStep_ = 0;
    openfruit::core::PageIndex editPage_ = openfruit::core::PageIndex::PageA;
    openfruit::plugin::StepRow editBefore_ {};
};

} // namespace openfruit::ui
