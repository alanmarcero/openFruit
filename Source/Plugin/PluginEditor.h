#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Panels.h"
#include "CyberLookAndFeel.h"
#include "suite/PresetBrowser.h"
#include "suite/SuiteBar.h"

class OpenFruitAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer {
public:
    explicit OpenFruitAudioProcessorEditor(OpenFruitAudioProcessor& ownerProcessor);
    ~OpenFruitAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void refresh() { timerCallback(); }
    void openSampleBrowser(int trackIdx, const juce::File& folder = {}) {
        trackerPanel_.openSampleBrowser(trackIdx, folder);
    }

private:
    void timerCallback() override;
    void showPatternMenu(juce::Component& target, openfruit::core::PageIndex page, int firstTrack, int count);

    OpenFruitAudioProcessor& processorRef_;
    openfruit::ui::CyberLookAndFeel cyberLookAndFeel_;
    openfruit::ui::HeaderPanel headerPanel_;
    openfruit::suite::SuiteBar suiteBar_;
    openfruit::suite::PresetBrowser presetBrowser_;
    openfruit::ui::TrackerPanel trackerPanel_;
    juce::TooltipWindow tooltips_ {this, 550};
    std::vector<openfruit::plugin::StepRow> clipboard_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFruitAudioProcessorEditor)
};
