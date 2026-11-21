#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AbCompare.h"
#include "UndoHistory.h"

namespace openfruit::suite {

struct SuiteColours {
    juce::Colour background;
    juce::Colour panel;
    juce::Colour edge;
    juce::Colour text;
    juce::Colour textDim;
    juce::Colour accent;
};

class SuiteBar : public juce::Component, private juce::ChangeListener {
public:
    SuiteBar(std::array<UndoHistory, 2>& undo, AbCompare& ab);
    ~SuiteBar() override;

    void setColours(SuiteColours colours);
    std::function<void(int slot)> onSelectSlot;
    std::function<void(int from, int to)> onCopySlot;
    std::function<void()> onShowPresets;
    std::function<void(int delta)> onStepPreset;
    void setPreset(const juce::String& name, bool edited);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refresh();
    void styleButton(juce::Button& button, const juce::String& title, const juce::String& tooltip);

    std::array<UndoHistory, 2>& undo_;
    AbCompare& ab_;
    SuiteColours colours_ {};
    juce::TextButton undoButton_ { "Undo" };
    juce::TextButton redoButton_ { "Redo" };
    juce::TextButton slotA_ { "A" };
    juce::TextButton slotB_ { "B" };
    juce::TextButton copyAToB_ { "A>B" };
    juce::TextButton copyBToA_ { "B>A" };
    juce::TextButton presetsButton_ { "Presets" };
    juce::TextButton previousPreset_ { "<" };
    juce::TextButton nextPreset_ { ">" };
    juce::Label presetName_;
};

}
