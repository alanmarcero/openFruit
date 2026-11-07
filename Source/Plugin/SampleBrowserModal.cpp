#include "SampleBrowserModal.h"
#include "WavReader.h"
#include <algorithm>

namespace openfruit::ui {

SampleBrowserModal::SampleBrowserModal(int targetTrackIdx,
                                       openfruit::core::FruitEngine& engine,
                                       openfruit::core::ToneCache& cache,
                                       openfruit::core::AnalysisQueue& queue)
    : targetTrackIdx_(targetTrackIdx),
      engine_(engine),
      cache_(cache),
      queue_(queue),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

    // 1. Title Label
    titleLabel_.setText("SAMPLE BROWSER - TRACK " + juce::String(targetTrackIdx + 1) + " (MIDI CH " + juce::String(targetTrackIdx + 1) + ")",
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);

    // 2. Folder Navigation & Buttons
    folderLabel_.setText("Folder: (None)", juce::dontSendNotification);
    addAndMakeVisible(folderLabel_);

    browseFolderBtn_.onClick = [this] {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Select Sample Folder",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*"
        );
        fileChooser_->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& chooser) {
                auto result = chooser.getResult();
                if (result.isDirectory()) {
                    setFolder(result);
                } else if (result.exists()) {
                    setFolder(result.getParentDirectory());
                }
            }
        );
    };
    addAndMakeVisible(browseFolderBtn_);

    clearCacheBtn_.onClick = [this] { clearCache(); };
    addAndMakeVisible(clearCacheBtn_);

    // 3. Sorting Dropdown
    sortBox_.addItem("Sort by Length (Shortest)", 1);
    sortBox_.addItem("Sort by Length (Longest)", 2);
    sortBox_.addItem("Sort by Name", 3);
    sortBox_.addItem("Sort by Note / Tone", 4);
    sortBox_.setSelectedId(1, juce::dontSendNotification);
    sortBox_.onChange = [this] {
        sortMode_ = sortBox_.getSelectedId();
        sortItems();
    };
    addAndMakeVisible(sortBox_);

    // 4. Table List Box
    table_.setModel(this);
    table_.getHeader().addColumn("File Name", 1, 260, 150, 400);
    table_.getHeader().addColumn("Estimated Tone / Note", 2, 170, 120, 250);
    table_.getHeader().addColumn("Length", 3, 110, 80, 160);
    table_.setMultipleSelectionEnabled(false);
    addAndMakeVisible(table_);

    // 5. Controls at Bottom
    autoPlayToggle_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(autoPlayToggle_);

    playBtn_.onClick = [this] { previewSelectedItem(); };
    addAndMakeVisible(playBtn_);

    midiLiveNoteLabel_.setText("LIVE MIX AUDITION: Play MIDI Ch " + juce::String(targetTrackIdx + 1) + " to hear sample live in mix!",
                               juce::dontSendNotification);
    midiLiveNoteLabel_.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    addAndMakeVisible(midiLiveNoteLabel_);

    commitBtn_.onClick = [this] { commitSelection(); };
    addAndMakeVisible(commitBtn_);

    cancelBtn_.onClick = [this] { cancelAndClose(); };
    addAndMakeVisible(cancelBtn_);

    // Default to samples directory if exists, else music directory
    auto defaultDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    if (defaultDir.exists()) {
        setFolder(defaultDir);
    }
}

SampleBrowserModal::~SampleBrowserModal() {
    queue_.clearQueue();
}

void SampleBrowserModal::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    repaint();
    table_.repaint();
}

void SampleBrowserModal::setFolder(const juce::File& folder) {
    if (!folder.isDirectory()) return;
    currentFolder_ = folder;
    folderLabel_.setText("Folder: " + currentFolder_.getFullPathName(), juce::dontSendNotification);
    refreshFileList();
}

