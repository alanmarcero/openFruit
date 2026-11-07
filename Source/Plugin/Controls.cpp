#include "Controls.h"
#include <iomanip>
#include <sstream>

namespace openfruit::ui {

CyberStepPad::CyberStepPad(int trackIdx, int stepIdx)
    : trackIdx_(trackIdx), stepIdx_(stepIdx), palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    setRepaintsOnMouseActivity(true);
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
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const int beatGroup = (stepIdx_ / 4) % 2;

    // Recessed dark socket background
    g.setColour(palette_.background.darker(0.6f));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(palette_.panelEdge.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    const auto padBounds = bounds.reduced(1.8f);

    if (data_.active) {
        // Active Step: Vibrant glowing neon button
        juce::Colour activeColor = palette_.padActive.interpolatedWith(juce::Colours::white, (data_.velocity - 0.5f) * 0.25f);

        // Outer glow
        g.setColour(palette_.padGlow);
        g.drawRoundedRectangle(bounds, 4.0f, 2.0f);

        // 3D Pad Gradient
        juce::ColourGradient grad(
            activeColor.brighter(0.3f), padBounds.getX(), padBounds.getY(),
            activeColor.darker(0.35f), padBounds.getX(), padBounds.getBottom(),
            false
        );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(padBounds, 3.0f);

        // Top specular highlight
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(padBounds.getX() + 2.0f, padBounds.getY() + 1.0f, padBounds.getWidth() - 4.0f, 2.5f, 1.0f);

        // Velocity indicator bar at bottom
        const float barHeight = 3.0f;
        const float barWidth = (padBounds.getWidth() - 6.0f) * std::clamp(data_.velocity, 0.1f, 1.0f);
        const auto barRect = juce::Rectangle<float>(
            padBounds.getX() + 3.0f,
            padBounds.getBottom() - barHeight - 2.5f,
            barWidth,
            barHeight
        );
        g.setColour(palette_.accentSecondary);
        g.fillRoundedRectangle(barRect, 1.5f);

        // Semitone tune offset badge (if tuned)
        if (data_.semitoneOffset != 0) {
            const std::string badge = (data_.semitoneOffset > 0 ? "+" : "") + std::to_string(data_.semitoneOffset);
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawText(badge, padBounds.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, false);
            g.setColour(juce::Colours::white);
            g.drawText(badge, padBounds.toNearestInt(), juce::Justification::centred, false);
        }
    } else {
        // Inactive Step: Tactile hardware pad with 4-beat alternating shading
        juce::Colour baseColor = (beatGroup == 0) ? palette_.padInactiveBeatA : palette_.padInactiveBeatB;

        juce::ColourGradient grad(
            baseColor.brighter(0.18f), padBounds.getX(), padBounds.getY(),
            baseColor.darker(0.25f), padBounds.getX(), padBounds.getBottom(),
            false
        );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(padBounds, 3.0f);

        // Subtle top specular edge
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(padBounds.getX() + 2.0f, padBounds.getY() + 1.0f, padBounds.getWidth() - 4.0f, 1.5f, 0.75f);
    }

    // Playhead glowing aura
    if (isPlayhead_) {
        g.setColour(palette_.playheadGlow);
        g.drawRoundedRectangle(bounds.expanded(1.0f), 4.5f, 2.5f);
    }
}

void CyberStepPad::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        // Right click resets tune or toggles tune
        dragStartPos_ = e.getPosition();
        return;
    }

    // Left click toggles step
    data_.active = !data_.active;
    repaint();
    if (onToggle) {
        onToggle(data_.active);
    }
}

void CyberStepPad::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        // Drag vertically to adjust semitone tune (-24 .. +24)
        const int deltaY = dragStartPos_.y - e.y;
        const int semitones = std::clamp(deltaY / 4, -24, 24);
        if (semitones != data_.semitoneOffset) {
            data_.semitoneOffset = semitones;
            repaint();
            if (onTuneChange) {
                onTuneChange(semitones);
            }
        }
    } else if (e.mods.isLeftButtonDown() && data_.active) {
        // Drag vertically while active to adjust velocity
        const float delta = static_cast<float>(dragStartPos_.y - e.y) / 60.0f;
        const float newVel = std::clamp(data_.velocity + delta * 0.05f, 0.05f, 1.0f);
        if (std::abs(newVel - data_.velocity) > 0.02f) {
            data_.velocity = newVel;
            repaint();
            if (onVelocityChange) {
                onVelocityChange(newVel);
            }
        }
    }
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

