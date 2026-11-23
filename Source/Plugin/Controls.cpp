#include "Controls.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "WavReader.h"
#include <iomanip>
#include <sstream>

namespace openfruit::ui {

void drawPadlockIndicator(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, juce::Colour background) {
    g.setColour(background.withAlpha(0.85f));
    g.fillRoundedRectangle(bounds.expanded(1.0f), 2.5f);

    const auto body = bounds.withTrimmedTop(bounds.getHeight() * 0.42f).reduced(1.0f, 0.5f);
    const float shackleWidth = body.getWidth() * 0.62f;
    const float shackleLeft = body.getCentreX() - shackleWidth * 0.5f;
    const float shackleTop = bounds.getY() + 0.5f;
    juce::Path shackle;
    shackle.addArc(shackleLeft, shackleTop, shackleWidth, body.getY() - shackleTop + shackleWidth * 0.45f,
                   juce::MathConstants<float>::pi * 1.5f, juce::MathConstants<float>::pi * 2.5f, true);
    g.setColour(accent);
    g.strokePath(shackle, juce::PathStrokeType(1.3f));
    g.fillRoundedRectangle(body, 1.5f);
}

bool isParamLockedInApvts(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId) {
    if (auto* processor = dynamic_cast<OpenFruitAudioProcessor*>(&apvts.processor))
        return processor->isParamLocked(paramId);
    return false;
}

void addParamLockMenuItems(juce::PopupMenu& menu, juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramId, int lockItemId, int unlockAllItemId) {
    auto* processor = dynamic_cast<OpenFruitAudioProcessor*>(&apvts.processor);
    auto* param = apvts.getParameter(paramId);
    if (processor == nullptr || param == nullptr)
        return;
    const bool locked = processor->isParamLocked(paramId);
    menu.addItem(lockItemId, (locked ? "Unlock " : "Lock ") + param->getName(64));
    if (processor->hasAnyLockedParam())
        menu.addItem(unlockAllItemId, "Unlock all parameters");
}

void handleParamLockMenuResult(juce::Component& target, juce::AudioProcessorValueTreeState& apvts,
                               const juce::String& paramId, int result, int lockItemId, int unlockAllItemId) {
    auto* processor = dynamic_cast<OpenFruitAudioProcessor*>(&apvts.processor);
    if (processor == nullptr)
        return;
    if (result == lockItemId) {
        processor->setParamLocked(paramId, !processor->isParamLocked(paramId));
        if (auto* top = target.getTopLevelComponent())
            top->repaint();
    } else if (result == unlockAllItemId) {
        processor->unlockAllParams();
        if (auto* top = target.getTopLevelComponent())
            top->repaint();
    }
}

void showParamLockMenu(juce::Component& target, juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId) {
    juce::PopupMenu menu;
    addParamLockMenuItems(menu, apvts, paramId, 1, 2);
    if (menu.getNumItems() == 0)
        return;
    menu.setLookAndFeel(&target.getLookAndFeel());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),
        [safeTarget = juce::Component::SafePointer<juce::Component>(&target), &apvts, paramId](int result) {
            if (safeTarget != nullptr && result != 0)
                handleParamLockMenuResult(*safeTarget, apvts, paramId, result, 1, 2);
        });
}

