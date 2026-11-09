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
                           public juce::TableListBoxModel,
                           public juce::FileDragAndDropTarget {
public:
    SampleBrowserModal(int targetTrackIdx,
                       openfruit::core::FruitEngine& engine,
                       openfruit::core::ToneCache& cache,
                       openfruit::core::AnalysisQueue& queue);
    ~SampleBrowserModal() override;

    void setTheme(const openfruit::theme::Palette& palette);
    void setFolder(const juce::File& folder);

    std::function<void()> onClose;
    std::function<void(const juce::File&)> onFolderChanged;

    // TableListBoxModel implementation
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

    // FileDragAndDropTarget implementation
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshFileList();
    void sortItems();
    std::shared_ptr<openfruit::core::AudioSampleBuffer> decodeItemToSample(const BrowserFileItem& item);
    void selectFileByPath(const std::string& fullPath);
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
    juce::TextButton browseFolderBtn_{ "Browse folder" };
    juce::TextButton clearCacheBtn_{ "Clear cache" };
    juce::ComboBox sortBox_;

    juce::TableListBox table_;

    juce::ToggleButton autoPlayToggle_{ "Autoplay" };
    juce::TextButton playBtn_{ "Preview" };
    juce::Label midiLiveNoteLabel_;
    juce::TextButton commitBtn_{ "Load" };
    juce::TextButton cancelBtn_{ "Cancel" };

    std::unique_ptr<juce::FileChooser> fileChooser_;
};

} // namespace openfruit::ui