void SampleBrowserModal::refreshFileList() {
    files_.clear();
    queue_.clearQueue();

    if (!currentFolder_.isDirectory()) {
        table_.updateContent();
        return;
    }

    juce::Array<juce::File> found;
    currentFolder_.findChildFiles(found, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.mp3;*.flac");

    std::vector<std::string> pendingPaths;
    pendingPaths.reserve(static_cast<size_t>(found.size()));

    for (const auto& file : found) {
        BrowserFileItem item;
        item.fullPath = file.getFullPathName().toStdString();
        item.fileName = file.getFileName().toStdString();
        item.fileSize = file.getSize();
        item.modTime = file.getLastModificationTime().toMilliseconds() / 1000;

        // Check cache
        auto cached = cache_.get(item.fullPath, item.modTime, item.fileSize);
        if (cached.has_value()) {
            item.toneResult = *cached;
            item.hasTone = true;
        } else {
            pendingPaths.push_back(item.fullPath);
        }
        files_.push_back(std::move(item));
    }

    sortItems();

    // Start background queue for items needing analysis (max 5 at a time)
    if (!pendingPaths.empty()) {
        queue_.queueFolderFiles(currentFolder_.getFullPathName().toStdString(),
                                pendingPaths,
                                [this](const std::string& path, const openfruit::core::ToneAnalysisResult& res) {
            juce::MessageManager::callAsync([this, path, res] {
                for (auto& f : files_) {
                    if (f.fullPath == path) {
                        f.toneResult = res;
                        f.hasTone = true;
                        break;
                    }
                }
                table_.repaint();
            });
        });
    }
}

void SampleBrowserModal::sortItems() {
    if (sortMode_ == 1) {
        // Sort by length ascending (shortest first)
        std::sort(files_.begin(), files_.end(), [](const BrowserFileItem& a, const BrowserFileItem& b) {
            return a.toneResult.durationSeconds < b.toneResult.durationSeconds;
        });
    } else if (sortMode_ == 2) {
        // Sort by length descending (longest first)
        std::sort(files_.begin(), files_.end(), [](const BrowserFileItem& a, const BrowserFileItem& b) {
            return a.toneResult.durationSeconds > b.toneResult.durationSeconds;
        });
    } else if (sortMode_ == 3) {
        // Sort by name
        std::sort(files_.begin(), files_.end(), [](const BrowserFileItem& a, const BrowserFileItem& b) {
            return a.fileName < b.fileName;
        });
    } else if (sortMode_ == 4) {
        // Sort by note / tone
        std::sort(files_.begin(), files_.end(), [](const BrowserFileItem& a, const BrowserFileItem& b) {
            return a.toneResult.midiNote < b.toneResult.midiNote;
        });
    }
    table_.updateContent();
    table_.repaint();
}

int SampleBrowserModal::getNumRows() {
    return static_cast<int>(files_.size());
}

void SampleBrowserModal::paintRowBackground(juce::Graphics& g, int /*rowNumber*/, int /*width*/, int /*height*/, bool rowIsSelected) {
    if (rowIsSelected) {
        g.setColour(palette_.accentPrimary.withAlpha(0.25f));
        g.fillAll();
    }
}

void SampleBrowserModal::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) {
    if (rowNumber < 0 || rowNumber >= static_cast<int>(files_.size())) {
        return;
    }

    const auto& item = files_[static_cast<size_t>(rowNumber)];
    g.setColour(rowIsSelected ? palette_.text : palette_.text.withAlpha(0.85f));
    g.setFont(juce::FontOptions(12.0f));

    const auto area = juce::Rectangle<int>(4, 0, width - 8, height);

    if (columnId == 1) {
        // File Name
        g.drawText(item.fileName, area, juce::Justification::centredLeft, true);
    } else if (columnId == 2) {
        // Estimated Tone
        if (item.hasTone) {
            g.setColour(palette_.accentSecondary);
            g.drawText(item.toneResult.formattedTone(), area, juce::Justification::centredLeft, true);
        } else {
            g.setColour(palette_.textDim);
            g.drawText("Analyzing...", area, juce::Justification::centredLeft, true);
        }
    } else if (columnId == 3) {
        // Length
        if (item.hasTone) {
            g.setColour(palette_.textDim);
            g.drawText(item.toneResult.formattedDuration(), area, juce::Justification::centredLeft, true);
        } else {
            g.drawText("---", area, juce::Justification::centredLeft, true);
        }
    }
}

