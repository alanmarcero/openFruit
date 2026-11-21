#include "Panels.h"
#include "Parameters.h"
#include <BinaryData.h>

namespace openfruit::ui {

HeaderPanel::HeaderPanel(openfruit::core::FruitEngine& engine,
                         juce::AudioProcessorValueTreeState& apvts)
    : engine_(engine),
      apvts_(apvts),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)),
      fruitLogo_(juce::Drawable::createFromImageData(BinaryData::citrusclock_svg, BinaryData::citrusclock_svgSize)) {

    trackCountLabel_.setText("Tracks", juce::dontSendNotification);
    pageLabel_.setText("Pattern", juce::dontSendNotification);
    masterVolLabel_.setText("Master", juce::dontSendNotification);
    masterVolSlider_.setTooltip("Master mix level");
    playBtn_.setTooltip("Start or pause the sequencer; play arms host transport sync");
    transportState_.setTooltip("Stopped, Armed or Playing. Armed starts the sequencer when the host starts.");
    transportState_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    transportState_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(transportState_);
    // Track Count Toggle: 4, 8, 12, 16
    trackCountLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(trackCountLabel_);

    const std::array<int, 4> counts = { 4, 8, 12, 16 };
    for (size_t i = 0; i < 4; ++i) {
        trackCountBtns_[i].setTooltip("Show " + juce::String(counts[i]) + " tracks");
        trackCountBtns_[i].setButtonText(std::to_string(counts[i]));
        trackCountBtns_[i].setClickingTogglesState(false);
        trackCountBtns_[i].onClick = [this, count = counts[i], idx = static_cast<int>(i)] {
            if (auto* param = apvts_.getParameter(openfruit::params::kTrackCount)) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(idx)));
                param->endChangeGesture();
            }
            if (count == 4) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks4);
            if (count == 8) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks8);
            if (count == 12) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks12);
            if (count == 16) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks16);

            updateFromParameters();
            if (onTrackCountChanged) onTrackCountChanged();
        };
        addAndMakeVisible(trackCountBtns_[i]);
    }

    // 2. Automatable Pattern Pages: A, B, C, D
    pageLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(pageLabel_);

    const std::array<const char*, 4> pageNames = { "A", "B", "C", "D" };
    for (size_t i = 0; i < 4; ++i) {
        pageBtns_[i].setTooltip("Edit and play pattern " + juce::String(pageNames[i]));
        pageBtns_[i].setButtonText(pageNames[i]);
        pageBtns_[i].setClickingTogglesState(false);
        pageBtns_[i].onClick = [this, pageIdx = static_cast<openfruit::core::PageIndex>(i)] {
            if (auto* param = apvts_.getParameter(openfruit::params::kPage)) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(pageIdx)));
                param->endChangeGesture();
            }
            engine_.setActivePage(pageIdx);
            updateFromParameters();
        };
        addAndMakeVisible(pageBtns_[i]);
    }

    // 3. Transport Play / Pause Button
    playBtn_.onClick = [this] {
        engine_.setPlaying(!engine_.isPlaying());
        updateFromParameters();
    };
    addAndMakeVisible(playBtn_);

    // 4. Color Theme Dropdown
    themeBox_.addItem("Cyberpunk Neon", 1);
    themeBox_.addItem("Fruity Classic", 2);
    themeBox_.addItem("Synthwave Outrun", 3);
    themeBox_.addItem("Acid Matrix", 4);
    themeBox_.setSelectedId(1, juce::dontSendNotification);
    themeBox_.setTooltip("Choose a color theme for the editor.");
    themeBox_.onChange = [this] {
        const int id = themeBox_.getSelectedId();
        const auto th = static_cast<openfruit::core::ColorTheme>(id - 1);
        if (auto* param = apvts_.getParameter(openfruit::params::kTheme)) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(th)));
            param->endChangeGesture();
        }
        engine_.setColorTheme(th);
        setTheme(openfruit::theme::getPalette(th));
        if (onThemeChanged) onThemeChanged();
    };
    addAndMakeVisible(themeBox_);

    // 5. Master Volume
    masterVolLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(masterVolLabel_);

    masterVolSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    masterVolSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 26);
    masterVolSlider_.setRange(0.0, 1.5, 0.01);
    masterVolSlider_.setValue(1.0, juce::dontSendNotification);
    masterVolAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kMasterVol, masterVolSlider_);
    addAndMakeVisible(masterVolSlider_);

    outputDbLabel_.setText("Output", juce::dontSendNotification);
    outputDbLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    outputDbLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(outputDbLabel_);
    outputDbSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    outputDbSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 26);
    outputDbSlider_.setRange(-24.0, 12.0, 0.1);
    outputDbSlider_.setDoubleClickReturnValue(true, 0.0);
    outputDbSlider_.setTooltip("Output gain in decibels");
    outputDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kOutputDb, outputDbSlider_);
    addAndMakeVisible(outputDbSlider_);

    songModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, openfruit::params::kSongMode, songModeToggle_);
    songModeToggle_.setButtonText("Song mode");
    songModeToggle_.setTooltip("Chain the four sections in Song Mode");
    addAndMakeVisible(songModeToggle_);
    addAndMakeVisible(intervalLabel_);

    swingLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(swingLabel_);
    swingSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    swingSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    swingSlider_.setRange(0.0, 0.5, 0.001);
    swingSlider_.setNumDecimalPlacesToDisplay(2);
    swingSlider_.setTooltip("Delay alternating steps by up to half a step");
    swingAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kSwing, swingSlider_);
    addAndMakeVisible(swingSlider_);

    accentLabel_.setText("Accent", juce::dontSendNotification);
    accentLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    accentLabel_.setTooltip("Accent the first step, then repeat at the selected interval.");
    addAndMakeVisible(accentLabel_);
    accentAmountSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    accentAmountSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    accentAmountSlider_.setRange(0.0, 1.0, 0.01);
    accentAmountSlider_.setTooltip("Periodic accent amount. Stored step velocities remain unchanged.");
    accentAmountAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kAccentAmount, accentAmountSlider_);
    addAndMakeVisible(accentAmountSlider_);
    accentEverySlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    accentEverySlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 32, 26);
    accentEverySlider_.setRange(2.0, 16.0, 1.0);
    accentEverySlider_.setTooltip("Accent every 2 to 16 steps.");
    accentEveryAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kAccentEvery, accentEverySlider_);
    addAndMakeVisible(accentEverySlider_);

    humanizeLabel_.setText("Human", juce::dontSendNotification);
    humanizeLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    humanizeLabel_.setTooltip("Subtle deterministic per-step velocity humanization");
    addAndMakeVisible(humanizeLabel_);
    humanizeSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    humanizeSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    humanizeSlider_.setRange(0.0, 100.0, 0.1);
    humanizeSlider_.setTooltip("Deterministic step velocity humanization amount");
    humanizeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kHumanize, humanizeSlider_);
    addAndMakeVisible(humanizeSlider_);

    for (int i = 0; i < openfruit::core::kNumPages; ++i) {
        const auto index = static_cast<size_t>(i);
        sectionLabels_[index].setText("Section " + juce::String(i + 1),
                                      juce::dontSendNotification);
        sectionLabels_[index].setFont(juce::FontOptions(14.0f, juce::Font::bold));
        addAndMakeVisible(sectionLabels_[index]);

        auto& pageBox = sectionPageBoxes_[index];
        pageBox.addItem("A", 1);
        pageBox.addItem("B", 2);
        pageBox.addItem("C", 3);
        pageBox.addItem("D", 4);
        pageBox.setTooltip("Pattern page for this song section");
        sectionPageAttachments_[index] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts_, openfruit::params::songSectionPageId(i), pageBox);
        addAndMakeVisible(pageBox);

        auto& barsSlider = sectionBarsSliders_[index];
        barsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        barsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 47, 27);
        barsSlider.setRange(1.0, 32.0, 1.0);
        barsSlider.setTooltip("Number of bars in this section");
        sectionBarsAttachments_[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts_, openfruit::params::songSectionBarsId(i), barsSlider);
        addAndMakeVisible(barsSlider);

        auto& fillButton = sectionFillButtons_[index];
        fillButton.setButtonText("Fill");
        fillButton.setClickingTogglesState(true);
        fillButton.setTooltip("Use Page D as a one-bar fill at this section's end");
        sectionFillAttachments_[index] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, openfruit::params::songSectionFillId(i), fillButton);
        addAndMakeVisible(fillButton);
    }

    updateFromParameters();
}

