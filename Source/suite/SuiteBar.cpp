#include "SuiteBar.h"

namespace openfruit::suite {

SuiteBar::SuiteBar(UndoHistory& undo, AbCompare& ab)
    : undo_(undo), ab_(ab) {
    colours_ = {
        juce::Colour(0xff0d1317),
        juce::Colour(0xff152026),
        juce::Colour(0xff283842),
        juce::Colour(0xffe6f0f5),
        juce::Colour(0xff869ca8),
        juce::Colour(0xff48f0c8)
    };
    undo_.addChangeListener(this);
    ab_.addChangeListener(this);

    styleButton(undoButton_, "Undo the last edit");
    styleButton(redoButton_, "Redo the last undone edit");
    styleButton(slotA_, "Hear compare slot A");
    styleButton(slotB_, "Hear compare slot B");
    styleButton(copyAToB_, "Copy slot A into slot B");
    styleButton(copyBToA_, "Copy slot B into slot A");
    styleButton(presetsButton_, "Browse factory and saved kits");

    undoButton_.onClick = [this] { undo_.undo(); };
    redoButton_.onClick = [this] { undo_.redo(); };
    slotA_.onClick = [this] { if (onSelectSlot) onSelectSlot(0); };
    slotB_.onClick = [this] { if (onSelectSlot) onSelectSlot(1); };
    copyAToB_.onClick = [this] { if (onCopySlot) onCopySlot(0, 1); };
    copyBToA_.onClick = [this] { if (onCopySlot) onCopySlot(1, 0); };
    presetsButton_.onClick = [this] { if (onShowPresets) onShowPresets(); };

    for (auto* button : { &undoButton_, &redoButton_, &slotA_, &slotB_, &copyAToB_, &copyBToA_, &presetsButton_ })
        addAndMakeVisible(button);
    refresh();
}

SuiteBar::~SuiteBar() {
    undo_.removeChangeListener(this);
    ab_.removeChangeListener(this);
}

void SuiteBar::styleButton(juce::Button& button, const juce::String& tooltip) {
    button.setTooltip(tooltip);
    button.setClickingTogglesState(false);
}

void SuiteBar::setColours(SuiteColours colours) {
    colours_ = colours;
    repaint();
}

void SuiteBar::changeListenerCallback(juce::ChangeBroadcaster*) {
    refresh();
}

void SuiteBar::refresh() {
    undoButton_.setEnabled(undo_.canUndo());
    redoButton_.setEnabled(undo_.canRedo());
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
    auto row = getLocalBounds().reduced(8, 6);
    auto place = [&](juce::Button& button, int width) {
        button.setBounds(row.removeFromLeft(width).reduced(2, 0));
    };
    place(undoButton_, 58);
    place(redoButton_, 58);
    row.removeFromLeft(8);
    place(slotA_, 36);
    place(slotB_, 36);
    place(copyAToB_, 52);
    place(copyBToA_, 52);
    row.removeFromLeft(8);
    place(presetsButton_, 78);
}

}