namespace {

constexpr std::array<float, 5> kProbabilities { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
constexpr std::array<float, 5> kOffsets { -0.5f, -0.25f, 0.0f, 0.25f, 0.5f };
constexpr std::array<float, 4> kStarts { 0.0f, 0.1f, 0.25f, 0.5f };
constexpr std::array<float, 4> kEnds { 0.5f, 0.75f, 0.9f, 1.0f };

juce::String percentText(float fraction, bool showPlus) {
    const int percent = juce::roundToInt(fraction * 100.0f);
    const juce::String sign = showPlus && percent > 0 ? "+" : "";
    return sign + juce::String(percent) + "%";
}

// One item per value, ticked when it matches the step.
template <size_t Count>
void addPercentItems(juce::PopupMenu& menu, int firstId, const std::array<float, Count>& values, float current, bool showPlus) {
    for (size_t i = 0; i < Count; ++i)
        menu.addItem(firstId + static_cast<int>(i), percentText(values[i], showPlus), true, std::abs(values[i] - current) < 1.0e-4f);
}

} // namespace

CyberStepPad::CyberStepPad(int trackIdx, int stepIdx)
    : trackIdx_(trackIdx), stepIdx_(stepIdx), palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    setRepaintsOnMouseActivity(true);
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

void CyberStepPad::setSilencedNote(const juce::String& note) {
    silencedNote_ = note;
    setAlpha(note.isEmpty() ? 1.0f : 0.4f);
}

void CyberStepPad::setIsCurrentPlayhead(bool isPlayhead, bool live) {
    if (isPlayhead_ == isPlayhead && playheadLive_ == live) return;
    isPlayhead_ = isPlayhead;
    playheadLive_ = live;
    repaint();
}

juce::String CyberStepPad::getTooltip() {
    const auto help = "\nClick to toggle. Drag across to paint, or up and down for velocity. "
                      "Right-drag or Alt+scroll to tune. Right-click for step settings.";
    const auto step = "Step " + juce::String(stepIdx_ + 1) + ": ";
    if (!data_.active)
        return step + "off." + help + silencedNote_;
    juce::StringArray settings { "velocity " + percentText(data_.velocity, false) };
    if (data_.probability < 1.0f) settings.add(percentText(data_.probability, false) + " chance");
    if (data_.ratchets > 1) settings.add(juce::String(data_.ratchets) + " ratchets");
    if (data_.semitoneOffset != 0) settings.add((data_.semitoneOffset > 0 ? "+" : "") + juce::String(data_.semitoneOffset) + " st");
    if (data_.timingOffset != 0.0f) settings.add("offset " + percentText(data_.timingOffset, true));
    if (data_.sampleStart > 0.0f || data_.sampleEnd < 1.0f)
        settings.add("plays " + percentText(data_.sampleStart, false) + " to " + percentText(data_.sampleEnd, false));
    if (data_.reverse) settings.add("reversed");
    if (data_.chokeGroup > 0) settings.add("choke group " + juce::String(data_.chokeGroup));
    return step + settings.joinIntoString(", ") + "." + help + silencedNote_;
}

void CyberStepPad::paint(juce::Graphics& g) {
    auto cell = getLocalBounds().toFloat().reduced(1.5f, 3.0f);
    const auto accent = trackIdx_ % 4 < 2 ? palette_.accentPrimary : palette_.accentSecondary;
    const auto off = (stepIdx_ / 4) % 2 == 0 ? palette_.padInactiveBeatA : palette_.padInactiveBeatB;
    const auto hit = accent.withMultipliedBrightness(.75f + .25f*data_.velocity);
    // A step that rarely plays fills more faintly.
    g.setColour(data_.active ? hit.withMultipliedAlpha(.35f + .65f*data_.probability) : off);
    g.fillRoundedRectangle(cell, 5.0f);
    if (data_.active) {
        const auto ink = juce::Colour(0xff171b20);
        g.setColour(ink.withAlpha(.34f));
        auto inner = cell.reduced(5.0f);
        auto velocity = inner.removeFromBottom(3.0f);
        inner.removeFromBottom(2.0f);
        g.fillRoundedRectangle(velocity, 1.5f);
        g.setColour(ink);
        g.fillRoundedRectangle(velocity.withWidth(velocity.getWidth() * data_.velocity), 1.5f);

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        const auto top = inner.removeFromTop(13.0f);
        if (data_.ratchets > 1)
            g.drawText("x" + juce::String(data_.ratchets), top, juce::Justification::topLeft);
        if (data_.semitoneOffset != 0)
            g.drawText((data_.semitoneOffset > 0 ? "+" : "") + juce::String(data_.semitoneOffset), top, juce::Justification::topRight);
        auto marks = inner.removeFromBottom(12.0f);
        if (data_.probability < 1.0f)
            g.drawText(percentText(data_.probability, false), inner, juce::Justification::centred);
        if (data_.timingOffset != 0.0f) {
            const float edge = data_.timingOffset < 0.0f ? inner.getX() : inner.getRight();
            const float tip = data_.timingOffset < 0.0f ? -4.0f : 4.0f;
            juce::Path arrow;
            arrow.addTriangle(edge, inner.getCentreY() - 4.0f, edge, inner.getCentreY() + 4.0f, edge + tip, inner.getCentreY());
            g.fillPath(arrow);
        }
        if (data_.sampleStart > 0.0f || data_.sampleEnd < 1.0f) {
            const auto region = marks.removeFromBottom(2.0f);
            g.fillRect(region.withTrimmedLeft(region.getWidth() * data_.sampleStart)
                             .withWidth(region.getWidth() * (data_.sampleEnd - data_.sampleStart)));
        }
        if (data_.reverse)
            g.drawText("R", marks, juce::Justification::centredLeft);
        if (data_.chokeGroup > 0)
            g.drawText("C" + juce::String(data_.chokeGroup), marks, juce::Justification::centredRight);
    }
    // The playhead uses the text colour, so it shows on lit pads of either accent.
    // A dim playhead means the shown page is not the one playing.
    if (isPlayhead_) {
        g.setColour(palette_.text.withAlpha(playheadLive_ ? 0.28f : 0.08f));
        g.fillRoundedRectangle(cell, 5.0f);
    }
    if (isPlayhead_ || isMouseOver() || hasKeyboardFocus(false)) {
        g.setColour(palette_.text.withAlpha(isPlayhead_ && !playheadLive_ ? 0.35f : 1.0f));
        g.drawRoundedRectangle(cell, 5.0f, 2.0f);
    }
}

void CyberStepPad::mouseDown(const juce::MouseEvent& e) {
    if (onEditBegan) onEditBegan();
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
    painting_ = false;
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
    } else if (e.mods.isLeftButtonDown() && !velocityDragged_ && (painting_ || e.x < 0 || e.x >= getWidth())) {
        // Dragging across pads paints them with the first pad's new state.
        if (!painting_)
            paintActive_ = !wasActive_;
        painting_ = true;
        if (onPaint) onPaint(getParentComponent()->getLocalPoint(this, e.getPosition()).x, paintActive_);
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
        if (e.mouseWasClicked() && !tuneDragged_) {
            showStepMenu();
            tuneGesture_ = false;
            return;
        }
        tuneGesture_ = false;
    } else if (!velocityDragged_ && !painting_) {
        data_.active = !wasActive_;
        if (onToggle) onToggle(data_.active);
        repaint();
    }
    if (onEditEnded) onEditEnded();
}

