#include "PluginEditor.h"
#include "StepEditAction.h"

namespace {

openfruit::suite::SuiteColours suiteColours(const openfruit::theme::Palette& palette) {
    return { palette.background, palette.panel, palette.panelEdge, palette.text, palette.textDim, palette.accentPrimary };
}

} // namespace

juce::AudioProcessorEditor* OpenFruitAudioProcessor::createEditor() {
    return new OpenFruitAudioProcessorEditor(*this);
}

OpenFruitAudioProcessorEditor::OpenFruitAudioProcessorEditor(OpenFruitAudioProcessor& ownerProcessor)
    : AudioProcessorEditor(&ownerProcessor),
      processorRef_(ownerProcessor),
      headerPanel_(ownerProcessor.getEngine(), ownerProcessor.getAPVTS()),
      suiteBar_(ownerProcessor.getUndo(), ownerProcessor.getAb()),
      presetBrowser_(ownerProcessor.getPresets()),
      trackerPanel_(ownerProcessor.getEngine(), ownerProcessor.getToneCache(), ownerProcessor.getAnalysisQueue(), &ownerProcessor.getAPVTS()) {

    const auto theme = processorRef_.getEngine().getColorTheme();
    const auto palette = openfruit::theme::getPalette(theme);
    cyberLookAndFeel_.setPalette(palette);
    setLookAndFeel(&cyberLookAndFeel_);

    headerPanel_.onArrangementChanged = [this] { resized(); };
    headerPanel_.setTheme(palette);
    suiteBar_.setColours(suiteColours(palette));
    presetBrowser_.setColours(suiteColours(palette));
    trackerPanel_.setTheme(palette);
    headerPanel_.onTrackCountChanged = [this] {
        trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());
    };

    headerPanel_.onThemeChanged = [this] {
        const auto newTheme = processorRef_.getEngine().getColorTheme();
        const auto newPalette = openfruit::theme::getPalette(newTheme);
        cyberLookAndFeel_.setPalette(newPalette);
        headerPanel_.setTheme(newPalette);
        suiteBar_.setColours(suiteColours(newPalette));
        presetBrowser_.setColours(suiteColours(newPalette));
        trackerPanel_.setTheme(newPalette);
        repaint();
    };

    addAndMakeVisible(headerPanel_);
    addAndMakeVisible(suiteBar_);
    addAndMakeVisible(presetBrowser_);
    presetBrowser_.setVisible(false);
    addAndMakeVisible(trackerPanel_);

    suiteBar_.onSelectSlot = [this](int slot) { processorRef_.selectAbSlot(slot); };
    suiteBar_.onCopySlot = [this](int from, int to) { processorRef_.copyAbSlot(from, to); };
    suiteBar_.onShowPresets = [this] {
        presetBrowser_.setVisible(!presetBrowser_.isVisible());
        if (presetBrowser_.isVisible()) {
            presetBrowser_.refreshList();
            presetBrowser_.toFront(false);
        }
        resized();
    };
    presetBrowser_.captureState = [this] { return processorRef_.captureInnerState(); };
    presetBrowser_.restoreState = [this](const juce::MemoryBlock& state) {
        processorRef_.loadPresetBlock(state);
        headerPanel_.updateFromParameters();
        trackerPanel_.updateFromEngine();
    };
    trackerPanel_.onStepEdited = [this](int track, openfruit::core::PageIndex page, int step,
                                        openfruit::core::StepData before, openfruit::core::StepData after) {
        processorRef_.getUndo().beginNewTransaction();
        processorRef_.getUndo().perform(new openfruit::plugin::StepEditAction(
            processorRef_.getEngine(), track, static_cast<int>(page), step, before, after));
    };

    setWantsKeyboardFocus(true);

    trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());
    headerPanel_.updateFromParameters();
    trackerPanel_.updateFromEngine();
    trackerPanel_.updatePlayhead();

    setResizable(true, true);
    setResizeLimits(1120, 680, 1920, 1200);
    setSize(1200, 792);

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

bool OpenFruitAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    return processorRef_.getUndo().keyPressed(key);
}

void OpenFruitAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    headerPanel_.setBounds(bounds.removeFromTop(headerPanel_.arrangementVisible() ? 206 : 142));
    suiteBar_.setBounds(bounds.removeFromTop(48).reduced(10, 4));
    trackerPanel_.setBounds(bounds);
    if (presetBrowser_.isVisible())
        presetBrowser_.setBounds(getWidth() - 360, suiteBar_.getBottom() + 8, 340, 300);
}