void HeaderPanel::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    repaint();
}

void HeaderPanel::updateFromParameters() {
    const int page = static_cast<int>(*apvts_.getRawParameterValue(params::kPage));
    const int count = static_cast<int>(*apvts_.getRawParameterValue(params::kTrackCount));
    engine_.setActivePage(static_cast<core::PageIndex>(page));
    constexpr core::TrackCountMode counts[] = {
        core::TrackCountMode::Tracks4, core::TrackCountMode::Tracks8,
        core::TrackCountMode::Tracks12, core::TrackCountMode::Tracks16
    };
    if (count >= 0 && count < 4)
        engine_.setTrackCountMode(counts[count]);
    engine_.setMasterVolume(*apvts_.getRawParameterValue(params::kMasterVol));
    for (int i = 0; i < 4; ++i) {
        pageBtns_[static_cast<size_t>(i)].setToggleState(i == page, juce::dontSendNotification);
        trackCountBtns_[static_cast<size_t>(i)].setToggleState(i == count, juce::dontSendNotification);
    }
    const bool song = *apvts_.getRawParameterValue(params::kSongMode) > .5f;
    for (size_t i = 0; i < 4; ++i) {
        sectionLabels_[i].setVisible(song);
        sectionPageBoxes_[i].setVisible(song);
        sectionBarsSliders_[i].setVisible(song);
        sectionFillButtons_[i].setVisible(song);
    }
    if (song != arrangementVisible_) {
        arrangementVisible_ = song;
        if (onArrangementChanged) onArrangementChanged();
    }
    const int themeId = static_cast<int>(*apvts_.getRawParameterValue(params::kTheme)) + 1;
    if (themeId != themeBox_.getSelectedId()) {
        themeBox_.setSelectedId(themeId, juce::dontSendNotification);
        engine_.setColorTheme(static_cast<core::ColorTheme>(themeId-1));
        if (onThemeChanged) onThemeChanged();
    }
    masterVolSlider_.setValue(*apvts_.getRawParameterValue(params::kMasterVol), juce::dontSendNotification);
    const bool playing = engine_.isPlaying();
    playBtn_.setButtonText(playing ? "Pause" : "Play");
    playBtn_.setToggleState(playing, juce::dontSendNotification);
    const int transport = playing ? 2 : static_cast<int>(engine_.isHostSyncArmed());
    const std::array<const char*, 3> transportNames = { "Stopped", "Armed", "Playing" };
    const std::array<juce::Colour, 3> transportColours = { palette_.textDim, palette_.accentSecondary, palette_.accentPrimary };
    transportState_.setText(transportNames[static_cast<size_t>(transport)], juce::dontSendNotification);
    transportState_.setColour(juce::Label::textColourId, transportColours[static_cast<size_t>(transport)]);
    const float phase = engine_.displayBarPhase();
    if (std::abs(phase - lastPhase_) > 1.0e-6f) { lastPhase_ = phase; repaint(8, 6, 75, 75); }
}