TrackRowComponent::TrackRowComponent(int trackIdx, openfruit::core::FruitEngine& engine)
    : trackIdx_(trackIdx), engine_(engine), palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

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

    // 3. Timing Selector (1/16, 1/32, 1/64)
    timingBox_.addItem("1/16", 1);
    timingBox_.addItem("1/32", 2);
    timingBox_.addItem("1/64", 3);
    timingBox_.setSelectedId(1, juce::dontSendNotification);
    timingBox_.onChange = [this] {
        const int id = timingBox_.getSelectedId();
        auto& t = engine_.getTrack(trackIdx_);
        if (id == 1) t.timing = openfruit::core::StepTiming::Timing1_16;
        else if (id == 2) t.timing = openfruit::core::StepTiming::Timing1_32;
        else if (id == 3) t.timing = openfruit::core::StepTiming::Timing1_64;
    };
    addAndMakeVisible(timingBox_);

    // 4. Base Tune Slider (-24 .. +24)
    tuneSlider_.setSliderStyle(juce::Slider::LinearBar);
    tuneSlider_.setRange(-24.0, 24.0, 1.0);
    tuneSlider_.setValue(0.0, juce::dontSendNotification);
    tuneSlider_.setTextValueSuffix(" st");
    tuneSlider_.onValueChange = [this] {
        engine_.getTrack(trackIdx_).baseTune = static_cast<int>(tuneSlider_.getValue());
    };
    addAndMakeVisible(tuneSlider_);

    // 5. Volume Slider
    volSlider_.setSliderStyle(juce::Slider::LinearBar);
    volSlider_.setRange(0.0, 1.5, 0.01);
    volSlider_.setValue(1.0, juce::dontSendNotification);
    volSlider_.onValueChange = [this] {
        engine_.getTrack(trackIdx_).volume = static_cast<float>(volSlider_.getValue());
    };
    addAndMakeVisible(volSlider_);

    // 6. Pan Slider
    panSlider_.setSliderStyle(juce::Slider::LinearBar);
    panSlider_.setRange(-1.0, 1.0, 0.01);
    panSlider_.setValue(0.0, juce::dontSendNotification);
    panSlider_.onValueChange = [this] {
        engine_.getTrack(trackIdx_).pan = static_cast<float>(panSlider_.getValue());
    };
    addAndMakeVisible(panSlider_);

    // 7. Mute & Solo
    muteBtn_.onClick = [this] {
        engine_.getTrack(trackIdx_).mute = muteBtn_.getToggleState();
    };
    addAndMakeVisible(muteBtn_);

    soloBtn_.onClick = [this] {
        engine_.getTrack(trackIdx_).solo = soloBtn_.getToggleState();
    };
    addAndMakeVisible(soloBtn_);

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

    // Update track sample button text with sample name and estimated tone
    std::string btnText = track.name;
    if (track.sample) {
        btnText = track.sample->sampleName;
        if (!track.sample->toneResult.noteName.empty() && track.sample->toneResult.noteName != "---") {
            btnText += " [" + track.sample->toneResult.formattedTone() + "]";
        }
    }
    samplePadBtn_.setButtonText(btnText);

    // Update timing
    if (track.timing == openfruit::core::StepTiming::Timing1_16) timingBox_.setSelectedId(1, juce::dontSendNotification);
    else if (track.timing == openfruit::core::StepTiming::Timing1_32) timingBox_.setSelectedId(2, juce::dontSendNotification);
    else if (track.timing == openfruit::core::StepTiming::Timing1_64) timingBox_.setSelectedId(3, juce::dontSendNotification);

    tuneSlider_.setValue(track.baseTune, juce::dontSendNotification);
    volSlider_.setValue(track.volume, juce::dontSendNotification);
    panSlider_.setValue(track.pan, juce::dontSendNotification);
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
    const auto bounds = getLocalBounds().toFloat();

    // Subtle row background and separator line
    g.setColour(palette_.panel.withAlpha(0.6f));
    g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);

    // MIDI Channel Badge (Track index 0..15 -> MIDI Ch 1..16)
    const auto badgeRect = juce::Rectangle<float>(4.0f, 4.0f, 44.0f, bounds.getHeight() - 8.0f);
    g.setColour(palette_.midiChannelBadge);
    g.fillRoundedRectangle(badgeRect, 3.0f);

    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    const std::string chText = "CH " + std::to_string(trackIdx_ + 1);
    g.drawText(chText, badgeRect.toNearestInt(), juce::Justification::centred, false);

    // Separator line at bottom
    g.setColour(palette_.panelEdge.withAlpha(0.5f));
    g.drawLine(0.0f, bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);
}

void TrackRowComponent::resized() {
    auto area = getLocalBounds().reduced(2);

    // 1. Skip MIDI Badge (width 48)
    area.removeFromLeft(50);

    // 2. Track Sample Trigger Pad
    samplePadBtn_.setBounds(area.removeFromLeft(140).reduced(2));

    // 3. Load Button
    loadBtn_.setBounds(area.removeFromLeft(44).reduced(2));

    // 4. Timing Dropdown (1/16, 1/32, 1/64)
    timingBox_.setBounds(area.removeFromLeft(54).reduced(2));

    // 5. Tune Slider (-24 .. +24)
    tuneSlider_.setBounds(area.removeFromLeft(50).reduced(2));

    // 6. Vol & Pan Sliders
    volSlider_.setBounds(area.removeFromLeft(44).reduced(2));
    panSlider_.setBounds(area.removeFromLeft(40).reduced(2));

    // 7. Mute & Solo
    muteBtn_.setBounds(area.removeFromLeft(24).reduced(2));
    soloBtn_.setBounds(area.removeFromLeft(24).reduced(2));

    // Spacer
    area.removeFromLeft(8);

    // 8. 16 Step Pads
    const int numPads = openfruit::core::kStepsPerPage;
    const float padWidth = static_cast<float>(area.getWidth()) / static_cast<float>(numPads);

    for (int s = 0; s < numPads; ++s) {
        const int x = area.getX() + static_cast<int>(static_cast<float>(s) * padWidth);
        const int nextX = area.getX() + static_cast<int>(static_cast<float>(s + 1) * padWidth);
        stepPads_[static_cast<size_t>(s)]->setBounds(x, area.getY() + 2, nextX - x - 2, area.getHeight() - 4);
    }
}

} // namespace openfruit::ui
