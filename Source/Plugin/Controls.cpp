#include "Controls.h"
#include "Parameters.h"
#include <iomanip>
#include <sstream>

namespace openfruit::ui {

CyberStepPad::CyberStepPad(int trackIdx, int stepIdx)
    : trackIdx_(trackIdx), stepIdx_(stepIdx), palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    setRepaintsOnMouseActivity(true);
    setTooltip("Click to toggle. Drag vertically for velocity. Right-drag or scroll to tune. Right-click for step settings.");
    setName("Track " + juce::String(trackIdx+1) + " step " + juce::String(stepIdx+1));
}

void CyberStepPad::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    repaint();
}

void CyberStepPad::setStepData(const openfruit::core::StepData& data) {
    if (data_ == data) return;
    data_ = data;
    repaint();
}

void CyberStepPad::setIsCurrentPlayhead(bool isPlayhead) {
    if (isPlayhead_ == isPlayhead) return;
    isPlayhead_ = isPlayhead;
    repaint();
}

void CyberStepPad::paint(juce::Graphics& g) {
    auto cell = getLocalBounds().toFloat().reduced(1.5f, 3.0f);
    const auto accent = trackIdx_ % 4 < 2 ? palette_.accentPrimary : palette_.accentSecondary;
    const auto off = (stepIdx_ / 4) % 2 == 0 ? palette_.padInactiveBeatA : palette_.padInactiveBeatB;
    g.setColour(data_.active ? accent.withMultipliedBrightness(.75f + .25f*data_.velocity) : off);
    g.fillRoundedRectangle(cell, 5.0f);
    if (data_.active) {
        const auto ink = juce::Colour(0xff171b20);
        g.setColour(ink.withAlpha(.34f));
        auto velocity = cell.reduced(5.0f).removeFromBottom(3.0f);
        g.fillRoundedRectangle(velocity, 1.5f);
        g.setColour(ink);
        g.fillRoundedRectangle(velocity.withWidth(velocity.getWidth() * data_.velocity), 1.5f);
        if (data_.semitoneOffset != 0 || data_.ratchets > 1) {
            const juce::String value = data_.ratchets > 1 ? "x" + juce::String(data_.ratchets)
                    : (data_.semitoneOffset > 0 ? "+" : "") + juce::String(data_.semitoneOffset);
            g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            g.drawText(value, cell.toNearestInt(), juce::Justification::centred);
        }
    }
    if (isPlayhead_ || isMouseOver() || hasKeyboardFocus(false)) {
        g.setColour(isPlayhead_ ? palette_.playheadGlow : palette_.text);
        g.drawRoundedRectangle(cell, 5.0f, 2.0f);
    }
}

void CyberStepPad::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        // Right drag tunes the hit; a stationary right click opens its detail menu.
        dragStartPos_ = e.getPosition();
        dragStartTune_ = data_.semitoneOffset;
        tuneDragged_ = false;
        tuneGesture_ = true;
        return;
    }

    // Left click toggles step
    dragStartPos_ = e.getPosition();
    dragStartVelocity_ = data_.velocity;
    tuneGesture_ = false;
    wasActive_ = data_.active;
    velocityDragged_ = false;
}

void CyberStepPad::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        // Drag vertically to adjust semitone tune (-24 .. +24)
        const int deltaY = dragStartPos_.y - e.y;
        if (std::abs(deltaY) >= 4) tuneDragged_ = true;
        const int semitones = std::clamp(dragStartTune_ + deltaY / 4, -24, 24);
        if (semitones != data_.semitoneOffset) {
            data_.semitoneOffset = semitones;
            repaint();
            if (onTuneChange) {
                onTuneChange(semitones);
            }
        }
    } else if (e.mods.isLeftButtonDown() && std::abs(dragStartPos_.y - e.y) >= 3) {
        velocityDragged_ = true;
        if (!data_.active) {
            data_.active = true;
            if (onToggle) onToggle(true);
        }
        // Drag vertically while active to adjust velocity
        const float delta = static_cast<float>(dragStartPos_.y - e.y) / 60.0f;
        const float newVel = std::clamp(dragStartVelocity_ + delta * 0.5f, 0.05f, 1.0f);
        if (std::abs(newVel - data_.velocity) > 0.02f) {
            data_.velocity = newVel;
            repaint();
            if (onVelocityChange) {
                onVelocityChange(newVel);
            }
        }
    }
}

