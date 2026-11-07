#include "CyberLookAndFeel.h"

namespace openfruit::ui {

CyberLookAndFeel::CyberLookAndFeel()
    : palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    setColour(juce::ResizableWindow::backgroundColourId, palette_.background);
    setColour(juce::PopupMenu::backgroundColourId, palette_.panel.darker(0.2f));
    setColour(juce::PopupMenu::textColourId, palette_.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette_.accentPrimary.withAlpha(0.35f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void CyberLookAndFeel::setPalette(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    setColour(juce::ResizableWindow::backgroundColourId, palette_.background);
    setColour(juce::PopupMenu::backgroundColourId, palette_.panel.darker(0.2f));
    setColour(juce::PopupMenu::textColourId, palette_.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette_.accentPrimary.withAlpha(0.35f));
}

void CyberLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool isHighlighted,
                                            bool isDown) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isToggleOn = button.getToggleState();

    juce::Colour base = backgroundColour;
    if (isToggleOn) {
        base = palette_.accentPrimary;
    } else if (isDown) {
        base = base.darker(0.35f);
    } else if (isHighlighted) {
        base = base.brighter(0.18f);
    }

    // Outer glow on active / toggle state
    if (isToggleOn) {
        g.setColour(palette_.accentPrimary.withAlpha(0.40f));
        g.drawRoundedRectangle(bounds.expanded(1.0f), 4.5f, 2.0f);
    }

    // Outer edge / bezel
    const juce::Colour edgeCol = isToggleOn
        ? palette_.accentSecondary
        : (isHighlighted ? palette_.accentSecondary.withAlpha(0.6f) : palette_.panelEdge.brighter(0.1f));
    g.setColour(edgeCol);
    g.drawRoundedRectangle(bounds, 3.5f, isToggleOn ? 1.5f : 1.0f);

    // Inner face gradient
    const auto inner = bounds.reduced(1.0f);
    juce::ColourGradient grad(
        base.brighter(0.20f), inner.getX(), inner.getY(),
        base.darker(0.35f), inner.getX(), inner.getBottom(),
        false
    );
    g.setGradientFill(grad);
    g.fillRoundedRectangle(inner, 3.0f);

    // Top specular highlight
    g.setColour(juce::Colours::white.withAlpha(isToggleOn ? 0.35f : 0.12f));
    g.fillRoundedRectangle(inner.getX() + 2.0f, inner.getY() + 1.0f, inner.getWidth() - 4.0f, 1.8f, 0.9f);

    // Subtle bottom inner drop-shadow
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawLine(inner.getX() + 2.0f, inner.getBottom() - 1.0f, inner.getRight() - 2.0f, inner.getBottom() - 1.0f, 1.0f);
}

void CyberLookAndFeel::drawButtonText(juce::Graphics& g,
                                      juce::TextButton& button,
                                      bool /*isHighlighted*/,
                                      bool /*isDown*/) {
    const auto bounds = button.getLocalBounds();
    const bool isToggle = button.getToggleState();

    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));

    // Subtle text shadow for crisp legibility
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawText(button.getButtonText(), bounds.translated(0, 1), juce::Justification::centred, true);

    const juce::Colour textCol = isToggle
        ? juce::Colours::white
        : button.findColour(juce::TextButton::textColourOffId);
    g.setColour(textCol);
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred, true);
}

void CyberLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool isHighlighted,
                                        bool isDown) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
    const juce::String text = button.getButtonText();
    const bool isOn = button.getToggleState();

    if (text == "M" || text == "S") {
        // Hardware LED Push-Switch for Mute and Solo
        const bool isMute = (text == "M");
        const juce::Colour activeColour = isMute
            ? juce::Colour(0xffff2d55)   // High-intensity Crimson for Mute
            : palette_.accentSecondary; // Electric Cyan/Lime for Solo

        // Outer socket well
        g.setColour(palette_.background.darker(0.7f));
        g.fillRoundedRectangle(bounds, 3.5f);
        g.setColour(palette_.panelEdge.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds, 3.5f, 1.0f);

        const auto inner = bounds.reduced(1.5f);

        if (isOn) {
            // Glowing illuminated state
            g.setColour(activeColour.withAlpha(0.45f));
            g.drawRoundedRectangle(bounds.expanded(1.0f), 4.0f, 1.8f);

            juce::ColourGradient ledGrad(
                activeColour.brighter(0.35f), inner.getX(), inner.getY(),
                activeColour.darker(0.25f), inner.getX(), inner.getBottom(),
                false
            );
            g.setGradientFill(ledGrad);
            g.fillRoundedRectangle(inner, 2.5f);

            // Specular top glint
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillRoundedRectangle(inner.getX() + 2.0f, inner.getY() + 1.0f, inner.getWidth() - 4.0f, 1.5f, 0.75f);

            // Illuminated white text
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.drawText(text, inner.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, false);
            g.setColour(juce::Colours::white);
            g.drawText(text, inner.toNearestInt(), juce::Justification::centred, false);
        } else {
            // Inactive recessed tactile switch
            const juce::Colour inactiveBase = palette_.panel.darker(isDown ? 0.4f : (isHighlighted ? 0.05f : 0.2f));
            juce::ColourGradient offGrad(
                inactiveBase.brighter(0.12f), inner.getX(), inner.getY(),
                inactiveBase.darker(0.20f), inner.getX(), inner.getBottom(),
                false
            );
            g.setGradientFill(offGrad);
            g.fillRoundedRectangle(inner, 2.5f);

            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.setColour(palette_.textDim.withAlpha(isHighlighted ? 0.9f : 0.6f));
            g.drawText(text, inner.toNearestInt(), juce::Justification::centred, false);
        }
        return;
    }

    // Default toggle (e.g. Auto Play in modal): cyber switch pill
    const float pillHeight = std::min(18.0f, bounds.getHeight() - 4.0f);
    const float pillWidth = 32.0f;
    const auto pillRect = juce::Rectangle<float>(bounds.getX() + 2.0f, bounds.getCentreY() - pillHeight * 0.5f, pillWidth, pillHeight);

    g.setColour(isOn ? palette_.accentPrimary.withAlpha(0.4f) : palette_.panel.darker(0.3f));
    g.fillRoundedRectangle(pillRect, pillHeight * 0.5f);
    g.setColour(isOn ? palette_.accentPrimary : palette_.panelEdge);
    g.drawRoundedRectangle(pillRect, pillHeight * 0.5f, 1.0f);

    const float knobRadius = pillHeight * 0.5f - 2.0f;
    const float knobX = isOn ? (pillRect.getRight() - knobRadius * 2.0f - 2.0f) : (pillRect.getX() + 2.0f);
    const auto knobRect = juce::Rectangle<float>(knobX, pillRect.getCentreY() - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour(isOn ? palette_.accentSecondary : palette_.textDim);
    g.fillEllipse(knobRect);

    if (text.isNotEmpty()) {
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(palette_.text);
        const auto textRect = bounds.withTrimmedLeft(pillWidth + 8.0f);
        g.drawText(text, textRect.toNearestInt(), juce::Justification::centredLeft, true);
    }
}

void CyberLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPos,
                                        float /*minSliderPos*/,
                                        float /*maxSliderPos*/,
                                        juce::Slider::SliderStyle /*style*/,
                                        juce::Slider& slider) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);

    // Recessed dark groove with bevel
    g.setColour(palette_.background.darker(0.7f));
    g.fillRoundedRectangle(bounds, 3.5f);
    g.setColour(palette_.panelEdge.withAlpha(0.75f));
    g.drawRoundedRectangle(bounds, 3.5f, 1.0f);

    const double minVal = slider.getMinimum();
    const double maxVal = slider.getMaximum();
    const bool isBipolar = (minVal < 0.0 && maxVal > 0.0);

    const auto inner = bounds.reduced(1.5f);

    if (isBipolar) {
        // Center-zero bar (e.g. Pan, Tune)
        const float midX = inner.getCentreX();
        const float currentX = std::clamp(sliderPos, inner.getX(), inner.getRight());

        // Center zero notch
        g.setColour(palette_.textDim.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(midX), inner.getY() + 1.0f, inner.getBottom() - 1.0f);

        if (std::abs(currentX - midX) > 1.0f) {
            const float barLeft = std::min(midX, currentX);
            const float barWidth = std::abs(currentX - midX);
            const auto fillRect = juce::Rectangle<float>(barLeft, inner.getY() + 1.0f, barWidth, inner.getHeight() - 2.0f);

            const juce::Colour fillColor = (currentX >= midX) ? palette_.accentSecondary : palette_.midiChannelBadge;
            juce::ColourGradient fillGrad(
                fillColor.withAlpha(0.55f), midX, inner.getY(),
                fillColor.withAlpha(0.90f), currentX, inner.getY(),
                false
            );
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillRect, 2.0f);
        }
    } else {
        // Unipolar bar (e.g. Volume)
        const float currentX = std::clamp(sliderPos, inner.getX(), inner.getRight());
        const float fillWidth = currentX - inner.getX();
        if (fillWidth > 1.0f) {
            const auto fillRect = juce::Rectangle<float>(inner.getX(), inner.getY() + 1.0f, fillWidth, inner.getHeight() - 2.0f);
            juce::ColourGradient grad(
                palette_.accentSecondary.withAlpha(0.70f), inner.getX(), inner.getY(),
                palette_.accentPrimary.withAlpha(0.90f), currentX, inner.getY(),
                false
            );
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillRect, 2.0f);
        }
    }

    // Illuminated thumb marker needle
    const float thumbX = std::clamp(sliderPos - 1.5f, inner.getX(), inner.getRight() - 3.0f);
    g.setColour(palette_.accentSecondary.withAlpha(0.40f));
    g.drawRoundedRectangle(thumbX - 1.0f, inner.getY(), 5.0f, inner.getHeight(), 1.5f, 1.0f);
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(thumbX, inner.getY(), 3.0f, inner.getHeight(), 1.2f);

    // Value text overlay with drop shadow
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    const juce::String text = slider.getTextFromValue(slider.getValue());
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawText(text, bounds.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, false);
    g.setColour(palette_.text);
    g.drawText(text, bounds.toNearestInt(), juce::Justification::centred, false);
}

void CyberLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                    juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    const bool isHover = box.isMouseOver();

    // Outer glow on hover
    if (isHover) {
        g.setColour(palette_.accentSecondary.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds.expanded(1.0f), 4.5f, 1.5f);
    }

    // Background gradient
    const juce::Colour base = isButtonDown ? palette_.panel.darker(0.35f) : palette_.panel;
    juce::ColourGradient grad(
        base.brighter(isHover ? 0.20f : 0.08f), bounds.getX(), bounds.getY(),
        base.darker(0.25f), bounds.getX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 3.5f);

    // Bezel border
    g.setColour(isHover ? palette_.accentSecondary : palette_.panelEdge.brighter(0.1f));
    g.drawRoundedRectangle(bounds, 3.5f, 1.0f);

    // Top specular line
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(bounds.getX() + 2.0f, bounds.getY() + 1.0f, bounds.getWidth() - 4.0f, 1.5f, 0.75f);

    // Cyan drop arrow
    const float arrowX = static_cast<float>(width) - 15.0f;
    const float arrowY = static_cast<float>(height) * 0.5f - 2.5f;
    juce::Path arrow;
    arrow.addTriangle(arrowX, arrowY, arrowX + 8.0f, arrowY, arrowX + 4.0f, arrowY + 5.0f);
    g.setColour(palette_.accentSecondary);
    g.fillPath(arrow);
}

void CyberLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(4, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, palette_.text);
}

void CyberLookAndFeel::drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header) {
    const auto bounds = header.getLocalBounds().toFloat();
    g.setColour(palette_.panel.darker(0.3f));
    g.fillRect(bounds);
    g.setColour(palette_.accentSecondary.withAlpha(0.5f));
    g.drawLine(0.0f, bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);
}

void CyberLookAndFeel::drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& /*header*/,
                                             const juce::String& columnName, int /*columnId*/,
                                             int width, int height, bool isMouseOver, bool /*isMouseDown*/,
                                             int /*columnFlags*/) {
    const auto bounds = juce::Rectangle<int>(0, 0, width, height);
    if (isMouseOver) {
        g.setColour(palette_.accentSecondary.withAlpha(0.12f));
        g.fillRect(bounds);
    }
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(palette_.accentSecondary);
    g.drawText(columnName, bounds.reduced(4, 0), juce::Justification::centredLeft, true);

    g.setColour(palette_.panelEdge.withAlpha(0.5f));
    g.drawVerticalLine(width - 1, 3.0f, static_cast<float>(height - 3));
}

} // namespace openfruit::ui
