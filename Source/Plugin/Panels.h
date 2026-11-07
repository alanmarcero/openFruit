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
    juce::Label trackCountLabel_{ "Tracks:", "TRACKS:" };
    std::array<juce::TextButton, 4> trackCountBtns_;

    // Automatable Pages: A, B, C, D
    juce::Label pageLabel_{ "Pages:", "PATTERN PAGE:" };
    std::array<juce::TextButton, 4> pageBtns_;

    // Mode Toggle: Sequencer vs MIDI
    juce::TextButton modeBtn_{ "PAD SEQUENCER" };

    // Theme selector
    juce::ComboBox themeBox_;

    // Master Volume
    juce::Slider masterVolSlider_;
    juce::Label masterVolLabel_{ "Master", "MASTER" };
};

class TrackerPanel : public juce::Component {
public:
    TrackerPanel(openfruit::core::FruitEngine& engine,
                 openfruit::core::ToneCache& cache,
                 openfruit::core::AnalysisQueue& queue);
    ~TrackerPanel() override = default;

    void setTheme(const openfruit::theme::Palette& palette);
    void updateFromEngine();
    void updatePlayhead();
    void setVisibleTrackCount(int count);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void openSampleBrowser(int trackIdx);
    void closeSampleBrowser();

    openfruit::core::FruitEngine& engine_;
    openfruit::core::ToneCache& cache_;
    openfruit::core::AnalysisQueue& queue_;
    openfruit::theme::Palette palette_;

    int visibleTrackCount_ = 16;
    std::array<std::unique_ptr<TrackRowComponent>, openfruit::core::kMaxTracks> rows_;

    std::unique_ptr<SampleBrowserModal> activeBrowser_;
};

} // namespace openfruit::ui