void CyberStepPad::mouseUp(const juce::MouseEvent& e) {
    if (tuneGesture_) {
        if (e.mouseWasClicked() && !tuneDragged_) showStepMenu();
        tuneGesture_ = false;
    } else if (!velocityDragged_) {
        data_.active = !wasActive_;
        if (onToggle) onToggle(data_.active);
        repaint();
    }
}

void CyberStepPad::showStepMenu() {
    juce::PopupMenu probability;
    probability.addItem(101, "0% (never)");
    probability.addItem(102, "25%");
    probability.addItem(103, "50%");
    probability.addItem(104, "75%");
    probability.addItem(105, "100%");

    juce::PopupMenu ratchets;
    for (int i = 1; i <= 8; ++i) ratchets.addItem(200 + i, juce::String(i));

    juce::PopupMenu offset;
    offset.addItem(301, "-50%");
    offset.addItem(302, "-25%");
    offset.addItem(303, "0%");
    offset.addItem(304, "+25%");
    offset.addItem(305, "+50%");

    juce::PopupMenu start;
    start.addItem(401, "0%");
    start.addItem(402, "10%");
    start.addItem(403, "25%");
    start.addItem(404, "50%");

    juce::PopupMenu end;
    end.addItem(501, "50%");
    end.addItem(502, "75%");
    end.addItem(503, "90%");
    end.addItem(504, "100%");

    juce::PopupMenu choke;
    for (int i = 0; i <= 4; ++i)
        choke.addItem(600 + i, i == 0 ? "Off" : juce::String(i));

    juce::PopupMenu menu;
    menu.addSubMenu("Probability", probability);
    menu.addSubMenu("Ratchets", ratchets);
    menu.addSubMenu("Timing offset", offset);
    menu.addSubMenu("Sample start", start);
    menu.addSubMenu("Sample end", end);
    menu.addItem(700, "Reverse", true, data_.reverse);
    menu.addSubMenu("Choke group", choke);
    menu.setLookAndFeel(&getLookAndFeel());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        juce::ModalCallbackFunction::create([safeThis = juce::Component::SafePointer<CyberStepPad>(this)](int result) {
            if (safeThis != nullptr && result != 0) safeThis->selectStepMenuItem(result);
        }));
}

