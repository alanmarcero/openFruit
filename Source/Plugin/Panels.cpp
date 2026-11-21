#include "Panels.h"
#include "Parameters.h"
#include <BinaryData.h>

namespace openfruit::ui {

HeaderPanel::HeaderPanel(openfruit::core::FruitEngine& engine,
                         juce::AudioProcessorValueTreeState& apvts,
                         juce::ValueTree session)
    : engine_(engine),
      apvts_(apvts),
      session_(std::move(session)),
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
        pageBtns_[i].onPopupMenu = [this, i] {
            if (onPatternMenu) onPatternMenu(pageBtns_[i], static_cast<openfruit::core::PageIndex>(i));
        };
        pageBtns_[i].setButtonText(pageNames[i]);
        pageBtns_[i].setClickingTogglesState(false);
        pageBtns_[i].onClick = [this, pageIdx = static_cast<openfruit::core::PageIndex>(i)] {
            if (auto* param = apvts_.getParameter(openfruit::params::kPage)) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(pageIdx)));
                param->endChangeGesture();
            }
            engine_.setActivePage(pageIdx);
            session_.setProperty("followSong", false, nullptr);
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

    // Value boxes show the readout, drag like a slider and accept typing.
    const auto styleValueBox = [](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 36);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    };

    songModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, openfruit::params::kSongMode, songModeToggle_);
    songModeToggle_.setButtonText("Song mode");
    songModeToggle_.setTooltip("Chain the four sections in Song Mode");
    addAndMakeVisible(songModeToggle_);
    followToggle_.setTooltip("Show the pattern the song is playing. Picking a pattern turns this off.");
    followToggle_.onClick = [this] { session_.setProperty("followSong", followToggle_.getToggleState(), nullptr); };
    addChildComponent(followToggle_);
    addAndMakeVisible(intervalLabel_);

    swingLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(swingLabel_);
    styleValueBox(swingSlider_);
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
    styleValueBox(accentAmountSlider_);
    accentAmountSlider_.setRange(0.0, 1.0, 0.01);
    accentAmountSlider_.setTooltip("Periodic accent amount. Stored step velocities remain unchanged.");
    accentAmountAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kAccentAmount, accentAmountSlider_);
    addAndMakeVisible(accentAmountSlider_);
    // The stepper buttons copy the tooltip when the style creates them.
    accentEverySlider_.setTooltip("Accent every 2 to 16 steps. Raise Accent above 0% to hear it.");
    accentEverySlider_.setSliderStyle(juce::Slider::IncDecButtons);
    accentEverySlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 36, 36);
    accentEverySlider_.setRange(2.0, 16.0, 1.0);
    accentEveryAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, openfruit::params::kAccentEvery, accentEverySlider_);
    addAndMakeVisible(accentEverySlider_);

    humanizeLabel_.setText("Human", juce::dontSendNotification);
    humanizeLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    humanizeLabel_.setTooltip("Subtle deterministic per-step velocity humanization");
    addAndMakeVisible(humanizeLabel_);
    styleValueBox(humanizeSlider_);
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
        styleValueBox(barsSlider);
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
    const bool song = *apvts_.getRawParameterValue(params::kSongMode) > .5f;
    const bool playing = engine_.isPlaying();

    // In Song mode the sections pick the playing page, and Follow shows it.
    std::array<core::SongSection, core::kNumPages> sections;
    for (int i = 0; i < core::kNumPages; ++i)
        sections[static_cast<size_t>(i)] = {
            static_cast<core::PageIndex>(static_cast<int>(*apvts_.getRawParameterValue(params::songSectionPageId(i)))),
            static_cast<int>(*apvts_.getRawParameterValue(params::songSectionBarsId(i))),
            *apvts_.getRawParameterValue(params::songSectionFillId(i)) > 0.5f };
    const auto position = core::songPositionAt(sections, std::max(0.0, engine_.displayPpq()));
    const bool follow = session_.getProperty("followSong", false);
    followToggle_.setToggleState(follow, juce::dontSendNotification);
    shownPage_ = song && playing && follow ? position.page : static_cast<core::PageIndex>(page);
    playheadLive_ = !song || position.page == shownPage_;
    const int playingSection = song && playing ? position.section : -1;
    for (size_t i = 0; i < sectionLabels_.size(); ++i) {
        juce::String label = "Section " + juce::String(i + 1);
        if (static_cast<int>(i) == playingSection)
            label << "\nbar " << (position.bar + 1) << " of " << sections[i].bars;
        sectionLabels_[i].setText(label, juce::dontSendNotification);
    }
    if (playingSection != playingSection_) {
        playingSection_ = playingSection;
        repaint();
    }

    for (int i = 0; i < 4; ++i) {
        const juce::String verb = song ? "Edit pattern " : "Edit and play pattern ";
        const juce::String note = song ? ". The song sections choose what plays." : ".";
        pageBtns_[static_cast<size_t>(i)].setTooltip(verb + core::pageName(static_cast<core::PageIndex>(i)) + note
                                                     + " Right-click to copy, paste, clear or shift it.");
        pageBtns_[static_cast<size_t>(i)].setToggleState(i == static_cast<int>(shownPage_), juce::dontSendNotification);
        trackCountBtns_[static_cast<size_t>(i)].setToggleState(i == count, juce::dontSendNotification);
    }
    accentEverySlider_.setAlpha(*apvts_.getRawParameterValue(params::kAccentAmount) > 0.0f ? 1.0f : 0.4f);
    followToggle_.setVisible(song);
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
        if (playingSection_ >= 0) {
            g.setColour(palette_.accentPrimary.withAlpha(0.18f));
            g.fillRoundedRectangle(sectionBounds_[static_cast<size_t>(playingSection_)].expanded(3).toFloat(), 6.0f);
        }
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
    swingSlider_.setBounds(row.removeFromLeft(64).reduced(0,4));
    row.removeFromLeft(8);
    accentLabel_.setBounds(row.removeFromLeft(50));
    accentAmountSlider_.setBounds(row.removeFromLeft(64).reduced(0,4));
    row.removeFromLeft(6);
    intervalLabel_.setBounds(row.removeFromLeft(42));
    accentEverySlider_.setBounds(row.removeFromLeft(116).reduced(0,4));
    row.removeFromLeft(8);
    humanizeLabel_.setBounds(row.removeFromLeft(48));
    humanizeSlider_.setBounds(row.removeFromLeft(64).reduced(0,4));

    auto sections = juce::Rectangle<int>(22, 151, getWidth()-44, 36);
    followToggle_.setBounds(sections.removeFromLeft(80));
    sections.removeFromLeft(8);
    const int width = sections.getWidth()/4;
    for (size_t i = 0; i < 4; ++i) {
        auto section = sections.removeFromLeft(width).reduced(5,0);
        sectionBounds_[i] = section;
        sectionLabels_[i].setBounds(section.removeFromLeft(84));
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
                           juce::AudioProcessorValueTreeState* apvts,
                           juce::ValueTree session)
    : engine_(engine),
      cache_(cache),
      queue_(queue),
      apvts_(apvts),
      session_(std::move(session)),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    viewport_.setViewedComponent(&grid_, false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        rows_[static_cast<size_t>(i)] = std::make_unique<TrackRowComponent>(i, engine_, apvts_);
        auto* row = rows_[static_cast<size_t>(i)].get();
        row->onOpenBrowser = [this](int trackIdx) { openSampleBrowser(trackIdx); };
        row->onOpenSettings = [this](int track) { openTrackSettings(track); };
        row->onStepsEdited = [this](std::vector<openfruit::plugin::StepChange> changes) {
            if (onStepsEdited) onStepsEdited(std::move(changes));
        };
        row->onRowMenu = [this](juce::Component& target, openfruit::core::PageIndex page, int track) {
            if (onRowMenu) onRowMenu(target, page, track);
        };
        grid_.addAndMakeVisible(row);
    }
    stepsViewBtn_.setTooltip("Show the step pads");
    mixViewBtn_.setTooltip("Show each track's level and pan");
    for (auto* button : { &stepsViewBtn_, &mixViewBtn_ }) {
        button->setRadioGroupId(1);
        button->setClickingTogglesState(true);
        button->onClick = [this] { setMixView(mixViewBtn_.getToggleState()); };
        addAndMakeVisible(button);
    }
    setMixView(session_.getProperty("mixView", false));
}

