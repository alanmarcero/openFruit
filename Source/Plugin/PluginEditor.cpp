#include "PluginEditor.h"

OpenFruitAudioProcessorEditor::OpenFruitAudioProcessorEditor(OpenFruitAudioProcessor& ownerProcessor)
    : AudioProcessorEditor(&ownerProcessor),
      processorRef_(ownerProcessor),
      headerPanel_(ownerProcessor.getEngine(), ownerProcessor.getAPVTS()),
      trackerPanel_(ownerProcessor.getEngine(), ownerProcessor.getToneCache(), ownerProcessor.getAnalysisQueue(), &ownerProcessor.getAPVTS()) {

    const auto theme = processorRef_.getEngine().getColorTheme();
    const auto palette = openfruit::theme::getPalette(theme);
    cyberLookAndFeel_.setPalette(palette);
    setLookAndFeel(&cyberLookAndFeel_);

    headerPanel_.onTrackCountChanged = [this] {
        trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());
    };

    headerPanel_.onThemeChanged = [this] {
        const auto newTheme = processorRef_.getEngine().getColorTheme();
        const auto newPalette = openfruit::theme::getPalette(newTheme);
        cyberLookAndFeel_.setPalette(newPalette);
        headerPanel_.setTheme(newPalette);
        trackerPanel_.setTheme(newPalette);
        repaint();
    };

    addAndMakeVisible(headerPanel_);
    addAndMakeVisible(trackerPanel_);

    trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());
    headerPanel_.updateFromParameters();
    trackerPanel_.updateFromEngine();
    trackerPanel_.updatePlayhead();

    setResizable(true, true);
    setResizeLimits(880, 500, 1920, 1200);
    setSize(1120, 720);

    startTimerHz(30); // 30 fps UI animation & playhead updates
}

OpenFruitAudioProcessorEditor::~OpenFruitAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void OpenFruitAudioProcessorEditor::timerCallback() {
    headerPanel_.updateFromParameters();
    trackerPanel_.updateFromEngine();
    trackerPanel_.updatePlayhead();
}

void OpenFruitAudioProcessorEditor::paint(juce::Graphics& g) {
    const auto theme = processorRef_.getEngine().getColorTheme();
    const auto palette = openfruit::theme::getPalette(theme);
    const auto bounds = getLocalBounds().toFloat();

    g.fillAll(palette.background);

    // Outer hardware bezel border
    g.setColour(palette.panelEdge);
    g.drawRect(bounds, 1.0f);
}

void OpenFruitAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    headerPanel_.setBounds(bounds.removeFromTop(68));
    trackerPanel_.setBounds(bounds);
}