void CyberStepPad::selectStepMenuItem(int itemId) {
    if (itemId >= 101 && itemId <= 105) {
        constexpr float probabilities[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        data_.probability = probabilities[itemId - 101];
    } else if (itemId >= 201 && itemId <= 208) {
        data_.ratchets = itemId - 200;
    } else if (itemId >= 301 && itemId <= 305) {
        constexpr float offsets[] = {-0.5f, -0.25f, 0.0f, 0.25f, 0.5f};
        data_.timingOffset = offsets[itemId - 301];
    } else if (itemId >= 401 && itemId <= 404) {
        constexpr float starts[] = {0.0f, 0.1f, 0.25f, 0.5f};
        data_.sampleStart = starts[itemId - 401];
    } else if (itemId >= 501 && itemId <= 504) {
        constexpr float ends[] = {0.5f, 0.75f, 0.9f, 1.0f};
        data_.sampleEnd = ends[itemId - 501];
    } else if (itemId == 700) {
        data_.reverse = !data_.reverse;
    } else if (itemId >= 600 && itemId <= 604) {
        data_.chokeGroup = itemId - 600;
    }

    // Keep sample regions valid when start and end are adjusted independently.
    data_.sampleStart = std::min(data_.sampleStart, std::max(0.0f, data_.sampleEnd - 0.01f));
    data_.sampleEnd = std::max(data_.sampleEnd, std::min(1.0f, data_.sampleStart + 0.01f));
    repaint();
    if (onStepDataChange) onStepDataChange(data_);
}

void CyberStepPad::mouseWheelMove(const juce::MouseEvent& /*e*/, const juce::MouseWheelDetails& wheel) {
    if (!data_.active) return;
    const int delta = (wheel.deltaY > 0.0f) ? 1 : -1;
    const int newTune = std::clamp(data_.semitoneOffset + delta, -24, 24);
    if (newTune != data_.semitoneOffset) {
        data_.semitoneOffset = newTune;
        repaint();
        if (onTuneChange) {
            onTuneChange(newTune);
        }
    }
}

// ============================================================================
// TrackRowComponent
// ============================================================================

namespace {

void setApvtsParamValue(juce::AudioProcessorValueTreeState* apvts,
                        const std::string& paramId,
                        float plainValue) {
    if (apvts == nullptr) {
        return;
    }
    if (auto* param = apvts->getParameter(paramId)) {
        param->setValueNotifyingHost(param->convertTo0to1(plainValue));
    }
}

} // namespace

TrackVoiceSettingsPanel::TrackVoiceSettingsPanel(int trackIdx,
                                                 juce::AudioProcessorValueTreeState& apvts)
{
    track_ = trackIdx;
    close_.onClick = [this] { if (onClose) onClose(); };
    close_.setTooltip("Return to the pattern grid");
    addAndMakeVisible(close_);
    for (int i = 1; i <= 16; ++i) midi_.addItem("Channel " + juce::String(i), i);
    midi_.setTooltip("Incoming MIDI channel for this track");
    midiAttachment_ = std::make_unique<juce::ParameterAttachment>(*apvts.getParameter(params::trackMidiChId(trackIdx)),
        [this](float value) { midi_.setSelectedId(juce::roundToInt(value), juce::dontSendNotification); });
    midiAttachment_->sendInitialUpdate();
    midi_.onChange = [this] { midiAttachment_->setValueAsCompleteGesture(static_cast<float>(midi_.getSelectedId())); };
    timing_.addItemList({"1/16", "1/32", "1/64"}, 1);
    timing_.setTooltip("Duration of one step for this track");
    timingAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, params::trackTimingId(trackIdx), timing_);
    addAndMakeVisible(midi_);
    addAndMakeVisible(timing_);
    routingLabels_[0].setText("MIDI input", juce::dontSendNotification);
    routingLabels_[1].setText("Step rate", juce::dontSendNotification);
    for (auto& label : routingLabels_) addAndMakeVisible(label);
    const std::array<std::string, 10> ids = {
        params::trackTuneId(trackIdx), params::trackVolId(trackIdx), params::trackPanId(trackIdx),
        params::trackFilterId(trackIdx), params::trackNudgeId(trackIdx), params::trackGrooveId(trackIdx),
        params::trackAttackId(trackIdx), params::trackDecayId(trackIdx), params::trackSustainId(trackIdx), params::trackReleaseId(trackIdx)};
    const std::array<const char*, 10> names = {"Pitch", "Level", "Pan", "Low-pass", "Nudge", "Groove", "Attack", "Decay", "Sustain", "Release"};
    const std::array<const char*, 10> units = {" st", "", "", " Hz", " stp", " stp", " s", " s", "%", " s"};
    for (size_t i = 0; i < sliders_.size(); ++i) {
        labels_[i].setText(names[i], juce::dontSendNotification);
        labels_[i].setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(labels_[i]);

        auto& slider = sliders_[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 82, 28);
        slider.setTooltip(juce::String(names[i]) + " for track " + juce::String(trackIdx+1));
        addAndMakeVisible(slider);
        attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ids[i], slider);
        slider.setTextValueSuffix(units[i]);
        slider.setNumDecimalPlacesToDisplay(i == 0 || i == 3 || i == 8 ? 0 : 2);
    }
}

