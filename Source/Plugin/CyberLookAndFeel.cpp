#include "CyberLookAndFeel.h"

namespace openfruit::ui {

CyberLookAndFeel::CyberLookAndFeel()
    : palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {
    setPalette(palette_);
}

void CyberLookAndFeel::setPalette(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    setColourScheme(getDarkColourScheme());
    setColour(juce::ResizableWindow::backgroundColourId, palette.background);
    setColour(juce::Label::textColourId, palette.text);
    setColour(juce::TextButton::buttonColourId, palette.panel);
    setColour(juce::Slider::thumbColourId, palette.accentPrimary);
    setColour(juce::Slider::textBoxTextColourId, palette.text);
    setColour(juce::Slider::textBoxBackgroundColourId, palette.background);
    setColour(juce::Slider::textBoxOutlineColourId, palette.panelEdge);
    setColour(juce::TextEditor::backgroundColourId, palette.background);
    setColour(juce::TextEditor::textColourId, palette.text);
    setColour(juce::ListBox::backgroundColourId, palette.background);
    setColour(juce::PopupMenu::backgroundColourId, palette.panel);
    setColour(juce::PopupMenu::textColourId, palette.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.accentPrimary);
    setColour(juce::PopupMenu::highlightedTextColourId, palette.background);
    setColour(juce::TooltipWindow::backgroundColourId, palette.panel);
    setColour(juce::TooltipWindow::textColourId, palette.text);
    setColour(juce::TooltipWindow::outlineColourId, palette.accentPrimary);
}

void CyberLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& colour,
                                            bool hover, bool down) {
    const auto rect = button.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(button.getToggleState() ? palette_.accentPrimary : colour.brighter(down ? .18f : .07f));
    g.fillRoundedRectangle(rect, 5);
    g.setColour(hover || button.hasKeyboardFocus(false) ? palette_.accentSecondary : palette_.panelEdge);
    g.drawRoundedRectangle(rect, 5, 1);
}

void CyberLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.setColour((button.getToggleState() ? palette_.background : palette_.text).withMultipliedAlpha(button.isEnabled() ? 1.0f : .45f));
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(4,0), juce::Justification::centred, true);
}

void CyberLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool hover, bool down) {
    drawButtonBackground(g,button,palette_.panel,hover,down);
    g.setColour(button.getToggleState() ? palette_.background : palette_.text);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(3,0), juce::Justification::centred, true);
}

void CyberLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float, float, juce::Slider::SliderStyle, juce::Slider& slider) {
    const float middle = static_cast<float>(y) + .5f*static_cast<float>(height);
    const auto track = juce::Rectangle<float>(static_cast<float>(x), middle-2, static_cast<float>(width), 4);
    g.setColour(palette_.panelEdge);
    g.fillRoundedRectangle(track, 2);
    g.setColour(palette_.accentPrimary);
    g.fillRoundedRectangle(track.withRight(juce::jlimit(track.getX(), track.getRight(), pos)), 2);
    g.setColour(slider.hasKeyboardFocus(true) ? palette_.accentSecondary : palette_.text);
    g.fillEllipse(pos-4.5f, middle-4.5f, 9,9);
}

void CyberLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    auto rect = box.getLocalBounds().toFloat().reduced(1);
    g.setColour(palette_.panel);
    g.fillRoundedRectangle(rect,5);
    g.setColour(box.hasKeyboardFocus(true) || box.isMouseOver() ? palette_.accentSecondary : palette_.panelEdge);
    g.drawRoundedRectangle(rect,5,1);
    const float x = static_cast<float>(width)-14, y = static_cast<float>(height)/2;
    juce::Path arrow;
    arrow.startNewSubPath(x-4,y-2); arrow.lineTo(x,y+2); arrow.lineTo(x+4,y-2);
    g.setColour(palette_.textDim);
    g.strokePath(arrow, juce::PathStrokeType(1.6f));
}

void CyberLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(6, 1, box.getWidth()-23, box.getHeight()-2);
    label.setColour(juce::Label::textColourId, palette_.text);
    label.setFont(juce::FontOptions(14.0f));
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
