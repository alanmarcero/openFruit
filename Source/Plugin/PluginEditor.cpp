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
      headerPanel_(ownerProcessor.getEngine(), ownerProcessor.getAPVTS(), ownerProcessor.getSessionState()),
      suiteBar_(ownerProcessor.getUndoHistories(), ownerProcessor.getAb()),
      presetBrowser_(ownerProcessor.getPresets()),
      trackerPanel_(ownerProcessor.getEngine(), ownerProcessor.getToneCache(), ownerProcessor.getAnalysisQueue(),
                    &ownerProcessor.getAPVTS(), ownerProcessor.getSessionState()) {

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
    headerPanel_.onPageChanged = [this] {
        trackerPanel_.setShownPage(headerPanel_.shownPage(), headerPanel_.playheadLive());
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
            presetBrowser_.selectPreset(processorRef_.getPresetName());
            presetBrowser_.toFront(false);
        }
        resized();
    };
    presetBrowser_.onDismiss = [this] {
        if (!presetBrowser_.isVisible()) return;
        presetBrowser_.setVisible(false);
        resized();
    };
    presetBrowser_.captureState = [this] { return processorRef_.captureInnerState(); };
    presetBrowser_.restoreState = [this](const juce::MemoryBlock& state, const juce::String& name) {
        processorRef_.loadPresetBlock(state, name);
        headerPanel_.updateFromParameters();
        trackerPanel_.updateFromEngine();
    };
    presetBrowser_.onSaved = [this](const juce::String& name) { processorRef_.presetSaved(name); };
    suiteBar_.onStepPreset = [this](int delta) { presetBrowser_.stepPreset(processorRef_.getPresetName(), delta); };
    trackerPanel_.onStepsEdited = [this](std::vector<openfruit::plugin::StepChange> changes) {
        processorRef_.performEdit(new openfruit::plugin::StepEditAction(processorRef_.getEngine(), std::move(changes)));
    };
    trackerPanel_.onSampleLoaded = [this] {
        processorRef_.markEdited();
        suiteBar_.setPreset(processorRef_.getPresetName(), processorRef_.isPresetEdited());
    };
    trackerPanel_.onRowMenu = [this](juce::Component& target, openfruit::core::PageIndex page, int track) {
        showPatternMenu(target, page, track, 1);
    };
    headerPanel_.onPatternMenu = [this](juce::Component& target, openfruit::core::PageIndex page) {
        showPatternMenu(target, page, 0, openfruit::core::kMaxTracks);
    };

    setWantsKeyboardFocus(true);

    trackerPanel_.setVisibleTrackCount(processorRef_.getEngine().getActiveTrackCount());
    timerCallback();

    // The resize limits trigger resized(), which stores the size, so read the saved size first.
    const auto session = processorRef_.getSessionState();
    const int width = session.getProperty("editorWidth", 1200);
    const int height = session.getProperty("editorHeight", 792);
    setResizable(true, true);
    setResizeLimits(1120, 680, 1920, 1200);
    setSize(width, height);

    startTimerHz(30); // 30 fps UI animation & playhead updates
}

OpenFruitAudioProcessorEditor::~OpenFruitAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void OpenFruitAudioProcessorEditor::timerCallback() {
    headerPanel_.updateFromParameters();
    trackerPanel_.setShownPage(headerPanel_.shownPage(), headerPanel_.playheadLive());
    trackerPanel_.updateFromEngine();
    trackerPanel_.updatePlayhead();
    suiteBar_.setPreset(processorRef_.getPresetName(), processorRef_.isPresetEdited());
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

void OpenFruitAudioProcessorEditor::showPatternMenu(juce::Component& target, openfruit::core::PageIndex page,
                                                    int firstTrack, int count) {
    const bool row = count == 1;
    juce::PopupMenu menu;
    menu.addItem(1, row ? "Copy row" : "Copy pattern");
    menu.addItem(2, row ? "Paste row" : "Paste pattern", static_cast<int>(clipboard_.size()) == count);
    menu.addItem(3, row ? "Clear row" : "Clear pattern");
    menu.addItem(4, "Shift left");
    menu.addItem(5, "Shift right");
    const bool isPageButton = !row;
    if (isPageButton) {
        menu.addSeparator();
        openfruit::ui::addParamLockMenuItems(menu, processorRef_.getAPVTS(), openfruit::params::kPage, 100, 101);
    }
    menu.setLookAndFeel(&cyberLookAndFeel_);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),
        [safeThis = juce::Component::SafePointer<OpenFruitAudioProcessorEditor>(this),
         safeTarget = juce::Component::SafePointer<juce::Component>(&target),
         page, firstTrack, count, isPageButton](int result) {
            if (safeThis == nullptr || result == 0)
                return;
            if (isPageButton && (result == 100 || result == 101)) {
                if (safeTarget != nullptr)
                    openfruit::ui::handleParamLockMenuResult(*safeTarget, safeThis->processorRef_.getAPVTS(),
                                                             openfruit::params::kPage, result, 100, 101);
                return;
            }
            auto& engine = safeThis->processorRef_.getEngine();
            if (result == 1) {
                safeThis->clipboard_ = openfruit::plugin::copyRows(engine, page, firstTrack, count);
                return;
            }
            constexpr openfruit::plugin::RowsCommand commands[] = {
                openfruit::plugin::RowsCommand::Paste, openfruit::plugin::RowsCommand::Clear,
                openfruit::plugin::RowsCommand::ShiftLeft, openfruit::plugin::RowsCommand::ShiftRight };
            auto changes = openfruit::plugin::rowsCommandChanges(engine, page, firstTrack, count,
                                                                 commands[result - 2], safeThis->clipboard_);
            if (!changes.empty())
                safeThis->processorRef_.performEdit(new openfruit::plugin::StepEditAction(engine, std::move(changes)));
        });
}

bool OpenFruitAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && presetBrowser_.isVisible()) {
        presetBrowser_.setVisible(false);
        resized();
        return true;
    }
    return processorRef_.getUndo().keyPressed(key);
}

void OpenFruitAudioProcessorEditor::resized() {
    auto session = processorRef_.getSessionState();
    session.setProperty("editorWidth", getWidth(), nullptr);
    session.setProperty("editorHeight", getHeight(), nullptr);
    auto bounds = getLocalBounds();
    headerPanel_.setBounds(bounds.removeFromTop(headerPanel_.arrangementVisible() ? 206 : 142));
    suiteBar_.setBounds(bounds.removeFromTop(56).reduced(10, 4));
    trackerPanel_.setBounds(bounds);
    if (presetBrowser_.isVisible())
        presetBrowser_.setBounds(getWidth() - 360, suiteBar_.getBottom() + 8, 340, 300);
}