void CyberStepPad::showStepMenu() {
    juce::PopupMenu probability;
    addPercentItems(probability, 101, kProbabilities, data_.probability, false);

    juce::PopupMenu ratchets;
    for (int i = 1; i <= 8; ++i) ratchets.addItem(200 + i, juce::String(i), true, data_.ratchets == i);

    juce::PopupMenu offset;
    addPercentItems(offset, 301, kOffsets, data_.timingOffset, true);

    juce::PopupMenu start;
    addPercentItems(start, 401, kStarts, data_.sampleStart, false);

    juce::PopupMenu end;
    addPercentItems(end, 501, kEnds, data_.sampleEnd, false);

    juce::PopupMenu choke;
    for (int i = 0; i <= 4; ++i)
        choke.addItem(600 + i, i == 0 ? "Off" : juce::String(i), true, data_.chokeGroup == i);

    juce::PopupMenu menu;
    menu.addSubMenu("Probability", probability);
    menu.addSubMenu("Ratchets", ratchets);
    menu.addSubMenu("Timing offset", offset);
    menu.addSubMenu("Sample start", start);
    menu.addSubMenu("Sample end", end);
    menu.addItem(700, "Reverse", true, data_.reverse);
    menu.addSubMenu("Choke group", choke);
    menu.addSeparator();
    menu.addItem(800, "Reset step");
    menu.setLookAndFeel(&getLookAndFeel());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        juce::ModalCallbackFunction::create([safeThis = juce::Component::SafePointer<CyberStepPad>(this)](int result) {
            if (safeThis == nullptr) return;
            if (result != 0) safeThis->selectStepMenuItem(result);
            else if (safeThis->onEditEnded) safeThis->onEditEnded();
        }));
}