void HeaderPanel::paint(juce::Graphics& g) {
    g.fillAll(palette_.background);
    g.setColour(palette_.panel);
    g.fillRoundedRectangle(10, 8, static_cast<float>(getWidth()-20), 70, 10);
    g.fillRoundedRectangle(10, 84, static_cast<float>(getWidth()-20), 52, 8);
    g.setColour(palette_.panelEdge);
    g.drawRoundedRectangle(10.5f, 8.5f, static_cast<float>(getWidth()-21), 69.0f, 10.0f, 1.0f);
    g.drawRoundedRectangle(10.5f, 84.5f, static_cast<float>(getWidth()-21), 51.0f, 8.0f, 1.0f);
    if (fruitLogo_) {
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(engine_.displayBarPhase() * juce::MathConstants<float>::twoPi, 48.0f, 43.0f));
        fruitLogo_->drawWithin(g, {21, 16, 54, 54}, juce::RectanglePlacement::centred, 1.0f);
    }
    g.setColour(palette_.text);
    g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    g.drawText("openFruit", 86, 12, 160, 40, juce::Justification::centredLeft);
    g.setColour(palette_.textDim);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("v" JucePlugin_VersionString, 246, 20, 48, 24, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("Drum sampler  /  Step sequencer", 88, 47, 252, 23, juce::Justification::centredLeft);
    if (arrangementVisible_) {
        g.setColour(palette_.panel);
        g.fillRoundedRectangle(10, 142, static_cast<float>(getWidth()-20), 54, 8);
        g.setColour(palette_.panelEdge);
        g.drawRoundedRectangle(10.5f, 142.5f, static_cast<float>(getWidth()-21), 53.0f, 8.0f, 1.0f);
    }
}

