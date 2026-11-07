#include "PluginEditor.h"

OpenFruitAudioProcessorEditor::OpenFruitAudioProcessorEditor(OpenFruitAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      processorRef_(processor),
      headerPanel_(processor.getEngine(), processor.getAPVTS()),
      trackerPanel_(processor.getEngine(), processor.getToneCache(), processor.getAnalysisQueue()) {

    headerPanel_.onTrackCountChanged = [this] {
        trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());
    };

    headerPanel_.onThemeChanged = [this] {
        const auto theme = processorRef_.getEngine().getColorTheme();
        const auto palette = openfruit::theme::getPalette(theme);
        headerPanel_.setTheme(palette);
        trackerPanel_.setTheme(palette);
        repaint();
    };

    addAndMakeVisible(headerPanel_);
    addAndMakeVisible(trackerPanel_);

    trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());

    setResizable(true, true);
    setResizeLimits(880, 500, 1920, 1200);
    setSize(1120, 720);

    startTimerHz(30); // 30 fps UI animation & playhead updates
}

OpenFruitAudioProcessorEditor::~OpenFruitAudioProcessorEditor() {
    stopTimer();
}

void OpenFruitAudioProcessorEditor::timerCallback() {
    headerPanel_.updateFromParameters();
    trackerPanel_.updateFromEngine();
    trackerPanel_.updatePlayhead();
}

void OpenFruitAudioProcessorEditor::paint(juce::Graphics& g) {
    const auto theme = processorRef_.getEngine().getColorTheme();
    const auto palette = openfruit::theme::getPalette(theme);
    g.fillAll(palette.background);
}

void OpenFruitAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    headerPanel_.setBounds(bounds.removeFromTop(68));
    trackerPanel_.setBounds(bounds);
}