void CyberStepPad::selectStepMenuItem(int itemId) {
    if (itemId >= 101 && itemId <= 105) {
        data_.probability = kProbabilities[static_cast<size_t>(itemId - 101)];
    } else if (itemId >= 201 && itemId <= 208) {
        data_.ratchets = itemId - 200;
    } else if (itemId >= 301 && itemId <= 305) {
        data_.timingOffset = kOffsets[static_cast<size_t>(itemId - 301)];
    } else if (itemId >= 401 && itemId <= 404) {
        data_.sampleStart = kStarts[static_cast<size_t>(itemId - 401)];
    } else if (itemId >= 501 && itemId <= 504) {
        data_.sampleEnd = kEnds[static_cast<size_t>(itemId - 501)];
    } else if (itemId == 800) {
        // Reset step keeps the hit and its velocity and clears the other settings.
        openfruit::core::StepData reset;
        reset.active = data_.active;
        reset.velocity = data_.velocity;
        data_ = reset;
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
    if (onEditEnded) onEditEnded();
}

void CyberStepPad::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    // Without Alt or Cmd the wheel scrolls the grid.
    if (!(e.mods.isAltDown() || e.mods.isCommandDown()) || wheel.deltaY == 0.0f || !data_.active) {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }
    if (!isTimerRunning() && onEditBegan) onEditBegan();
    const int delta = (wheel.deltaY > 0.0f) ? 1 : -1;
    const int newTune = std::clamp(data_.semitoneOffset + delta, -24, 24);
    if (newTune != data_.semitoneOffset) {
        data_.semitoneOffset = newTune;
        repaint();
        if (onTuneChange) {
            onTuneChange(newTune);
        }
    }
    // Ticks less than half a second apart make one undo step.
    startTimer(500);
}

void CyberStepPad::timerCallback() {
    stopTimer();
    if (onEditEnded) onEditEnded();
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
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(plainValue));
        param->endChangeGesture();
    }
}

void enableDoubleClickDefault(juce::Slider& slider,
                              juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& paramId) {
    if (auto* param = apvts.getParameter(paramId)) {
        slider.setDoubleClickReturnValue(
            true, static_cast<double>(param->convertFrom0to1(param->getDefaultValue())));
    }
}

} // namespace