void TrackerPanel::setMixView(bool mix) {
    mixView_ = mix;
    session_.setProperty("mixView", mix, nullptr);
    mixViewBtn_.setToggleState(mix, juce::dontSendNotification);
    stepsViewBtn_.setToggleState(!mix, juce::dontSendNotification);
    for (auto& row : rows_)
        row->setMixView(mix);
    repaint();
}

void TrackerPanel::showGrid(bool visible) {
    viewport_.setVisible(visible);
    stepsViewBtn_.setVisible(visible);
    mixViewBtn_.setVisible(visible);
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
    // A muted track, or any other track while one is soloed, plays nothing.
    const bool anySolo = std::any_of(rows_.begin(), rows_.begin() + visibleTrackCount_,
        [this](const auto& row) { return static_cast<bool>(engine_.getTrack(row->getTrackIndex()).solo); });
    for (int i = 0; i < visibleTrackCount_; ++i)
        rows_[static_cast<size_t>(i)]->setAnySolo(anySolo);
}

void TrackerPanel::setShownPage(core::PageIndex page, bool playheadLive) {
    if (playheadLive != playheadLive_) {
        playheadLive_ = playheadLive;
        repaint(0, 38, getWidth(), 28);
    }
    if (page == shownPage_)
        return;
    shownPage_ = page;
    for (auto& row : rows_)
        row->setShownPage(page);
    repaint();
}

