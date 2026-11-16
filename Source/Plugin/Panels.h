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
                juce::AudioProcessorValueTreeState& apvts);
    ~HeaderPanel() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromParameters();

    std::function<void()> onTrackCountChanged;
    std::function<void()> onThemeChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    openfruit::core::FruitEngine& engine_;
    juce::AudioProcessorValueTreeState& apvts_;
    openfruit::theme::Palette palette_;

    // Track Count Toggle: 4, 8, 12, 16
    juce::Label trackCountLabel_{ "Tracks:", "Tracks:" };
    std::array<juce::TextButton, 4> trackCountBtns_;

    // Automatable Pages: A, B, C, D
    juce::Label pageLabel_{ "Pages:", "Page:" };
    std::array<juce::TextButton, 4> pageBtns_;

    // Transport Play / Pause Toggle
    juce::TextButton playBtn_{ "Play" };

    // Theme selector
    juce::ComboBox themeBox_;

    // Master Volume
    juce::Slider masterVolSlider_;
    juce::Label masterVolLabel_{ "Master", "Master volume" };
    juce::Slider outputDbSlider_;
    juce::Label outputDbLabel_{ "Output", "Output level in decibels" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputDbAttachment_;

    juce::ToggleButton songModeToggle_ { "SONG MODE" };
    juce::Label songModeLabel_ { "SongMode", "SONG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> songModeAttachment_;
    juce::Label swingLabel_ { "Swing", "Swing" };
    juce::Slider swingSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment_;
    std::array<juce::Label, openfruit::core::kNumPages> sectionLabels_;
    std::array<juce::ComboBox, openfruit::core::kNumPages> sectionPageBoxes_;
    std::array<juce::Slider, openfruit::core::kNumPages> sectionBarsSliders_;
    std::array<juce::TextButton, openfruit::core::kNumPages> sectionFillButtons_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, openfruit::core::kNumPages> sectionPageAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, openfruit::core::kNumPages> sectionBarsAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, openfruit::core::kNumPages> sectionFillAttachments_;

    juce::Image fruitLogo_;
    juce::Image faceplateTexture_;
};

class TrackerPanel : public juce::Component {
public:
    TrackerPanel(openfruit::core::FruitEngine& engine,
                 openfruit::core::ToneCache& cache,
                 openfruit::core::AnalysisQueue& queue,
                 juce::AudioProcessorValueTreeState* apvts = nullptr);
    ~TrackerPanel() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromEngine();
    void updatePlayhead();
    void setVisibleTrackCount(int count);

    void paint(juce::Graphics& g) override;
    void resized() override;

    SampleBrowserModal* openSampleBrowser(int trackIdx, const juce::File& folder = {});
    void closeSampleBrowser();
    SampleBrowserModal* getActiveBrowser() noexcept { return activeBrowser_.get(); }

private:

    openfruit::core::FruitEngine& engine_;
    openfruit::core::ToneCache& cache_;
    openfruit::core::AnalysisQueue& queue_;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    openfruit::theme::Palette palette_;

    int visibleTrackCount_ = 8;
    juce::File lastBrowsedFolder_;
    std::array<std::unique_ptr<TrackRowComponent>, openfruit::core::kMaxTracks> rows_;

    std::unique_ptr<SampleBrowserModal> activeBrowser_;
    juce::Image faceplateTexture_;
};

} // namespace openfruit::ui
