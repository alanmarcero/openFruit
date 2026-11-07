#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace openfruit::ui {

class CyberLookAndFeel : public juce::LookAndFeel_V4 {
public:
    CyberLookAndFeel();
    ~CyberLookAndFeel() override = default;

    void setPalette(const openfruit::theme::Palette& palette);
    [[nodiscard]] const openfruit::theme::Palette& getPalette() const noexcept { return palette_; }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool isHighlighted, bool isDown) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    void drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header) override;
    void drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& header,
                               const juce::String& columnName, int columnId,
                               int width, int height, bool isMouseOver, bool isMouseDown,
                               int columnFlags) override;

private:
    openfruit::theme::Palette palette_;
};

} // namespace openfruit::ui
