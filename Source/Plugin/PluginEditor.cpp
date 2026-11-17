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

    headerPanel_.onArrangementChanged = [this] { resized(); };
    headerPanel_.setTheme(palette);
    trackerPanel_.setTheme(palette);
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
    setResizeLimits(1120, 680, 1920, 1200);
    setSize(1200, 744);

    startTimerHz(30); // 30 fps UI animation & playhead updates
}

OpenFruitAudioProcessorEditor::~OpenFruitAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void OpenFruitAudioProcessorEditor::timerCallback() {
    const int page = static_cast<int>(processorRef_.getEngine().getActivePage());
    if (page != displayedPage_) {
        displayedPage_ = page;
        trackerPanel_.repaint();
    }
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
    headerPanel_.setBounds(bounds.removeFromTop(headerPanel_.arrangementVisible() ? 206 : 142));
    trackerPanel_.setBounds(bounds);
}
