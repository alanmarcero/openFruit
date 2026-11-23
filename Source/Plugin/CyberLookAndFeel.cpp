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
    setColour(juce::TextButton::buttonOnColourId, palette.accentPrimary);
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
    const float alpha = button.isEnabled() ? 1.0f : 0.4f;
    const auto fill = button.getToggleState() ? button.findColour(juce::TextButton::buttonOnColourId) : colour.brighter(down ? .18f : .07f);
    g.setColour(fill.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(rect, 5);
    const auto activeEdge = button.getToggleState() ? fill.brighter(0.25f) : palette_.panelEdge;
    const auto edge = hover || button.hasKeyboardFocus(false) ? palette_.accentSecondary : activeEdge;
    g.setColour(edge.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(rect, 5, 1);
}

void CyberLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.setColour((button.getToggleState() ? palette_.background : palette_.text).withMultipliedAlpha(button.isEnabled() ? 1.0f : .45f));
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(4,0), juce::Justification::centred, true);
}

void CyberLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool hover, bool down) {
    drawButtonBackground(g,button,palette_.panel,hover,down);
    const auto text = button.getToggleState() ? palette_.background : palette_.text;
    g.setColour(text.withMultipliedAlpha(button.isEnabled() ? 1.0f : .45f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(4,0), juce::Justification::centred, true);
}

void CyberLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    const float alpha = slider.isEnabled() ? 1.0f : 0.4f;
    if (style == juce::Slider::LinearBar) {
        const auto bar = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(0.5f);
        g.setColour(palette_.background.withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(bar, 5);
        g.setColour(palette_.accentPrimary.withAlpha(0.3f * alpha));
        g.fillRoundedRectangle(bar.withRight(juce::jlimit(bar.getX(), bar.getRight(), pos)), 5);
        g.setColour(palette_.panelEdge.withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(bar, 5, 1);
        return;
    }
    const float middle = static_cast<float>(y) + 0.5f * static_cast<float>(height);
    const auto track = juce::Rectangle<float>(static_cast<float>(x), middle - 2.5f, static_cast<float>(width), 5.0f);
    g.setColour(palette_.panelEdge.brighter(0.25f).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(track, 2.5f);
    g.setColour(palette_.accentPrimary.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(track.withRight(juce::jlimit(track.getX(), track.getRight(), pos)), 2.5f);
    const auto thumb = slider.hasKeyboardFocus(true) ? palette_.accentSecondary : palette_.text;
    g.setColour(palette_.background.withMultipliedAlpha(alpha));
    g.fillEllipse(pos - 6.5f, middle - 6.5f, 13.0f, 13.0f);
    g.setColour(thumb.withMultipliedAlpha(alpha));
    g.fillEllipse(pos - 5.5f, middle - 5.5f, 11.0f, 11.0f);
}

juce::Label* CyberLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    struct SliderTextBoxLabel final : public juce::Label {
        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isPopupMenu())
                return;
            juce::Label::mouseDown(e);
        }
        void mouseUp(const juce::MouseEvent& e) override {
            if (e.mods.isPopupMenu())
                return;
            juce::Label::mouseUp(e);
        }
    };
    auto* label = new SliderTextBoxLabel();
    label->setJustificationType(juce::Justification::centred);
    label->setKeyboardType(juce::TextInputTarget::decimalKeyboard);
    label->setColour(juce::Label::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    label->setColour(juce::Label::backgroundColourId, slider.findColour(juce::Slider::textBoxBackgroundColourId));
    label->setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    label->setColour(juce::TextEditor::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    label->setColour(juce::TextEditor::backgroundColourId, slider.findColour(juce::Slider::textBoxBackgroundColourId));
    label->setColour(juce::TextEditor::highlightColourId, slider.findColour(juce::Slider::textBoxHighlightColourId));
    return label;
}

void CyberLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label) {
    const auto bounds = label.getLocalBounds().toFloat();
    const bool isSliderBox = dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr;
    const auto bg = label.findColour(juce::Label::backgroundColourId);
    if (!bg.isTransparent()) {
        g.setColour(bg);
        if (isSliderBox)
            g.fillRoundedRectangle(bounds.reduced(0.5f), 5.0f);
        else
            g.fillRect(bounds);
    }
    if (!label.isBeingEdited()) {
        const float alpha = label.isEnabled() ? 1.0f : 0.45f;
        const auto font = getLabelFont(label);
        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);
        const auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                         label.getMinimumHorizontalScale());
        g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
    } else if (label.isEnabled()) {
        g.setColour(label.findColour(juce::Label::outlineColourId));
    }
    const auto outline = label.findColour(juce::Label::outlineColourId);
    if (!outline.isTransparent()) {
        if (isSliderBox)
            g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
        else
            g.drawRect(label.getLocalBounds());
    }
}

void CyberLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    auto rect = box.getLocalBounds().toFloat().reduced(1);
    const float alpha = box.isEnabled() ? 1.0f : 0.4f;
    g.setColour(palette_.panel.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(rect,5);
    const auto edge = box.hasKeyboardFocus(true) || box.isMouseOver() ? palette_.accentSecondary : palette_.panelEdge;
    g.setColour(edge.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(rect,5,1);
    const float x = static_cast<float>(width)-14, y = static_cast<float>(height)/2;
    juce::Path arrow;
    arrow.startNewSubPath(x-4,y-2); arrow.lineTo(x,y+2); arrow.lineTo(x+4,y-2);
    g.setColour(palette_.textDim.withMultipliedAlpha(alpha));
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
                                             int columnFlags) {
    const auto bounds = juce::Rectangle<int>(0, 0, width, height);
    if (isMouseOver) {
        g.setColour(palette_.accentSecondary.withAlpha(0.12f));
        g.fillRect(bounds);
    }
    const bool sortedForwards = (columnFlags & juce::TableHeaderComponent::sortedForwards) != 0;
    const bool sortedBackwards = (columnFlags & juce::TableHeaderComponent::sortedBackwards) != 0;
    const bool isSorted = sortedForwards || sortedBackwards;

    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.setColour(palette_.accentSecondary);
    g.drawText(columnName, bounds.reduced(8, 0).withTrimmedRight(isSorted ? 16 : 0),
               juce::Justification::centredLeft, true);

    if (isSorted) {
        const float cx = static_cast<float>(width) - 12.0f;
        const float cy = static_cast<float>(height) * 0.5f;
        const float dy = sortedForwards ? 2.0f : -2.0f;
        juce::Path chevron;
        chevron.startNewSubPath(cx - 4.0f, cy + dy);
        chevron.lineTo(cx, cy - dy);
        chevron.lineTo(cx + 4.0f, cy + dy);
        g.strokePath(chevron, juce::PathStrokeType(1.6f));
    }

    g.setColour(palette_.panelEdge.withAlpha(0.5f));
    g.drawVerticalLine(width - 1, 3.0f, static_cast<float>(height - 3));
}

} // namespace openfruit::ui