void HeaderPanel::resized() {
    auto top = juce::Rectangle<int>(24, 19, getWidth()-48, 48);
    auto output = top.removeFromRight(156);
    outputDbLabel_.setBounds(output.removeFromTop(18));
    outputDbSlider_.setBounds(output.reduced(3,0));
    top.removeFromRight(10);
    auto master = top.removeFromRight(156);
    masterVolLabel_.setBounds(master.removeFromTop(18));
    masterVolSlider_.setBounds(master.reduced(3,0));
    top.removeFromRight(22);
    themeBox_.setBounds(top.removeFromRight(172).withSizeKeepingCentre(172,36));
    top.removeFromRight(12);
    playBtn_.setBounds(top.removeFromRight(92).withSizeKeepingCentre(92,36));
    top.removeFromRight(6);
    transportState_.setBounds(top.removeFromRight(76).withSizeKeepingCentre(76,36));

    auto row = juce::Rectangle<int>(20, 88, getWidth()-40, 44);
    pageLabel_.setBounds(row.removeFromLeft(54));
    for (auto& button : pageBtns_) button.setBounds(row.removeFromLeft(38).reduced(1,4));
    row.removeFromLeft(10);
    trackCountLabel_.setBounds(row.removeFromLeft(50));
    for (auto& button : trackCountBtns_) button.setBounds(row.removeFromLeft(38).reduced(1,4));
    row.removeFromLeft(10);
    songModeToggle_.setBounds(row.removeFromLeft(112).reduced(1,4));
    row.removeFromLeft(8);
    swingLabel_.setBounds(row.removeFromLeft(44));
    swingSlider_.setBounds(row.removeFromLeft(66));
    row.removeFromLeft(6);
    accentLabel_.setBounds(row.removeFromLeft(48));
    accentAmountSlider_.setBounds(row.removeFromLeft(64));
    intervalLabel_.setBounds(row.removeFromLeft(42));
    accentEverySlider_.setBounds(row.removeFromLeft(74));
    row.removeFromLeft(6);
    humanizeLabel_.setBounds(row.removeFromLeft(48));
    humanizeSlider_.setBounds(row.removeFromLeft(66));

    auto sections = juce::Rectangle<int>(22, 151, getWidth()-44, 36);
    const int width = sections.getWidth()/4;
    for (size_t i = 0; i < 4; ++i) {
        auto section = sections.removeFromLeft(width).reduced(5,0);
        sectionLabels_[i].setBounds(section.removeFromLeft(65));
        sectionPageBoxes_[i].setBounds(section.removeFromLeft(48));
        sectionFillButtons_[i].setBounds(section.removeFromRight(44));
        sectionBarsSliders_[i].setBounds(section.reduced(3,0));
    }
}

// ============================================================================
// TrackerPanel
// ============================================================================

TrackerPanel::TrackerPanel(openfruit::core::FruitEngine& engine,
                           openfruit::core::ToneCache& cache,
                           openfruit::core::AnalysisQueue& queue,
                           juce::AudioProcessorValueTreeState* apvts)
    : engine_(engine),
      cache_(cache),
      queue_(queue),
      apvts_(apvts),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    viewport_.setViewedComponent(&grid_, false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        rows_[static_cast<size_t>(i)] = std::make_unique<TrackRowComponent>(i, engine_, apvts_);
        auto* row = rows_[static_cast<size_t>(i)].get();
        row->onOpenBrowser = [this](int trackIdx) { openSampleBrowser(trackIdx); };
        row->onOpenSettings = [this](int track) { openTrackSettings(track); };
        row->onStepEdited = [this, row](openfruit::core::PageIndex page, int step,
                                        openfruit::core::StepData before, openfruit::core::StepData after) {
            if (onStepEdited) onStepEdited(row->getTrackIndex(), page, step, before, after);
        };
        grid_.addAndMakeVisible(row);
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
        if (i < visibleTrackCount_) rows_[static_cast<size_t>(i)]->updateFromEngine();
    }
    resized();
}

