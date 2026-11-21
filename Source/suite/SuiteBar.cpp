#include "SuiteBar.h"

namespace openfruit::suite {

SuiteBar::SuiteBar(std::array<UndoHistory, 2>& undo, AbCompare& ab)
    : undo_(undo), ab_(ab) {
    colours_ = {
        juce::Colour(0xff0d1317),
        juce::Colour(0xff152026),
        juce::Colour(0xff283842),
        juce::Colour(0xffe6f0f5),
        juce::Colour(0xff869ca8),
        juce::Colour(0xff48f0c8)
    };
    for (auto& history : undo_)
        history.addChangeListener(this);
    ab_.addChangeListener(this);

    styleButton(undoButton_, "Undo", "Undo the last edit");
    styleButton(redoButton_, "Redo", "Redo the last undone edit");
    styleButton(slotA_, "Compare slot A", "Hear compare slot A");
    styleButton(slotB_, "Compare slot B", "Hear compare slot B");
    styleButton(copyAToB_, "Copy A to B", "Copy slot A into slot B");
    styleButton(copyBToA_, "Copy B to A", "Copy slot B into slot A");
    styleButton(presetsButton_, "Presets", "Browse factory and saved kits");
    styleButton(previousPreset_, "Previous preset", "Previous preset");
    styleButton(nextPreset_, "Next preset", "Next preset");
    presetName_.setTooltip("Loaded kit. A star marks edits since it was loaded or saved.");
    presetName_.setJustificationType(juce::Justification::centred);
    presetName_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(presetName_);

    undoButton_.onClick = [this] { undo_[static_cast<size_t>(ab_.active())].undo(); };
    redoButton_.onClick = [this] { undo_[static_cast<size_t>(ab_.active())].redo(); };
    slotA_.onClick = [this] { if (onSelectSlot) onSelectSlot(0); };
    slotB_.onClick = [this] { if (onSelectSlot) onSelectSlot(1); };
    copyAToB_.onClick = [this] { if (onCopySlot) onCopySlot(0, 1); };
    copyBToA_.onClick = [this] { if (onCopySlot) onCopySlot(1, 0); };
    presetsButton_.onClick = [this] { if (onShowPresets) onShowPresets(); };
    previousPreset_.onClick = [this] { if (onStepPreset) onStepPreset(-1); };
    nextPreset_.onClick = [this] { if (onStepPreset) onStepPreset(1); };

    for (auto* button : { &undoButton_, &redoButton_, &slotA_, &slotB_, &copyAToB_, &copyBToA_, &presetsButton_,
                          &previousPreset_, &nextPreset_ })
        addAndMakeVisible(button);
    refresh();
}

SuiteBar::~SuiteBar() {
    for (auto& history : undo_)
        history.removeChangeListener(this);
    ab_.removeChangeListener(this);
}

void SuiteBar::styleButton(juce::Button& button, const juce::String& title, const juce::String& tooltip) {
    button.setTitle(title);
    button.setTooltip(tooltip);
    button.setClickingTogglesState(false);
}

void SuiteBar::setColours(SuiteColours colours) {
    colours_ = colours;
    repaint();
}

void SuiteBar::setPreset(const juce::String& name, bool edited) {
    const auto shown = name.isEmpty() ? juce::String("Untitled") : name;
    presetName_.setText(edited ? shown + " *" : shown, juce::dontSendNotification);
}

void SuiteBar::changeListenerCallback(juce::ChangeBroadcaster*) {
    refresh();
}

void SuiteBar::refresh() {
    const auto& active = undo_[static_cast<size_t>(ab_.active())];
    undoButton_.setEnabled(active.canUndo());
    redoButton_.setEnabled(active.canRedo());
    slotA_.setToggleState(ab_.active() == 0, juce::dontSendNotification);
    slotB_.setToggleState(ab_.active() == 1, juce::dontSendNotification);
}

void SuiteBar::paint(juce::Graphics& g) {
    g.setColour(colours_.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f);
    g.setColour(colours_.edge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
}

void SuiteBar::resized() {
    auto row = getLocalBounds().reduced(8, 4);
    auto place = [&](juce::Button& button, int width) {
        button.setBounds(row.removeFromLeft(width).reduced(2, 0));
    };
    place(undoButton_, 62);
    place(redoButton_, 62);
    row.removeFromLeft(8);
    place(slotA_, 44);
    place(slotB_, 44);
    place(copyAToB_, 58);
    place(copyBToA_, 58);
    presetsButton_.setBounds(row.removeFromRight(90).reduced(2, 0));
    nextPreset_.setBounds(row.removeFromRight(44).reduced(2, 0));
    presetName_.setBounds(row.removeFromRight(240));
    previousPreset_.setBounds(row.removeFromRight(44).reduced(2, 0));
}

}