TrackVoiceSettingsPanel::TrackVoiceSettingsPanel(int trackIdx,
                                                 juce::AudioProcessorValueTreeState& apvts)
{
    track_ = trackIdx;
    setWantsKeyboardFocus(true);
    close_.onClick = [this] { if (onClose) onClose(); };
    close_.setTooltip("Return to the pattern grid");
    close_.setTitle("Done");
    addAndMakeVisible(close_);
    previous_.setTooltip("Previous track");
    previous_.setTitle("Previous track");
    next_.setTooltip("Next track");
    next_.setTitle("Next track");
    previous_.onClick = [this] { if (onStepTrack) onStepTrack(-1); };
    next_.onClick = [this] { if (onStepTrack) onStepTrack(1); };
    addAndMakeVisible(previous_);
    addAndMakeVisible(next_);
    for (int i = 1; i <= 16; ++i) midi_.addItem("Channel " + juce::String(i), i);
    midi_.setTooltip("Incoming MIDI channel for this track");
    midi_.setTitle("MIDI input");
    midiAttachment_ = std::make_unique<juce::ParameterAttachment>(*apvts.getParameter(params::trackMidiChId(trackIdx)),
        [this](float value) { midi_.setSelectedId(juce::roundToInt(value), juce::dontSendNotification); });
    midiAttachment_->sendInitialUpdate();
    midi_.onChange = [this] { midiAttachment_->setValueAsCompleteGesture(static_cast<float>(midi_.getSelectedId())); };
    timing_.addItemList({"1/16", "1/32", "1/64"}, 1);
    timing_.setTooltip("Duration of one step for this track");
    timing_.setTitle("Step rate");
    timingAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, params::trackTimingId(trackIdx), timing_);
    bindParamLock(midi_, apvts, params::trackMidiChId(trackIdx));
    bindParamLock(timing_, apvts, params::trackTimingId(trackIdx));
    addAndMakeVisible(midi_);
    addAndMakeVisible(timing_);
    routingLabels_[0].setText("MIDI input", juce::dontSendNotification);
    routingLabels_[1].setText("Step rate", juce::dontSendNotification);
    for (auto& label : routingLabels_) addAndMakeVisible(label);
    const std::array<std::string, 12> ids = {
        params::trackTuneId(trackIdx), params::trackVolId(trackIdx), params::trackPanId(trackIdx),
        params::trackDriveId(trackIdx), params::trackNudgeId(trackIdx), params::trackGrooveId(trackIdx),
        params::trackHpfId(trackIdx), params::trackFilterId(trackIdx),
        params::trackAttackId(trackIdx), params::trackDecayId(trackIdx), params::trackSustainId(trackIdx), params::trackReleaseId(trackIdx)};
    const std::array<const char*, 12> names = {"Pitch", "Level", "Pan", "Drive", "Nudge", "Groove", "High-pass", "Low-pass", "Attack", "Decay", "Sustain", "Release"};
    for (size_t i = 0; i < sliders_.size(); ++i) {
        labels_[i].setText(names[i], juce::dontSendNotification);
        labels_[i].setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(labels_[i]);

        auto& slider = sliders_[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 82, 28);
        slider.setTooltip(juce::String(names[i]) + " for track " + juce::String(trackIdx+1));
        slider.setTitle(names[i]);
        addAndMakeVisible(slider);
        attachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ids[i], slider);
        bindParamLock(slider, apvts, ids[i]);
        // Attack, decay and release spend more travel on short times. The parameter range stays linear.
        if (i == 8 || i == 9 || i == 11) {
            juce::NormalisableRange<double> travel(slider.getMinimum(), slider.getMaximum(), slider.getInterval());
            travel.setSkewForCentre(slider.getMaximum() / 20.0);
            slider.setNormalisableRange(travel);
        }
        enableDoubleClickDefault(slider, apvts, ids[i]);
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
    g.drawText("Filter and envelope", area.getX()+column, area.getY(), column, 26, juce::Justification::centredLeft);
}

