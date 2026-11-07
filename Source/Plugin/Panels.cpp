#include "Panels.h"
#include "Parameters.h"

namespace openfruit::ui {

HeaderPanel::HeaderPanel(openfruit::core::FruitEngine& engine,
                         juce::AudioProcessorValueTreeState& apvts)
    : engine_(engine), apvts_(apvts), palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

    // 1. Track Count Toggle: 4, 8, 12, 16
    trackCountLabel_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(trackCountLabel_);

    const std::array<int, 4> counts = { 4, 8, 12, 16 };
    for (size_t i = 0; i < 4; ++i) {
        trackCountBtns_[i].setButtonText(std::to_string(counts[i]));
        trackCountBtns_[i].setClickingTogglesState(false);
        trackCountBtns_[i].onClick = [this, count = counts[i], idx = static_cast<int>(i)] {
            if (auto* param = apvts_.getParameter(openfruit::params::kTrackCount)) {
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(idx)));
            }
            if (count == 4) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks4);
            else if (count == 8) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks8);
            else if (count == 12) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks12);
            else engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks16);

            updateFromParameters();
            if (onTrackCountChanged) onTrackCountChanged();
        };
        addAndMakeVisible(trackCountBtns_[i]);
    }

    // 2. Automatable Pattern Pages: A, B, C, D
    pageLabel_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(pageLabel_);

    const std::array<const char*, 4> pageNames = { "Page A", "Page B", "Page C", "Page D" };
    for (size_t i = 0; i < 4; ++i) {
        pageBtns_[i].setButtonText(pageNames[i]);
        pageBtns_[i].setClickingTogglesState(false);
        pageBtns_[i].onClick = [this, pageIdx = static_cast<openfruit::core::PageIndex>(i)] {
            if (auto* param = apvts_.getParameter(openfruit::params::kPage)) {
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(pageIdx)));
            }
            engine_.setActivePage(pageIdx);
            updateFromParameters();
        };
        addAndMakeVisible(pageBtns_[i]);
    }

    // 3. Mode Toggle: Pad Sequencer vs MIDI Mode
    modeBtn_.onClick = [this] {
        const bool isCurrentlyMidi = (engine_.getInputControlMode() == openfruit::core::InputControlMode::MidiMode);
        const auto newMode = isCurrentlyMidi ? openfruit::core::InputControlMode::SequencerPads
                                             : openfruit::core::InputControlMode::MidiMode;
        if (auto* param = apvts_.getParameter(openfruit::params::kInputMode)) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(newMode)));
        }
        engine_.setInputControlMode(newMode);
        updateFromParameters();
    };
    addAndMakeVisible(modeBtn_);

    // 4. Color Theme Dropdown
    themeBox_.addItem("Cyberpunk Neon", 1);
    themeBox_.addItem("Fruity Classic", 2);
    themeBox_.addItem("Synthwave Outrun", 3);
    themeBox_.addItem("Acid Matrix", 4);
    themeBox_.setSelectedId(1, juce::dontSendNotification);
    themeBox_.onChange = [this] {
        const int id = themeBox_.getSelectedId();
        const auto th = static_cast<openfruit::core::ColorTheme>(id - 1);
        if (auto* param = apvts_.getParameter(openfruit::params::kTheme)) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(th)));
        }
        engine_.setColorTheme(th);
        setTheme(openfruit::theme::getPalette(th));
        if (onThemeChanged) onThemeChanged();
    };
    addAndMakeVisible(themeBox_);

    // 5. Master Volume
    masterVolLabel_.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    addAndMakeVisible(masterVolLabel_);

    masterVolSlider_.setSliderStyle(juce::Slider::LinearBar);
    masterVolSlider_.setRange(0.0, 1.5, 0.01);
    masterVolSlider_.setValue(1.0, juce::dontSendNotification);
    masterVolSlider_.onValueChange = [this] {
        const float v = static_cast<float>(masterVolSlider_.getValue());
        if (auto* param = apvts_.getParameter(openfruit::params::kMasterVol)) {
            param->setValueNotifyingHost(param->convertTo0to1(v));
        }
        engine_.setMasterVolume(v);
    };
    addAndMakeVisible(masterVolSlider_);

    updateFromParameters();
}

void HeaderPanel::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    repaint();
}

void HeaderPanel::updateFromParameters() {
    // 1. Page buttons highlight
    const auto activePage = engine_.getActivePage();
    for (size_t i = 0; i < 4; ++i) {
        const bool active = (static_cast<int>(activePage) == static_cast<int>(i));
        pageBtns_[i].setColour(juce::TextButton::buttonColourId,
                               active ? palette_.accentPrimary : palette_.panel);
    }

    // 2. Track Count buttons highlight
    const int activeTracks = engine_.getActiveTrackCount();
    const std::array<int, 4> counts = { 4, 8, 12, 16 };
    for (size_t i = 0; i < 4; ++i) {
        const bool active = (activeTracks == counts[i]);
        trackCountBtns_[i].setColour(juce::TextButton::buttonColourId,
                                    active ? palette_.accentSecondary : palette_.panel);
    }

    // 3. Mode Button
    const bool isMidi = (engine_.getInputControlMode() == openfruit::core::InputControlMode::MidiMode);
    modeBtn_.setButtonText(isMidi ? "MIDI mode" : "Sequencer mode");
    modeBtn_.setColour(juce::TextButton::buttonColourId,
                       isMidi ? palette_.midiChannelBadge : palette_.accentPrimary);
}

