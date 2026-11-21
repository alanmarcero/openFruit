#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "FruitEngine.h"
#include "Theme.h"
#include "Controls.h"
#include "SampleBrowserModal.h"
#include <array>
#include <memory>

namespace openfruit::ui {

class HeaderPanel : public juce::Component {
public:
    HeaderPanel(openfruit::core::FruitEngine& engine,
                juce::AudioProcessorValueTreeState& apvts,
                juce::ValueTree session);
    ~HeaderPanel() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromParameters();

    std::function<void()> onTrackCountChanged;
    std::function<void(juce::Component& target, openfruit::core::PageIndex page)> onPatternMenu;
    std::function<void()> onThemeChanged, onArrangementChanged;
    bool arrangementVisible() const noexcept { return arrangementVisible_; }
    // The page the grid shows, and whether that page is the one playing.
    openfruit::core::PageIndex shownPage() const noexcept { return shownPage_; }
    bool playheadLive() const noexcept { return playheadLive_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    openfruit::core::FruitEngine& engine_;
    juce::AudioProcessorValueTreeState& apvts_;
    juce::ValueTree session_;
    openfruit::theme::Palette palette_;

    // Track Count Toggle: 4, 8, 12, 16
    juce::Label trackCountLabel_{ "Tracks:", "Tracks:" };
    std::array<juce::TextButton, 4> trackCountBtns_;

    // Automatable Pages: A, B, C, D
    juce::Label pageLabel_{ "Pages:", "Page:" };
    std::array<MenuTextButton, 4> pageBtns_;

    // Transport Play / Pause Toggle
    juce::TextButton playBtn_{ "Play" };
    juce::Label transportState_;

    // Theme selector
    juce::ComboBox themeBox_;

    // Master Volume
    juce::Slider masterVolSlider_;
    juce::Label masterVolLabel_{ "Master", "Master volume" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolAttachment_;
    juce::Slider outputDbSlider_;
    juce::Label outputDbLabel_{ "Output", "Output level in decibels" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputDbAttachment_;

    juce::ToggleButton songModeToggle_ { "SONG MODE" };
    juce::Label intervalLabel_ {"Interval", "Every"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> songModeAttachment_;
    juce::Label swingLabel_ { "Swing", "Swing" };
    juce::Slider swingSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment_;
    juce::Label accentLabel_ { "Accent", "ACC" };
    juce::Slider accentAmountSlider_;
    juce::Slider accentEverySlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> accentAmountAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> accentEveryAttachment_;
    juce::Label humanizeLabel_ { "Humanize", "Human" };
    juce::Slider humanizeSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humanizeAttachment_;
    juce::ToggleButton followToggle_ { "Follow" };
    std::array<juce::Label, openfruit::core::kNumPages> sectionLabels_;
    std::array<juce::Rectangle<int>, openfruit::core::kNumPages> sectionBounds_;
    int playingSection_ = -1;
    openfruit::core::PageIndex shownPage_ = openfruit::core::PageIndex::PageA;
    bool playheadLive_ = true;
    std::array<juce::ComboBox, openfruit::core::kNumPages> sectionPageBoxes_;
    std::array<juce::Slider, openfruit::core::kNumPages> sectionBarsSliders_;
    std::array<juce::TextButton, openfruit::core::kNumPages> sectionFillButtons_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, openfruit::core::kNumPages> sectionPageAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, openfruit::core::kNumPages> sectionBarsAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, openfruit::core::kNumPages> sectionFillAttachments_;

    std::unique_ptr<juce::Drawable> fruitLogo_;
    bool arrangementVisible_ = false;
    float lastPhase_ = -1.0f;
};

class TrackerPanel : public juce::Component {
public:
    TrackerPanel(openfruit::core::FruitEngine& engine,
                 openfruit::core::ToneCache& cache,
                 openfruit::core::AnalysisQueue& queue,
                 juce::AudioProcessorValueTreeState* apvts,
                 juce::ValueTree session);
    ~TrackerPanel() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromEngine();
    void updatePlayhead();
    void setVisibleTrackCount(int count);
    void setShownPage(openfruit::core::PageIndex page, bool playheadLive);
    std::function<void(std::vector<openfruit::plugin::StepChange>)> onStepsEdited;
    std::function<void(juce::Component& target, openfruit::core::PageIndex page, int trackIdx)> onRowMenu;

    void paint(juce::Graphics& g) override;
    void resized() override;

    SampleBrowserModal* openSampleBrowser(int trackIdx, const juce::File& folder = {});
    void closeSampleBrowser();
    void openTrackSettings(int track);
    SampleBrowserModal* getActiveBrowser() noexcept { return activeBrowser_.get(); }

private:
    void showGrid(bool visible);
    void setMixView(bool mix);

    openfruit::core::FruitEngine& engine_;
    openfruit::core::ToneCache& cache_;
    openfruit::core::AnalysisQueue& queue_;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    juce::ValueTree session_;
    openfruit::theme::Palette palette_;

    int visibleTrackCount_ = 8;
    bool mixView_ = false;
    juce::TextButton stepsViewBtn_ { "Steps" };
    juce::TextButton mixViewBtn_ { "Mix" };
    uint32_t playheadSteps_ = 0;
    openfruit::core::PageIndex shownPage_ = openfruit::core::PageIndex::PageA;
    bool playheadLive_ = true;
    juce::File lastBrowsedFolder_;
    juce::Component grid_;
    juce::Viewport viewport_;
    std::array<std::unique_ptr<TrackRowComponent>, openfruit::core::kMaxTracks> rows_;

    std::unique_ptr<SampleBrowserModal> activeBrowser_;
    std::unique_ptr<TrackVoiceSettingsPanel> settings_;
};

} // namespace openfruit::ui