void TrackVoiceSettingsPanel::resized() {
    auto area = getLocalBounds().reduced(36, 16);
    auto title = area.removeFromTop(48);
    close_.setBounds(title.removeFromRight(98).reduced(0, 5));
    title.removeFromRight(8);
    next_.setBounds(title.removeFromRight(44).reduced(2, 4));
    previous_.setBounds(title.removeFromRight(44).reduced(2, 4));
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

bool TrackVoiceSettingsPanel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        if (onClose) onClose();
        return true;
    }
    return false;
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
    samplePadBtn_.setTitle("Track " + juce::String(trackIdx_ + 1) + " sample");
    samplePadBtn_.onClick = [this] {
        engine_.triggerTrackManual(trackIdx_);
        if (onSampleAudition) onSampleAudition(trackIdx_);
    };
    addAndMakeVisible(samplePadBtn_);

    // 2. Load / Browse Button
    loadBtn_.setButtonText("Browse");
    loadBtn_.setTitle("Browse");
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
    for (auto* slider : { &levelSlider_, &panSlider_ }) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 28);
        addChildComponent(slider);
    }
    levelSlider_.setTooltip("Level for track " + juce::String(trackIdx_ + 1));
    levelSlider_.setTitle("Level");
    panSlider_.setTooltip("Pan for track " + juce::String(trackIdx_ + 1));
    panSlider_.setTitle("Pan");
    if (apvts_ != nullptr) {
        levelAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            *apvts_, openfruit::params::trackVolId(trackIdx_), levelSlider_);
        panAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            *apvts_, openfruit::params::trackPanId(trackIdx_), panSlider_);
        enableDoubleClickDefault(levelSlider_, *apvts_, openfruit::params::trackVolId(trackIdx_));
        enableDoubleClickDefault(panSlider_, *apvts_, openfruit::params::trackPanId(trackIdx_));
        bindParamLock(levelSlider_, *apvts_, openfruit::params::trackVolId(trackIdx_));
        bindParamLock(panSlider_, *apvts_, openfruit::params::trackPanId(trackIdx_));
        bindParamLock(muteBtn_, *apvts_, openfruit::params::trackMuteId(trackIdx_));
        bindParamLock(soloBtn_, *apvts_, openfruit::params::trackSoloId(trackIdx_));
    }
    muteBtn_.setTooltip("Mute this track");
    muteBtn_.setTitle("Mute");
    addAndMakeVisible(muteBtn_);

    soloBtn_.onClick = [this] {
        const bool s = soloBtn_.getToggleState();
        engine_.getTrack(trackIdx_).solo = s;
        setApvtsParamValue(apvts_, openfruit::params::trackSoloId(trackIdx_), s ? 1.0f : 0.0f);
    };
    soloBtn_.setTooltip("Solo this track");
    soloBtn_.setTitle("Solo");
    addAndMakeVisible(soloBtn_);

    setTooltip("Right-click the track number to copy, paste, clear or shift this row.");
    voiceSettingsBtn_.setButtonText("Edit");
    voiceSettingsBtn_.setTitle("Edit");
    voiceSettingsBtn_.setTooltip("Edit tuning, level, MIDI, timing, filter and amplitude envelope");
    voiceSettingsBtn_.onClick = [this] { if (onOpenSettings) onOpenSettings(trackIdx_); };
    loadBtn_.setTooltip("Choose a sample for this track");
    addAndMakeVisible(voiceSettingsBtn_);

    // 8. 16 Step Pads
    for (int s = 0; s < openfruit::core::kStepsPerPage; ++s) {
        stepPads_[static_cast<size_t>(s)] = std::make_unique<CyberStepPad>(trackIdx_, s);
        auto* pad = stepPads_[static_cast<size_t>(s)].get();

        pad->onToggle = [this, s](bool active) {
            engine_.setStepActive(trackIdx_, page_, s, active);
        };
        pad->onTuneChange = [this, s](int tune) {
            engine_.setStepTune(trackIdx_, page_, s, tune);
        };
        pad->onVelocityChange = [this, s](float vel) {
            engine_.setStepVelocity(trackIdx_, page_, s, vel);
        };
        pad->onStepDataChange = [this, s](openfruit::core::StepData data) {
            engine_.setStepData(trackIdx_, page_, s, data);
        };
        pad->onPaint = [this](int x, bool active) {
            const int step = std::clamp((x - kLaneWidth) * core::kStepsPerPage / std::max(1, getWidth() - kLaneWidth),
                                        0, core::kStepsPerPage - 1);
            // Fill the steps a fast drag skipped.
            for (int index = std::min(paintStep_, step); index <= std::max(paintStep_, step); ++index)
                engine_.setStepActive(trackIdx_, editPage_, index, active);
            paintStep_ = step;
            updateFromEngine();
        };
        // One gesture on the row, on any of its pads, is one undo step.
        pad->onEditBegan = [this, s] {
            paintStep_ = s;
            if (editOpen_) return;
            editOpen_ = true;
            editPage_ = page_;
            editBefore_ = plugin::copyRows(engine_, editPage_, trackIdx_, 1).front();
        };
        pad->onEditEnded = [this] {
            if (!editOpen_) return;
            editOpen_ = false;
            const auto after = plugin::copyRows(engine_, editPage_, trackIdx_, 1).front();
            std::vector<plugin::StepChange> changes;
            for (int step = 0; step < core::kStepsPerPage; ++step)
                if (after[static_cast<size_t>(step)] != editBefore_[static_cast<size_t>(step)])
                    changes.push_back({ trackIdx_, editPage_, step, editBefore_[static_cast<size_t>(step)], after[static_cast<size_t>(step)] });
            if (!changes.empty() && onStepsEdited) onStepsEdited(std::move(changes));
        };

        addAndMakeVisible(pad);
    }
}

