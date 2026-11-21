#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetLibrary.h"
#include "SuiteBar.h"

namespace openfruit::suite {

class PresetBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    PresetBrowser(PresetLibrary& library);
    void setColours(SuiteColours colours);
    std::function<juce::MemoryBlock()> captureState;
    std::function<void(const juce::MemoryBlock&, const juce::String& name)> restoreState;
    std::function<void(const juce::String& name)> onSaved;

    void refreshList();
    void stepPreset(const juce::String& currentName, int delta);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& event) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& event) override;
    void returnKeyPressed(int lastRow) override;
    void selectedRowsChanged(int lastRow) override;
    void timerCallback() override;
    void apply(const PresetEntry& entry);
    void showError(const juce::String& message);

    PresetLibrary& library_;
    SuiteColours colours_ {};
    std::vector<PresetEntry> entries_;
    juce::TextEditor search_;
    juce::TextEditor name_;
    juce::ListBox list_ { "Presets", this };
    juce::TextButton save_ { "Save" };
    juce::TextButton rename_ { "Rename" };
    juce::TextButton remove_ { "Delete" };
    juce::Label status_;
    juce::Button* confirming_ = nullptr;
};

}