void TrackerPanel::updateFromEngine() {
    const int count = engine_.getActiveTrackCount();
    if (count != visibleTrackCount_) setVisibleTrackCount(count);
    for (int i = 0; i < visibleTrackCount_; ++i) {
        rows_[static_cast<size_t>(i)]->updateFromEngine();
    }
}

void TrackerPanel::updatePlayhead() {
    for (int i = 0; i < visibleTrackCount_; ++i) {
        const int step = engine_.getCurrentStep(i);
        rows_[static_cast<size_t>(i)]->updatePlayhead(engine_.isPlaying() ? step : -1);
    }
}

SampleBrowserModal* TrackerPanel::openSampleBrowser(int trackIdx, const juce::File& folder) {
    viewport_.setVisible(false);
    settings_.reset();
    activeBrowser_ = std::make_unique<SampleBrowserModal>(trackIdx, engine_, cache_, queue_);
    activeBrowser_->setTheme(palette_);
    activeBrowser_->onClose = [this] { closeSampleBrowser(); };
    activeBrowser_->onFolderChanged = [this](const juce::File& newFolder) {
        lastBrowsedFolder_ = newFolder;
    };
    if (folder.isDirectory()) {
        lastBrowsedFolder_ = folder;
        activeBrowser_->setFolder(folder);
    }
    if (!folder.isDirectory() && lastBrowsedFolder_.isDirectory()) {
        activeBrowser_->setFolder(lastBrowsedFolder_);
    }
    addAndMakeVisible(activeBrowser_.get());
    activeBrowser_->setBounds(getLocalBounds().reduced(12, 4));
    return activeBrowser_.get();
}

void TrackerPanel::closeSampleBrowser() {
    if (activeBrowser_) {
        removeChildComponent(activeBrowser_.get());
        activeBrowser_.reset();
    }
    viewport_.setVisible(true);
    updateFromEngine();
}

void TrackerPanel::openTrackSettings(int track) {
    if (!apvts_) return;
    closeSampleBrowser();
    viewport_.setVisible(false);
    settings_ = std::make_unique<TrackVoiceSettingsPanel>(track, *apvts_);
    settings_->onClose = [this] { settings_.reset(); viewport_.setVisible(true); repaint(); };
    addAndMakeVisible(*settings_);
    resized();
}

void TrackerPanel::paint(juce::Graphics& g) {
    g.fillAll(palette_.background);
    if (settings_ || activeBrowser_) return;
    g.setColour(palette_.text);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("Pattern " + juce::String(core::pageName(engine_.getActivePage())), 20, 0, 200, 37, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(13.0f));
    g.setColour(palette_.textDim);
    g.drawText("Click a pad to toggle   /   Drag for velocity   /   Right-click to edit", 190, 2, getWidth()-216, 34, juce::Justification::centredRight);
    g.drawText("Track   /   Sample", 23, 41, 206, 24, juce::Justification::centredLeft);
    const int start = 12 + kLaneWidth;
    const int width = grid_.getWidth() - kLaneWidth;
    for (int i = 0; i < 16; ++i) {
        g.setColour(i % 4 == 0 ? palette_.accentSecondary : palette_.textDim);
        g.drawText(juce::String(i+1), start+i*width/16, 41, (i+1)*width/16-i*width/16, 24, juce::Justification::centred);
    }
}

void TrackerPanel::resized() {
    auto area = getLocalBounds().reduced(12,4);
    if (activeBrowser_) activeBrowser_->setBounds(area);
    if (settings_) settings_->setBounds(area);
    area.removeFromTop(64);
    viewport_.setBounds(area);
    const int rowHeight = juce::jlimit(46, 74, area.getHeight() / visibleTrackCount_);
    const int height = rowHeight * visibleTrackCount_;
    const int width = area.getWidth() - (height > area.getHeight() ? viewport_.getScrollBarThickness() : 0);
    grid_.setSize(width, height);
    for (int i = 0; i < visibleTrackCount_; ++i)
        rows_[static_cast<size_t>(i)]->setBounds(0, i*rowHeight, width, rowHeight);
    repaint();
}

} // namespace openfruit::ui