void TrackerPanel::updatePlayhead() {
    uint32_t steps = 0;
    for (int i = 0; i < visibleTrackCount_; ++i) {
        const int step = engine_.isPlaying() ? engine_.getCurrentStep(i) : -1;
        rows_[static_cast<size_t>(i)]->updatePlayhead(step, playheadLive_);
        if (step >= 0) steps |= 1u << step;
    }
    if (steps != playheadSteps_) {
        playheadSteps_ = steps;
        repaint(0, 38, getWidth(), 28);
    }
}

SampleBrowserModal* TrackerPanel::openSampleBrowser(int trackIdx, const juce::File& folder) {
    showGrid(false);
    settings_.reset();
    const juce::File savedFolder(session_.isValid() ? session_.getProperty("sampleFolder").toString() : juce::String());
    juce::File initialFolder = savedFolder.isDirectory() ? savedFolder : juce::File();
    if (lastBrowsedFolder_.isDirectory()) {
        initialFolder = lastBrowsedFolder_;
    }
    if (folder.isDirectory()) {
        initialFolder = folder;
    }
    if (initialFolder.isDirectory()) {
        lastBrowsedFolder_ = initialFolder;
        if (session_.isValid()) {
            session_.setProperty("sampleFolder", initialFolder.getFullPathName(), nullptr);
        }
    }
    activeBrowser_ = std::make_unique<SampleBrowserModal>(trackIdx, engine_, cache_, queue_, initialFolder);
    activeBrowser_->setTheme(palette_);
    activeBrowser_->onClose = [this] { closeSampleBrowser(); };
    activeBrowser_->onFolderChanged = [this](const juce::File& newFolder) {
        lastBrowsedFolder_ = newFolder;
        if (session_.isValid() && newFolder.isDirectory()) {
            session_.setProperty("sampleFolder", newFolder.getFullPathName(), nullptr);
        }
    };
    addAndMakeVisible(activeBrowser_.get());
    activeBrowser_->setBounds(getLocalBounds().reduced(12, 4));
    activeBrowser_->grabKeyboardFocus();
    return activeBrowser_.get();
}

void TrackerPanel::closeSampleBrowser() {
    if (activeBrowser_) {
        removeChildComponent(activeBrowser_.get());
        activeBrowser_.reset();
    }
    showGrid(true);
    updateFromEngine();
}

void TrackerPanel::openTrackSettings(int track) {
    if (!apvts_) return;
    closeSampleBrowser();
    showGrid(false);
    settings_ = std::make_unique<TrackVoiceSettingsPanel>(track, *apvts_);
    settings_->onClose = [this] { settings_.reset(); showGrid(true); repaint(); };
    settings_->onStepTrack = [this, track](int delta) {
        const int count = visibleTrackCount_;
        openTrackSettings((track + delta + count) % count);
    };
    addAndMakeVisible(*settings_);
    resized();
}

void TrackerPanel::paint(juce::Graphics& g) {
    g.fillAll(palette_.background);
    if (settings_ || activeBrowser_) return;
    g.setColour(palette_.text);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("Pattern " + juce::String(core::pageName(shownPage_)), 20, 0, 200, 37, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(13.0f));
    g.setColour(palette_.textDim);
    g.drawText("Click a pad to toggle   /   Drag for velocity   /   Right-click to edit", 190, 2, getWidth()-216, 34, juce::Justification::centredRight);
    g.drawText("Track   /   Sample", 23, 41, 206, 24, juce::Justification::centredLeft);
    const int start = 12 + kLaneWidth;
    const int width = grid_.getWidth() - kLaneWidth;
    if (mixView_) {
        g.setColour(palette_.textDim);
        g.drawText("Level", start, 41, width / 2, 24, juce::Justification::centred);
        g.drawText("Pan", start + width / 2, 41, width / 2, 24, juce::Justification::centred);
        return;
    }
    for (int i = 0; i < 16; ++i) {
        const auto number = juce::Rectangle<int>(start+i*width/16, 41, (i+1)*width/16-i*width/16, 24);
        const bool playing = ((playheadSteps_ >> i) & 1u) != 0;
        if (playing) {
            g.setColour(palette_.text.withAlpha(playheadLive_ ? 1.0f : 0.35f));
            g.fillRoundedRectangle(number.reduced(6, 2).toFloat(), 4.0f);
        }
        const auto numberColour = i % 4 == 0 ? palette_.accentSecondary : palette_.textDim;
        g.setColour(playing ? palette_.background : numberColour);
        g.drawText(juce::String(i+1), number, juce::Justification::centred);
    }
}

void TrackerPanel::resized() {
    auto area = getLocalBounds().reduced(12,4);
    stepsViewBtn_.setBounds(190, 1, 76, 36);
    mixViewBtn_.setBounds(268, 1, 64, 36);
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
