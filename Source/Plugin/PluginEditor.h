#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Panels.h"
#include "CyberLookAndFeel.h"

class OpenFruitAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer {
public:
    explicit OpenFruitAudioProcessorEditor(OpenFruitAudioProcessor& ownerProcessor);
    ~OpenFruitAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void openSampleBrowser(int trackIdx, const juce::File& folder = {}) {
        trackerPanel_.openSampleBrowser(trackIdx, folder);
    }

private:
    void timerCallback() override;

    OpenFruitAudioProcessor& processorRef_;
    openfruit::ui::CyberLookAndFeel cyberLookAndFeel_;
    openfruit::ui::HeaderPanel headerPanel_;
    openfruit::ui::TrackerPanel trackerPanel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFruitAudioProcessorEditor)
};
