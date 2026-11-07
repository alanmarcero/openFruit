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
        base = base.darker(0.25f);
    } else if (isHighlighted) {
        base = base.brighter(0.2f);
    }

    // Outer edge / bezel
    g.setColour(isToggleOn ? palette_.accentSecondary : palette_.panelEdge.brighter(isHighlighted ? 0.25f : 0.05f));
    g.drawRoundedRectangle(bounds, 4.0f, isToggleOn ? 1.5f : 1.0f);

    // Inner face gradient
    const auto inner = bounds.reduced(1.0f);
    juce::ColourGradient grad(
        base.brighter(0.15f), inner.getX(), inner.getY(),
        base.darker(0.3f), inner.getX(), inner.getBottom(),
        false
    );
    g.setGradientFill(grad);
    g.fillRoundedRectangle(inner, 3.5f);

    // Top specular highlight
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillRoundedRectangle(inner.getX() + 2.0f, inner.getY() + 1.0f, inner.getWidth() - 4.0f, 1.5f, 0.75f);
}

void CyberLookAndFeel::drawButtonText(juce::Graphics& g,
                                      juce::TextButton& button,
                                      bool /*isHighlighted*/,
                                      bool /*isDown*/) {
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                          : juce::TextButton::textColourOffId));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
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

    // Recessed background groove
    g.setColour(palette_.background.darker(0.5f));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(palette_.panelEdge.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    const double minVal = slider.getMinimum();
    const double maxVal = slider.getMaximum();
    const bool isBipolar = (minVal < 0.0 && maxVal > 0.0);

    const auto inner = bounds.reduced(1.5f);

    if (isBipolar) {
        // Center-zero bar (e.g. Pan, Tune)
        const float midX = inner.getCentreX();
        const float currentX = std::clamp(sliderPos, inner.getX(), inner.getRight());

        g.setColour(palette_.textDim.withAlpha(0.4f));
        g.drawVerticalLine(static_cast<int>(midX), inner.getY() + 1.0f, inner.getBottom() - 1.0f);

        if (std::abs(currentX - midX) > 1.0f) {
            const float barLeft = std::min(midX, currentX);
            const float barWidth = std::abs(currentX - midX);
            const auto fillRect = juce::Rectangle<float>(barLeft, inner.getY() + 1.0f, barWidth, inner.getHeight() - 2.0f);

            const juce::Colour fillColor = (currentX >= midX) ? palette_.accentSecondary : palette_.midiChannelBadge;
            g.setColour(fillColor.withAlpha(0.75f));
            g.fillRoundedRectangle(fillRect, 1.5f);
        }
    } else {
        // Unipolar bar (e.g. Volume)
        const float currentX = std::clamp(sliderPos, inner.getX(), inner.getRight());
        const float fillWidth = currentX - inner.getX();
        if (fillWidth > 1.0f) {
            const auto fillRect = juce::Rectangle<float>(inner.getX(), inner.getY() + 1.0f, fillWidth, inner.getHeight() - 2.0f);
            juce::ColourGradient grad(
                palette_.accentSecondary.withAlpha(0.7f), inner.getX(), inner.getY(),
                palette_.accentPrimary.withAlpha(0.85f), currentX, inner.getY(),
                false
            );
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillRect, 1.5f);
        }
    }

    // Thumb line
    const float thumbX = std::clamp(sliderPos, inner.getX(), inner.getRight() - 2.0f);
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(thumbX, inner.getY(), 2.0f, inner.getHeight(), 1.0f);

    // Value text overlay
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(palette_.text.withAlpha(0.9f));
    const juce::String text = slider.getTextFromValue(slider.getValue());
    g.drawText(text, bounds.toNearestInt(), juce::Justification::centred, false);
}

void CyberLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                    juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);

    g.setColour(isButtonDown ? palette_.panel.darker(0.2f) : palette_.panel);
    g.fillRoundedRectangle(bounds, 3.5f);

    g.setColour(palette_.panelEdge.brighter(box.isMouseOver() ? 0.2f : 0.0f));
    g.drawRoundedRectangle(bounds, 3.5f, 1.0f);

    // Cyan drop arrow
    const float arrowX = static_cast<float>(width) - 14.0f;
    const float arrowY = static_cast<float>(height) * 0.5f - 2.0f;
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