void HeaderPanel::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    // Top faceplate header gradient
    juce::ColourGradient grad(
        palette_.panel.brighter(0.1f), 0.0f, 0.0f,
        palette_.panel.darker(0.3f), 0.0f, bounds.getBottom(),
        false
    );
    g.setGradientFill(grad);
    g.fillRect(bounds);

    // Glowing divider line at bottom
    g.setColour(palette_.panelEdge);
    g.drawLine(0.0f, bounds.getBottom() - 1.0f, bounds.getRight(), bounds.getBottom() - 1.0f, 1.5f);

    // Logo & Emblem
    g.setColour(palette_.accentPrimary);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("openFruit", 16, 6, 120, 36, juce::Justification::centredLeft, false);

    // Subtitle badge
    g.setColour(palette_.accentSecondary);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("16-track step tracker", 16, 40, 130, 14, juce::Justification::centredLeft, false);
}

void HeaderPanel::resized() {
    auto area = getLocalBounds().reduced(8);

    // Skip Logo area (150 px)
    area.removeFromLeft(150);

    // 1. Track Count Toggle (4, 8, 12, 16)
    auto trackArea = area.removeFromLeft(160);
    trackCountLabel_.setBounds(trackArea.removeFromTop(16));
    for (size_t i = 0; i < 4; ++i) {
        trackCountBtns_[i].setBounds(trackArea.getX() + static_cast<int>(i * 38), trackArea.getY(), 34, 26);
    }

    area.removeFromLeft(12);

    // 2. Pattern Page Buttons (PAGE A, B, C, D)
    auto pageArea = area.removeFromLeft(270);
    pageLabel_.setBounds(pageArea.removeFromTop(16));
    for (size_t i = 0; i < 4; ++i) {
        pageBtns_[i].setBounds(pageArea.getX() + static_cast<int>(i * 66), pageArea.getY(), 62, 26);
    }

    area.removeFromLeft(12);

    // 3. Control Mode Toggle
    modeBtn_.setBounds(area.removeFromLeft(140).reduced(0, 10));

    area.removeFromLeft(12);

    // 4. Color Theme Dropdown
    themeBox_.setBounds(area.removeFromLeft(140).reduced(0, 10));

    // 5. Master Volume at far right
    auto masterArea = area.removeFromRight(100);
    masterVolLabel_.setBounds(masterArea.removeFromTop(16));
    masterVolSlider_.setBounds(masterArea.reduced(2));
}

// ============================================================================
// TrackerPanel
// ============================================================================

TrackerPanel::TrackerPanel(openfruit::core::FruitEngine& engine,
                           openfruit::core::ToneCache& cache,
                           openfruit::core::AnalysisQueue& queue)
    : engine_(engine), cache_(cache), queue_(queue), palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        rows_[static_cast<size_t>(i)] = std::make_unique<TrackRowComponent>(i, engine_);
        auto* row = rows_[static_cast<size_t>(i)].get();
        row->onOpenBrowser = [this](int trackIdx) { openSampleBrowser(trackIdx); };
        addAndMakeVisible(row);
    }
}

void TrackerPanel::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    for (auto& row : rows_) {
        if (row) row->setTheme(palette);
    }
    if (activeBrowser_) {
        activeBrowser_->setTheme(palette);
    }
    repaint();
}

void TrackerPanel::setVisibleTrackCount(int count) {
    visibleTrackCount_ = std::clamp(count, 4, 16);
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        rows_[static_cast<size_t>(i)]->setVisible(i < visibleTrackCount_);
    }
    resized();
}

void TrackerPanel::updateFromEngine() {
    for (int i = 0; i < visibleTrackCount_; ++i) {
        rows_[static_cast<size_t>(i)]->updateFromEngine();
    }
}

void TrackerPanel::updatePlayhead() {
    for (int i = 0; i < visibleTrackCount_; ++i) {
        const int step = engine_.getCurrentStep(i);
        rows_[static_cast<size_t>(i)]->updatePlayhead(step);
    }
}

SampleBrowserModal* TrackerPanel::openSampleBrowser(int trackIdx, const juce::File& folder) {
    activeBrowser_ = std::make_unique<SampleBrowserModal>(trackIdx, engine_, cache_, queue_);
    activeBrowser_->setTheme(palette_);
    activeBrowser_->onClose = [this] { closeSampleBrowser(); };
    if (folder.isDirectory()) {
        activeBrowser_->setFolder(folder);
    }
    addAndMakeVisible(activeBrowser_.get());
    activeBrowser_->setBounds(getLocalBounds().reduced(20));
    return activeBrowser_.get();
}

void TrackerPanel::closeSampleBrowser() {
    if (activeBrowser_) {
        removeChildComponent(activeBrowser_.get());
        activeBrowser_.reset();
    }
    updateFromEngine();
}

void TrackerPanel::paint(juce::Graphics& g) {
    g.fillAll(palette_.background);
}

void TrackerPanel::resized() {
    const auto bounds = getLocalBounds();
    const int count = visibleTrackCount_;
    if (count <= 0) return;

    const float rowHeight = static_cast<float>(bounds.getHeight()) / static_cast<float>(count);

    for (int i = 0; i < count; ++i) {
        const int y = static_cast<int>(static_cast<float>(i) * rowHeight);
        const int nextY = static_cast<int>(static_cast<float>(i + 1) * rowHeight);
        rows_[static_cast<size_t>(i)]->setBounds(bounds.getX(), y, bounds.getWidth(), nextY - y);
    }

    if (activeBrowser_) {
        activeBrowser_->setBounds(bounds.reduced(24));
    }
}

} // namespace openfruit::ui
