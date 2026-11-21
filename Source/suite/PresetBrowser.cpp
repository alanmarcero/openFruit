#include "PresetBrowser.h"
#include <algorithm>

namespace openfruit::suite {

PresetBrowser::PresetBrowser(PresetLibrary& library)
    : library_(library) {
    colours_ = {
        juce::Colour(0xff0d1317),
        juce::Colour(0xff152026),
        juce::Colour(0xff283842),
        juce::Colour(0xffe6f0f5),
        juce::Colour(0xff869ca8),
        juce::Colour(0xff48f0c8)
    };
    search_.setTextToShowWhenEmpty("Search kits", colours_.textDim);
    search_.setTooltip("Filter factory and saved kits");
    search_.onTextChange = [this] { refreshList(); };
    name_.setTextToShowWhenEmpty("Kit name", colours_.textDim);
    name_.setTooltip("Name used when saving or renaming a kit");
    addAndMakeVisible(search_);
    addAndMakeVisible(name_);

    list_.setRowHeight(28);
    list_.setTooltip("Factory kits and kits you saved. Double-click or press Enter to load one.");
    addAndMakeVisible(list_);

    save_.setTooltip("Save the current song as a kit");
    rename_.setTooltip("Rename the selected saved kit");
    remove_.setTooltip("Delete the selected saved kit");
    save_.onClick = [this] {
        if (!captureState)
            return;
        juce::String error;
        if (!library_.save(name_.getText(), captureState(), error))
            showError(error);
        else
            showError({});
        if (error.isEmpty() && onSaved)
            onSaved(name_.getText());
        refreshList();
    };
    rename_.onClick = [this] {
        const int row = list_.getSelectedRow();
        if (row < 0 || row >= static_cast<int>(entries_.size()))
            return;
        juce::String error;
        if (!library_.rename(entries_[static_cast<size_t>(row)], name_.getText(), error))
            showError(error);
        else
            showError({});
        refreshList();
    };
    remove_.onClick = [this] {
        const int row = list_.getSelectedRow();
        if (row < 0 || row >= static_cast<int>(entries_.size()))
            return;
        juce::String error;
        if (!library_.remove(entries_[static_cast<size_t>(row)], error))
            showError(error);
        else
            showError({});
        refreshList();
    };
    for (auto* button : { &save_, &rename_, &remove_ })
        addAndMakeVisible(button);
    status_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(status_);
    refreshList();
}

void PresetBrowser::setColours(SuiteColours colours) {
    colours_ = colours;
    search_.setTextToShowWhenEmpty("Search kits", colours_.textDim);
    name_.setTextToShowWhenEmpty("Kit name", colours_.textDim);
    repaint();
}

void PresetBrowser::refreshList() {
    const auto selected = list_.getSelectedRow() >= 0 && list_.getSelectedRow() < static_cast<int>(entries_.size())
        ? entries_[static_cast<size_t>(list_.getSelectedRow())].name
        : juce::String();
    entries_ = library_.list(search_.getText());
    list_.updateContent();
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[static_cast<size_t>(i)].name == selected) {
            list_.selectRow(i, false, true);
            break;
        }
    }
    repaint();
}

void PresetBrowser::showError(const juce::String& message) {
    status_.setText(message, juce::dontSendNotification);
}

void PresetBrowser::apply(const PresetEntry& entry) {
    if (!restoreState)
        return;
    juce::MemoryBlock state;
    juce::String error;
    if (!library_.load(entry, state, error)) {
        showError(error);
        return;
    }
    showError({});
    restoreState(state, entry.name);
}

void PresetBrowser::stepPreset(const juce::String& currentName, int delta) {
    if (entries_.empty())
        return;
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&currentName](const PresetEntry& entry) { return entry.name == currentName; });
    const int count = static_cast<int>(entries_.size());
    const int current = static_cast<int>(std::distance(entries_.begin(), found));
    // A kit that is not in the list steps to the first entry, or back to the last.
    const int next = found == entries_.end() && delta > 0 ? 0 : (current + delta + count) % count;
    list_.selectRow(next);
    name_.setText(entries_[static_cast<size_t>(next)].name, juce::dontSendNotification);
    apply(entries_[static_cast<size_t>(next)]);
}

int PresetBrowser::getNumRows() {
    return static_cast<int>(entries_.size());
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return;
    const auto& entry = entries_[static_cast<size_t>(row)];
    g.fillAll(selected ? colours_.accent.withAlpha(0.22f) : colours_.panel);
    g.setColour(entry.favorite ? colours_.accent : colours_.textDim);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(entry.favorite ? "*" : ".", 6, 0, 16, height, juce::Justification::centredLeft);
    g.setColour(colours_.text);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(entry.name, 26, 0, width - 110, height, juce::Justification::centredLeft);
    g.setColour(colours_.textDim);
    g.drawText(entry.factory ? "Factory" : "Saved", width - 78, 0, 70, height, juce::Justification::centredRight);
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& event) {
    if (row < 0 || row >= static_cast<int>(entries_.size()))
        return;
    auto entry = entries_[static_cast<size_t>(row)];
    if (event.x < 22) {
        library_.setFavorite(entry, !entry.favorite);
        refreshList();
        return;
    }
    name_.setText(entry.name, juce::dontSendNotification);
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent& event) {
    if (row >= 0 && row < static_cast<int>(entries_.size()) && event.x >= 22)
        apply(entries_[static_cast<size_t>(row)]);
}

void PresetBrowser::returnKeyPressed(int lastRow) {
    if (lastRow >= 0 && lastRow < static_cast<int>(entries_.size()))
        apply(entries_[static_cast<size_t>(lastRow)]);
}

void PresetBrowser::selectedRowsChanged(int) {}

void PresetBrowser::paint(juce::Graphics& g) {
    g.setColour(colours_.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(colours_.edge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
}

void PresetBrowser::resized() {
    auto area = getLocalBounds().reduced(10, 8);
    search_.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);
    auto buttons = area.removeFromBottom(32);
    status_.setBounds(area.removeFromBottom(18));
    name_.setBounds(area.removeFromBottom(28));
    area.removeFromBottom(6);
    list_.setBounds(area);
    const int width = buttons.getWidth() / 3;
    save_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    rename_.setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    remove_.setBounds(buttons.reduced(2, 0));
}

}
