#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FruitEngine.h"
#include "ToneCache.h"
#include "AnalysisQueue.h"
#include "Theme.h"
#include <vector>
#include <string>

namespace openfruit::ui {

struct BrowserFileItem {
    std::string fullPath;
    std::string fileName;
    int64_t fileSize = 0;
    int64_t modTime = 0;
    openfruit::core::ToneAnalysisResult toneResult;
    bool hasTone = false;
};

class SampleBrowserModal : public juce::Component,
                           public juce::TableListBoxModel {
public:
    SampleBrowserModal(int targetTrackIdx,
                       openfruit::core::FruitEngine& engine,
                       openfruit::core::ToneCache& cache,
                       openfruit::core::AnalysisQueue& queue);
    ~SampleBrowserModal() override;

    void setTheme(const openfruit::theme::Palette& palette);
    void setFolder(const juce::File& folder);

    std::function<void()> onClose;

    // TableListBoxModel implementation
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshFileList();
    void sortItems();
    void previewSelectedItem();
    void clearCache();
    void commitSelection();
    void cancelAndClose();

    const int targetTrackIdx_;
    openfruit::core::FruitEngine& engine_;
    openfruit::core::ToneCache& cache_;
    openfruit::core::AnalysisQueue& queue_;
    openfruit::theme::Palette palette_;

    juce::File currentFolder_;
    std::vector<BrowserFileItem> files_;

    // Sorting modes: 0=Name, 1=Length Asc, 2=Length Desc, 3=Tone
    int sortMode_ = 1; // Default: sort by length (requested by user)

    juce::Label titleLabel_;
    juce::Label folderLabel_;
    juce::TextButton browseFolderBtn_{ "Browse Folder..." };
    juce::TextButton clearCacheBtn_{ "Clear Tone Cache" };
    juce::ComboBox sortBox_;

    juce::TableListBox table_;

    juce::ToggleButton autoPlayToggle_{ "Auto-Play" };
    juce::TextButton playBtn_{ "► Audition" };
    juce::Label midiLiveNoteLabel_;
    juce::TextButton commitBtn_{ "Load Sample" };
    juce::TextButton cancelBtn_{ "Cancel" };

    std::unique_ptr<juce::FileChooser> fileChooser_;
};

} // namespace openfruit::ui