void SampleBrowserModal::selectedRowsChanged(int lastRowSelected) {
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(files_.size())) {
        if (autoPlayToggle_.getToggleState()) {
            previewSelectedItem();
        }
    }
}

void SampleBrowserModal::sortOrderChanged(int newSortColumnId, bool isForwards) {
    if (newSortColumnId == 3) {
        sortMode_ = isForwards ? 1 : 2;
    } else if (newSortColumnId == 1) {
        sortMode_ = 3;
    } else if (newSortColumnId == 2) {
        sortMode_ = 4;
    }
    sortItems();
}

void SampleBrowserModal::previewSelectedItem() {
    const int row = table_.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(files_.size())) {
        return;
    }

    const auto& item = files_[static_cast<size_t>(row)];

    // Decode file
    const auto wav = openfruit::core::WavReader::readWavFile(item.fullPath);
    if (!wav.isValid || wav.channels.empty()) {
        return;
    }

    auto sample = std::make_shared<openfruit::core::AudioSampleBuffer>();
    sample->filePath = item.fullPath;
    sample->sampleName = item.fileName;
    sample->numChannels = wav.numChannels;
    sample->sampleRate = wav.sampleRate;
    sample->numSamples = wav.numSamples;
    sample->channels = wav.channels;
    sample->toneResult = item.toneResult;
    sample->isLoaded = true;

    // Set audition sample for this track so incoming MIDI triggers it live!
    engine_.setAuditionSampleForTrack(targetTrackIdx_, sample);

    // Also trigger one-shot preview immediately
    engine_.triggerTrackManual(targetTrackIdx_, 0.9f, 0);
}

void SampleBrowserModal::clearCache() {
    cache_.clear();
    cache_.saveToFile(openfruit::core::ToneCache::getDefaultCachePath());
    refreshFileList();
}

void SampleBrowserModal::commitSelection() {
    const int row = table_.getSelectedRow();
    if (row >= 0 && row < static_cast<int>(files_.size())) {
        engine_.commitAuditionSample(targetTrackIdx_);
    }
    if (onClose) onClose();
}

void SampleBrowserModal::cancelAndClose() {
    engine_.clearAuditionSampleForTrack(targetTrackIdx_);
    if (onClose) onClose();
}

void SampleBrowserModal::paint(juce::Graphics& g) {
    g.fillAll(palette_.background.withAlpha(0.96f));

    // Outer border
    g.setColour(palette_.panelEdge);
    g.drawRect(getLocalBounds(), 1);
}

void SampleBrowserModal::resized() {
    auto area = getLocalBounds().reduced(12);

    // Title
    titleLabel_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);

    // Folder navigation row
    auto topRow = area.removeFromTop(28);
    browseFolderBtn_.setBounds(topRow.removeFromRight(120).reduced(2));
    clearCacheBtn_.setBounds(topRow.removeFromRight(130).reduced(2));
    folderLabel_.setBounds(topRow);

    area.removeFromTop(6);

    // Sorting bar
    auto sortRow = area.removeFromTop(26);
    sortBox_.setBounds(sortRow.removeFromLeft(220));

    area.removeFromTop(6);

    // Bottom Action Row
    auto bottomRow = area.removeFromBottom(36);
    commitBtn_.setBounds(bottomRow.removeFromRight(120).reduced(3));
    cancelBtn_.setBounds(bottomRow.removeFromRight(90).reduced(3));
    autoPlayToggle_.setBounds(bottomRow.removeFromLeft(90));
    playBtn_.setBounds(bottomRow.removeFromLeft(100).reduced(3));
    midiLiveNoteLabel_.setBounds(bottomRow.reduced(2));

    area.removeFromBottom(8);

    // Table view
    table_.setBounds(area);
}

} // namespace openfruit::ui