void TrackVoiceSettingsPanel::paint(juce::Graphics& g) {
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId, true));
    auto area = getLocalBounds().reduced(36, 16);
    g.setColour(findColour(juce::Label::textColourId, true));
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("Track " + juce::String(track_ + 1).paddedLeft('0', 2) + "   /   Voice settings",
               area.removeFromTop(48), juce::Justification::centredLeft);
    area.removeFromTop(70);
    const int column = area.getWidth() / 2;
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.setColour(findColour(juce::Slider::thumbColourId, true));
    g.drawText("Mix and timing", area.getX(), area.getY(), column, 26, juce::Justification::centredLeft);
    g.drawText("Amplitude envelope", area.getX()+column, area.getY(), column, 26, juce::Justification::centredLeft);
}

void TrackVoiceSettingsPanel::resized() {
    auto area = getLocalBounds().reduced(36, 16);
    auto title = area.removeFromTop(48);
    close_.setBounds(title.removeFromRight(98).reduced(0, 5));
    auto route = area.removeFromTop(70).reduced(0, 17);
    for (size_t i = 0; i < 2; ++i) {
        routingLabels_[i].setBounds(route.removeFromLeft(82));
        (i == 0 ? midi_ : timing_).setBounds(route.removeFromLeft(138));
        route.removeFromLeft(28);
    }
    area.removeFromTop(34);
    const int w = area.getWidth()/2;
    for (size_t i = 0; i < sliders_.size(); ++i) {
        auto row = juce::Rectangle<int>(area.getX() + (i < 6 ? 0 : w), area.getY() + 43*static_cast<int>(i < 6 ? i : i-6), w-26, 38);
        labels_[i].setBounds(row.removeFromLeft(72));
        sliders_[i].setBounds(row);
    }
}

TrackRowComponent::TrackRowComponent(int trackIdx,
                                     openfruit::core::FruitEngine& engine,
                                     juce::AudioProcessorValueTreeState* apvts)
    : trackIdx_(trackIdx),
      engine_(engine),
      apvts_(apvts),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

    // 1. Sample Trigger Pad Button
    samplePadBtn_.setButtonText("Loading");
    samplePadBtn_.onClick = [this] {
        engine_.triggerTrackManual(trackIdx_);
        if (onSampleAudition) onSampleAudition(trackIdx_);
    };
    addAndMakeVisible(samplePadBtn_);

    // 2. Load / Browse Button
    loadBtn_.setButtonText("Browse");
    loadBtn_.onClick = [this] {
        if (onOpenBrowser) onOpenBrowser(trackIdx_);
    };
    addAndMakeVisible(loadBtn_);

    // 7. Mute & Solo
    muteBtn_.onClick = [this] {
        const bool m = muteBtn_.getToggleState();
        engine_.getTrack(trackIdx_).mute = m;
        setApvtsParamValue(apvts_, openfruit::params::trackMuteId(trackIdx_), m ? 1.0f : 0.0f);
    };
    muteBtn_.setTooltip("Mute this track");
    addAndMakeVisible(muteBtn_);

    soloBtn_.onClick = [this] {
        const bool s = soloBtn_.getToggleState();
        engine_.getTrack(trackIdx_).solo = s;
        setApvtsParamValue(apvts_, openfruit::params::trackSoloId(trackIdx_), s ? 1.0f : 0.0f);
    };
    soloBtn_.setTooltip("Solo this track");
    addAndMakeVisible(soloBtn_);

    voiceSettingsBtn_.setButtonText("Edit");
    voiceSettingsBtn_.setTooltip("Edit tuning, level, MIDI, timing, filter and amplitude envelope");
    voiceSettingsBtn_.onClick = [this] { if (onOpenSettings) onOpenSettings(trackIdx_); };
    loadBtn_.setTooltip("Choose a sample for this track");
    addAndMakeVisible(voiceSettingsBtn_);

    // 8. 16 Step Pads
    for (int s = 0; s < openfruit::core::kStepsPerPage; ++s) {
        stepPads_[static_cast<size_t>(s)] = std::make_unique<CyberStepPad>(trackIdx_, s);
        auto* pad = stepPads_[static_cast<size_t>(s)].get();

        pad->onToggle = [this, s](bool active) {
            engine_.setStepActive(trackIdx_, engine_.getActivePage(), s, active);
        };
        pad->onTuneChange = [this, s](int tune) {
            engine_.setStepTune(trackIdx_, engine_.getActivePage(), s, tune);
        };
        pad->onVelocityChange = [this, s](float vel) {
            engine_.setStepVelocity(trackIdx_, engine_.getActivePage(), s, vel);
        };
        pad->onStepDataChange = [this, s](openfruit::core::StepData data) {
            engine_.setStepData(trackIdx_, engine_.getActivePage(), s, data);
        };

        addAndMakeVisible(pad);
    }
}