void TrackRowComponent::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    soloBtn_.setColour(juce::TextButton::buttonOnColourId, palette.accentSecondary);
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
    const bool muted = apvts_ != nullptr
        ? apvts_->getRawParameterValue(openfruit::params::trackMuteId(trackIdx_))->load() > 0.5f
        : static_cast<bool>(track.mute);
    const bool soloed = apvts_ != nullptr
        ? apvts_->getRawParameterValue(openfruit::params::trackSoloId(trackIdx_))->load() > 0.5f
        : static_cast<bool>(track.solo);
    engine_.getTrack(trackIdx_).mute = muted;
    engine_.getTrack(trackIdx_).solo = soloed;
    muteBtn_.setToggleState(muted, juce::dontSendNotification);
    soloBtn_.setToggleState(soloed, juce::dontSendNotification);

    // Update 16 pads for active page
    for (int s = 0; s < openfruit::core::kStepsPerPage; ++s) {
        stepPads_[static_cast<size_t>(s)]->setStepData(engine_.getStep(trackIdx_, page_, s));
    }
}

void TrackRowComponent::setAnySolo(bool anySolo) {
    juce::String note;
    if (anySolo && !soloBtn_.getToggleState())
        note = "\nSilent while another track is soloed.";
    if (muteBtn_.getToggleState())
        note = "\nSilent: unmute this track to hear it.";
    for (auto& pad : stepPads_)
        pad->setSilencedNote(note);
    levelSlider_.setAlpha(note.isEmpty() ? 1.0f : 0.4f);
    panSlider_.setAlpha(note.isEmpty() ? 1.0f : 0.4f);
}

void TrackRowComponent::setMixView(bool mix) {
    for (auto& pad : stepPads_)
        pad->setVisible(!mix);
    levelSlider_.setVisible(mix);
    panSlider_.setVisible(mix);
}

void TrackRowComponent::updatePlayhead(int currentStep, bool live) {
    for (int s = 0; s < openfruit::core::kStepsPerPage; ++s) {
        stepPads_[static_cast<size_t>(s)]->setIsCurrentPlayhead(s == currentStep, live);
    }
}

void TrackRowComponent::paint(juce::Graphics& g) {
    g.fillAll(trackIdx_ % 2 == 0 ? palette_.panel : palette_.background);
    g.setColour(palette_.textDim);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(juce::String(trackIdx_+1).paddedLeft('0',2), 1, 0, 29, getHeight(), juce::Justification::centred);
}

void TrackRowComponent::paintOverChildren(juce::Graphics& g) {
    if (!fileDragHover_) return;
    g.setColour(palette_.accentPrimary);
    g.drawRect(getLocalBounds().toFloat().reduced(1.0f), 2.0f);
}

void TrackRowComponent::mouseDown(const juce::MouseEvent& e) {
    if (e.x < 31 && e.mods.isPopupMenu() && onRowMenu)
        onRowMenu(*this, page_, trackIdx_);
}

bool TrackRowComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& path : files) {
        if (openfruit::core::WavReader::isSupportedAudioFile(path.toStdString())) {
            return true;
        }
    }
    return false;
}

void TrackRowComponent::fileDragEnter(const juce::StringArray&, int, int) {
    fileDragHover_ = true;
    repaint();
}

void TrackRowComponent::fileDragExit(const juce::StringArray&) {
    fileDragHover_ = false;
    repaint();
}

void TrackRowComponent::filesDropped(const juce::StringArray& files, int, int) {
    fileDragHover_ = false;
    repaint();
    for (const auto& path : files) {
        if (!openfruit::core::WavReader::isSupportedAudioFile(path.toStdString())) {
            continue;
        }
        if (auto sample = openfruit::core::WavReader::loadSample(path.toStdString())) {
            engine_.loadSampleForTrack(trackIdx_, std::move(sample));
            updateFromEngine();
            if (onSampleLoaded) onSampleLoaded();
        }
        return;
    }
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
    auto mix = area.withSizeKeepingCentre(area.getWidth(), 36);
    levelSlider_.setBounds(mix.removeFromLeft(mix.getWidth() / 2).reduced(12, 0));
    panSlider_.setBounds(mix.reduced(12, 0));
}

} // namespace openfruit::ui