void TrackRowComponent::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    for (auto& pad : stepPads_) {
        if (pad) pad->setTheme(palette);
    }
    repaint();
}

void TrackRowComponent::updateFromEngine() {
    const auto& track = engine_.getTrack(trackIdx_);

    const auto sample = engine_.loadedSample(trackIdx_);
    const juce::String name = sample ? sample->sampleName : track.name;
    samplePadBtn_.setButtonText(name);
    samplePadBtn_.setTooltip("Audition " + name + (sample ? "   " + juce::String(sample->toneResult.formattedTone()) : ""));
    muteBtn_.setToggleState(track.mute, juce::dontSendNotification);
    soloBtn_.setToggleState(track.solo, juce::dontSendNotification);

    // Update 16 pads for active page
    const auto activePage = engine_.getActivePage();
    for (int s = 0; s < openfruit::core::kStepsPerPage; ++s) {
        stepPads_[static_cast<size_t>(s)]->setStepData(engine_.getStep(trackIdx_, activePage, s));
    }
}

void TrackRowComponent::updatePlayhead(int currentStep) {
    for (int s = 0; s < openfruit::core::kStepsPerPage; ++s) {
        stepPads_[static_cast<size_t>(s)]->setIsCurrentPlayhead(s == currentStep);
    }
}

void TrackRowComponent::paint(juce::Graphics& g) {
    g.fillAll(trackIdx_ % 2 == 0 ? palette_.panel : palette_.background);
    g.setColour(palette_.textDim);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(juce::String(trackIdx_+1).paddedLeft('0',2), 1, 0, 29, getHeight(), juce::Justification::centred);
}

void TrackRowComponent::resized() {
    auto area = getLocalBounds();
    auto controls = area.removeFromLeft(kLaneWidth).withSizeKeepingCentre(kLaneWidth, 36);
    controls.removeFromLeft(31);
    samplePadBtn_.setBounds(controls.removeFromLeft(140).reduced(2,0));
    loadBtn_.setBounds(controls.removeFromLeft(62).reduced(2,0));
    voiceSettingsBtn_.setBounds(controls.removeFromLeft(44).reduced(2,0));
    muteBtn_.setBounds(controls.removeFromLeft(48).reduced(2,0));
    soloBtn_.setBounds(controls.removeFromLeft(48).reduced(2,0));
    for (int s = 0; s < core::kStepsPerPage; ++s) {
        const int left = area.getX() + s * area.getWidth() / 16;
        const int right = area.getX() + (s+1) * area.getWidth() / 16;
        stepPads_[static_cast<size_t>(s)]->setBounds(left, 2, right-left, area.getHeight()-4);
    }
}

} // namespace openfruit::ui
