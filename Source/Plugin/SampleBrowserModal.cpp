#include "SampleBrowserModal.h"
#include "WavReader.h"
#include "SonicAnalyzer.h"
#include <algorithm>
#include <filesystem>

namespace openfruit::ui {

namespace {

constexpr size_t kMaxScannedAudioFiles = 2500;
constexpr int kMaxScanDirectoryDepth = 6;

bool hasHiddenPathComponent(const std::filesystem::path& relativePath) {
    for (const auto& part : relativePath) {
        const std::string name = part.filename().string();
        if (!name.empty() && name[0] == '.') {
            return true;
        }
    }
    return false;
}

std::vector<juce::File> findAudioFilesInFolder(const juce::File& rootFolder) {
    std::vector<juce::File> results;
    if (!rootFolder.isDirectory()) {
        return results;
    }

    std::error_code ec;
    const std::filesystem::path rootPath(rootFolder.getFullPathName().toStdString());
    auto it = std::filesystem::recursive_directory_iterator(
        rootPath,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    );
    const std::filesystem::recursive_directory_iterator end;

    while (!ec && it != end && results.size() < kMaxScannedAudioFiles) {
        const auto& entry = *it;
        const auto relPath = std::filesystem::relative(entry.path(), rootPath, ec);
        if (!ec && hasHiddenPathComponent(relPath)) {
            if (entry.is_directory(ec)) {
                it.disable_recursion_pending();
            }
            it.increment(ec);
            continue;
        }

        if (it.depth() > kMaxScanDirectoryDepth && entry.is_directory(ec)) {
            it.disable_recursion_pending();
        }

        std::error_code fileEc;
        if (entry.is_regular_file(fileEc)) {
            const std::string fullPath = entry.path().string();
            if (openfruit::core::WavReader::isSupportedAudioFile(fullPath)) {
                results.emplace_back(juce::String::fromUTF8(fullPath.c_str()));
            }
        }

        it.increment(ec);
        if (ec) {
            ec.clear();
        }
    }

    return results;
}

juce::File extractChosenFileFromChooser(const juce::FileChooser& chooser) {
    const auto results = chooser.getResults();
    if (!results.isEmpty()) {
        return results.getFirst();
    }
    const auto urls = chooser.getURLResults();
    if (!urls.isEmpty() && urls.getFirst().isLocalFile()) {
        return urls.getFirst().getLocalFile();
    }
    return chooser.getResult();
}

} // namespace

SampleBrowserModal::SampleBrowserModal(int targetTrackIdx,
                                       openfruit::core::FruitEngine& engine,
                                       openfruit::core::ToneCache& cache,
                                       openfruit::core::AnalysisQueue& queue,
                                       const juce::File& initialFolder)
    : targetTrackIdx_(targetTrackIdx),
      engine_(engine),
      cache_(cache),
      queue_(queue),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

    const int midiCh = engine_.getTrack(targetTrackIdx).midiChannel;

    browseFolderBtn_.setTooltip("Choose a folder or audio file");
    browseFolderBtn_.setTitle("Browse folder");
    clearCacheBtn_.setTooltip("Clear cached pitch analysis and rescan this folder");
    clearCacheBtn_.setTitle("Clear cache");
    sortBox_.setTooltip("Change sample order");
    sortBox_.setTitle("Sort");
    autoPlayToggle_.setTooltip("Audition each sample as you select it");
    autoPlayToggle_.setTitle("Autoplay");
    playBtn_.setTooltip("Hear the selected sample in this track");
    playBtn_.setTitle("Preview");
    commitBtn_.setTooltip("Assign the selected sample and return to the pattern");
    commitBtn_.setTitle("Load sample");
    cancelBtn_.setTooltip("Return to the pattern with the current sample");
    cancelBtn_.setTitle("Cancel");
    commitBtn_.setButtonText("Load sample");
    table_.setRowHeight(34);
    // 1. Title Label
    titleLabel_.setText("Sample library    /    Track " + juce::String(targetTrackIdx + 1).paddedLeft('0', 2),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);

    // 2. Folder Navigation & Buttons
    folderLabel_.setText("Folder: (none)", juce::dontSendNotification);
    addAndMakeVisible(folderLabel_);

    searchBox_.setTextToShowWhenEmpty("Search samples...", palette_.textDim);
    searchBox_.setTooltip("Filter samples by name");
    searchBox_.setTitle("Search samples");
    searchBox_.onTextChange = [this] { applyFilter(); };
    searchBox_.onReturnKey = [this] {
        if (table_.getSelectedRow() >= 0) {
            commitSelection();
        }
    };
    searchBox_.onEscapeKey = [this] { cancelAndClose(); };
    addAndMakeVisible(searchBox_);
    setWantsKeyboardFocus(true);

    browseFolderBtn_.onClick = [this] {
        const juce::File startDir = currentFolder_.isDirectory()
            ? currentFolder_
            : juce::File::getSpecialLocation(juce::File::userHomeDirectory);

        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Select sample folder or audio file",
            startDir,
            "*"
        );
        juce::Component::SafePointer<SampleBrowserModal> safeThis(this);
        fileChooser_->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectDirectories
                | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser) {
                if (safeThis == nullptr) {
                    return;
                }
                const auto result = extractChosenFileFromChooser(chooser);
                if (result.isDirectory()) {
                    safeThis->setFolder(result);
                    return;
                }
                if (result.existsAsFile()) {
                    safeThis->setFolder(result.getParentDirectory());
                    safeThis->selectFileByPath(result.getFullPathName().toStdString());
                }
            }
        );
    };
    addAndMakeVisible(browseFolderBtn_);

    clearCacheBtn_.onClick = [this] { clearCache(); };
    addAndMakeVisible(clearCacheBtn_);

    // 3. Sorting Dropdown
    sortBox_.addItem("Sort by length (shortest first)", 1);
    sortBox_.addItem("Sort by length (longest first)", 2);
    sortBox_.addItem("Sort by name", 3);
    sortBox_.addItem("Sort by tone", 4);
    sortBox_.setSelectedId(1, juce::dontSendNotification);
    sortBox_.onChange = [this] {
        sortMode_ = sortBox_.getSelectedId();
        syncHeaderToSortMode();
        sortItems();
    };
    addAndMakeVisible(sortBox_);

    // 4. Table List Box
    table_.setModel(this);
    table_.getHeader().addColumn("File", 1, 290, 150, 1100);
    table_.getHeader().addColumn("Estimated Tone / Pitch", 2, 260, 160, 800);
    table_.getHeader().addColumn("Length", 3, 110, 80, 600);
    syncHeaderToSortMode();
    table_.setMultipleSelectionEnabled(false);
    addAndMakeVisible(table_);

    // 5. Controls at Bottom
    autoPlayToggle_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(autoPlayToggle_);

    playBtn_.onClick = [this] { previewSelectedItem(); };
    addAndMakeVisible(playBtn_);

    midiLiveNoteLabel_.setText("MIDI channel " + juce::String(midiCh) + " plays the selected sample in the mix.",
                               juce::dontSendNotification);
    midiLiveNoteLabel_.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(midiLiveNoteLabel_);

    commitBtn_.onClick = [this] { commitSelection(); };
    addAndMakeVisible(commitBtn_);

    cancelBtn_.onClick = [this] { cancelAndClose(); };
    addAndMakeVisible(cancelBtn_);

    selectedRowsChanged(-1);
    const juce::File startDir = initialFolder.isDirectory()
        ? initialFolder
        : juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    if (startDir.isDirectory()) {
        setFolder(startDir);
    }
}

SampleBrowserModal::~SampleBrowserModal() {
    engine_.clearAuditionSampleForTrack(targetTrackIdx_);
    queue_.clearQueue();
}

void SampleBrowserModal::setTheme(const openfruit::theme::Palette& palette) {
    palette_ = palette;
    table_.setColour(juce::ListBox::backgroundColourId, palette_.panel);
    searchBox_.setColour(juce::TextEditor::backgroundColourId, palette_.panel.darker(0.2f));
    searchBox_.setColour(juce::TextEditor::textColourId, palette_.text);
    searchBox_.setColour(juce::TextEditor::outlineColourId, palette_.panelEdge);
    searchBox_.setColour(juce::TextEditor::focusedOutlineColourId, palette_.accentPrimary);
    searchBox_.setTextToShowWhenEmpty("Search samples...", palette_.textDim);
    repaint();
    table_.repaint();
}

void SampleBrowserModal::setFolder(const juce::File& folder) {
    if (!folder.isDirectory()) return;
    currentFolder_ = folder;
    folderLabel_.setTooltip(currentFolder_.getFullPathName());
    folderLabel_.setText("Folder   /   " + currentFolder_.getFileName(), juce::dontSendNotification);
    if (onFolderChanged) {
        onFolderChanged(currentFolder_);
    }
    refreshFileList();
}

void SampleBrowserModal::refreshFileList() {
    table_.deselectAllRows();
    engine_.clearAuditionSampleForTrack(targetTrackIdx_);
    allFiles_.clear();
    files_.clear();
    queue_.clearQueue();

    if (!currentFolder_.isDirectory()) {
        table_.updateContent();
        return;
    }

    const auto found = findAudioFilesInFolder(currentFolder_);

    std::vector<std::string> pendingPaths;
    pendingPaths.reserve(found.size());

    for (const auto& file : found) {
        BrowserFileItem item;
        item.fullPath = file.getFullPathName().toStdString();
        const juce::String relName = file.getRelativePathFrom(currentFolder_);
        item.fileName = (relName.isNotEmpty() ? relName : file.getFileName()).toStdString();
        if (!openfruit::core::ToneCache::getFileStats(item.fullPath, item.modTime, item.fileSize)) {
            item.fileSize = file.getSize();
            item.modTime = file.getLastModificationTime().toMilliseconds() / 1000;
        }

        const auto cached = cache_.get(item.fullPath, item.modTime, item.fileSize);
        if (cached.has_value()) {
            item.toneResult = *cached;
            item.hasTone = true;
        }
        if (!cached.has_value()) {
            pendingPaths.push_back(item.fullPath);
        }
        allFiles_.push_back(std::move(item));
    }

    sortItems();

    if (!pendingPaths.empty()) {
        juce::Component::SafePointer<SampleBrowserModal> safeThis(this);
        queue_.queueFolderFiles(currentFolder_.getFullPathName().toStdString(),
                                pendingPaths,
                                [safeThis](const std::string& path, const openfruit::core::ToneAnalysisResult& res) {
            juce::MessageManager::callAsync([safeThis, path, res] {
                if (safeThis == nullptr) {
                    return;
                }
                for (auto& f : safeThis->allFiles_) {
                    if (f.fullPath == path) {
                        f.toneResult = res;
                        f.hasTone = true;
                        break;
                    }
                }
                for (auto& f : safeThis->files_) {
                    if (f.fullPath == path) {
                        f.toneResult = res;
                        f.hasTone = true;
                        break;
                    }
                }
                // Repaint without re-sorting so rows stay put under the pointer.
                safeThis->table_.repaint();
            });
        });
    }
}

void SampleBrowserModal::sortItems() {
    std::stable_sort(allFiles_.begin(), allFiles_.end(), [mode = sortMode_](const BrowserFileItem& a, const BrowserFileItem& b) {
        if (mode == 3) {
            return a.fileName < b.fileName;
        }
        if (a.hasTone != b.hasTone) {
            return a.hasTone;
        }
        if (!a.hasTone) {
            return a.fileName < b.fileName;
        }
        if (mode == 1) {
            if (a.toneResult.durationSeconds < b.toneResult.durationSeconds) return true;
            if (b.toneResult.durationSeconds < a.toneResult.durationSeconds) return false;
            return a.fileName < b.fileName;
        }
        if (mode == 2) {
            if (a.toneResult.durationSeconds > b.toneResult.durationSeconds) return true;
            if (b.toneResult.durationSeconds > a.toneResult.durationSeconds) return false;
            return a.fileName < b.fileName;
        }
        if (mode == 4 && a.toneResult.midiNote != b.toneResult.midiNote) {
            return a.toneResult.midiNote < b.toneResult.midiNote;
        }
        return a.fileName < b.fileName;
    });
    applyFilter();
}

void SampleBrowserModal::applyFilter() {
    const int row = table_.getSelectedRow();
    const auto selected = row >= 0 && row < static_cast<int>(files_.size())
        ? files_[static_cast<size_t>(row)].fullPath
        : std::string{};
    const juce::String query = searchBox_.getText().trim();

    files_.clear();
    if (query.isEmpty()) {
        files_ = allFiles_;
    }
    if (query.isNotEmpty()) {
        for (const auto& item : allFiles_) {
            if (juce::String(item.fileName).containsIgnoreCase(query)) {
                files_.push_back(item);
            }
        }
    }

    table_.updateContent();
    if (!selected.empty()) {
        const juce::ScopedValueSetter<bool> preserve(restoringSelection_, true);
        selectFileByPath(selected);
    }
    if (table_.getSelectedRow() < 0) {
        selectedRowsChanged(-1);
    }
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
    const auto area = juce::Rectangle<float>(6.0f, 2.0f, static_cast<float>(width - 12), static_cast<float>(height - 4));

    if (columnId == 1) {
        // File Name with mini audio icon
        g.setColour(palette_.accentSecondary.withAlpha(0.6f));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::CharPointer_UTF8("\xe2\x99\xaa"), static_cast<int>(area.getX()), static_cast<int>(area.getY()), 14, static_cast<int>(area.getHeight()), juce::Justification::centredLeft, false);

        g.setColour(rowIsSelected ? juce::Colours::white : palette_.text);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(item.fileName, area.withTrimmedLeft(18.0f).toNearestInt(), juce::Justification::centredLeft, true);
    } else if (columnId == 2) {
        // Estimated Tone & Frequency Badge
        if (item.hasTone) {
            // Note pill badge
            const std::string noteStr = item.toneResult.hasClearTone ? item.toneResult.noteName : ("~" + item.toneResult.noteName);
            const float badgeW = 50.0f;
            const auto badgeRect = juce::Rectangle<float>(area.getX(), area.getY() + 1.0f, badgeW, area.getHeight() - 2.0f);

            g.setColour(palette_.panel.darker(0.35f));
            g.fillRoundedRectangle(badgeRect, 3.0f);
            g.setColour(palette_.accentSecondary.withAlpha(0.85f));
            g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);

            g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            g.setColour(palette_.accentSecondary);
            g.drawText(noteStr, badgeRect.toNearestInt(), juce::Justification::centred, false);

            // Frequency and detune text
            std::ostringstream ss;
            if (item.toneResult.detuneCents != 0) {
                if (item.toneResult.detuneCents > 0) ss << "+" << item.toneResult.detuneCents << "c ";
                else ss << item.toneResult.detuneCents << "c ";
            }
            ss << "(" << std::fixed << std::setprecision(1) << item.toneResult.dominantFrequencyHz << " Hz)";

            g.setFont(juce::FontOptions(13.0f));
            g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.95f) : palette_.text.withAlpha(0.9f));
            g.drawText(ss.str(), area.withTrimmedLeft(badgeW + 8.0f).toNearestInt(), juce::Justification::centredLeft, true);
        } else {
            g.setColour(palette_.textDim);
            g.setFont(juce::FontOptions(12.0f, juce::Font::italic));
            g.drawText("Estimating tone...", area.toNearestInt(), juce::Justification::centredLeft, true);
        }
    } else if (columnId == 3) {
        // Length
        if (item.hasTone) {
            const auto badgeRect = juce::Rectangle<float>(area.getX(), area.getY() + 1.0f, 62.0f, area.getHeight() - 2.0f);
            g.setColour(palette_.panel.darker(0.25f));
            g.fillRoundedRectangle(badgeRect, 3.0f);
            g.setColour(palette_.panelEdge.withAlpha(0.55f));
            g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);

            g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            g.setColour(palette_.accentPrimary);
            g.drawText(item.toneResult.formattedDuration(), badgeRect.toNearestInt(), juce::Justification::centred, false);
        } else {
            g.setColour(palette_.textDim);
            g.setFont(juce::FontOptions(12.0f));
            g.drawText("---", area.toNearestInt(), juce::Justification::centredLeft, true);
        }
    }
}

void SampleBrowserModal::selectedRowsChanged(int lastRowSelected) {
    if (restoringSelection_) return;
    const bool selected = lastRowSelected >= 0 && lastRowSelected < static_cast<int>(files_.size());
    commitBtn_.setEnabled(selected);
    playBtn_.setEnabled(selected);
    if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(files_.size())) {
        return;
    }

    const auto& item = files_[static_cast<size_t>(lastRowSelected)];
    auto sample = decodeItemToSample(item);
    if (!sample) {
        engine_.clearAuditionSampleForTrack(targetTrackIdx_);
        commitBtn_.setEnabled(false);
        return;
    }
    engine_.setAuditionSampleForTrack(targetTrackIdx_, sample);
    if (autoPlayToggle_.getToggleState()) {
        engine_.triggerTrackManual(targetTrackIdx_, 0.9f, 0);
    }
}

void SampleBrowserModal::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&) {
    commitRow(rowNumber);
}

void SampleBrowserModal::returnKeyPressed(int row) {
    commitRow(row);
}

void SampleBrowserModal::syncHeaderToSortMode() {
    syncingSort_ = true;
    auto& header = table_.getHeader();
    if (sortMode_ == 1)
        header.setSortColumnId(3, true);
    else if (sortMode_ == 2)
        header.setSortColumnId(3, false);
    else if (sortMode_ == 3)
        header.setSortColumnId(1, true);
    else if (sortMode_ == 4)
        header.setSortColumnId(2, true);
    syncingSort_ = false;
}

void SampleBrowserModal::sortOrderChanged(int newSortColumnId, bool isForwards) {
    if (syncingSort_)
        return;
    if (newSortColumnId == 3) {
        sortMode_ = isForwards ? 1 : 2;
    }
    if (newSortColumnId == 1) {
        sortMode_ = 3;
    }
    if (newSortColumnId == 2) {
        sortMode_ = 4;
    }
    sortBox_.setSelectedId(sortMode_, juce::dontSendNotification);
    sortItems();
}

bool SampleBrowserModal::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files) {
        const juce::File file(f);
        if (file.isDirectory() || openfruit::core::WavReader::isSupportedAudioFile(f.toStdString())) {
            return true;
        }
    }
    return false;
}

void SampleBrowserModal::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/) {
    if (files.isEmpty()) {
        return;
    }
    const juce::File first(files[0]);
    if (first.isDirectory()) {
        setFolder(first);
        return;
    }
    if (first.existsAsFile()) {
        setFolder(first.getParentDirectory());
        selectFileByPath(first.getFullPathName().toStdString());
    }
}

std::shared_ptr<openfruit::core::AudioSampleBuffer> SampleBrowserModal::decodeItemToSample(const BrowserFileItem& item) {
    return core::WavReader::loadSample(item.fullPath, item.hasTone ? &item.toneResult : nullptr);
}

void SampleBrowserModal::selectFileByPath(const std::string& fullPath) {
    for (size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].fullPath == fullPath) {
            table_.selectRow(static_cast<int>(i));
            return;
        }
    }
}

void SampleBrowserModal::previewSelectedItem() {
    const int row = table_.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(files_.size())) {
        return;
    }

    const auto& item = files_[static_cast<size_t>(row)];
    auto sample = decodeItemToSample(item);
    if (!sample) {
        return;
    }

    engine_.setAuditionSampleForTrack(targetTrackIdx_, sample);
    engine_.triggerTrackManual(targetTrackIdx_, 0.9f, 0);
}

void SampleBrowserModal::clearCache() {
    cache_.clear();
    cache_.saveToFile(openfruit::core::ToneCache::getDefaultCachePath());
    refreshFileList();
}

void SampleBrowserModal::commitRow(int row) {
    if (row < 0 || row >= static_cast<int>(files_.size())) {
        return;
    }
    table_.selectRow(row);
    commitSelection();
}

void SampleBrowserModal::commitSelection() {
    const int row = table_.getSelectedRow();
    if (row >= 0 && row < static_cast<int>(files_.size())) {
        if (!engine_.getTrack(targetTrackIdx_).auditionSample) {
            if (auto sample = decodeItemToSample(files_[static_cast<size_t>(row)])) {
                engine_.setAuditionSampleForTrack(targetTrackIdx_, sample);
            }
        }
        if (engine_.getTrack(targetTrackIdx_).auditionSample) {
            engine_.commitAuditionSample(targetTrackIdx_);
            if (onSampleLoaded) onSampleLoaded();
        }
    }
    if (onClose) onClose();
}

void SampleBrowserModal::cancelAndClose() {
    engine_.clearAuditionSampleForTrack(targetTrackIdx_);
    if (onClose) onClose();
}

void SampleBrowserModal::paint(juce::Graphics& g) {
    g.fillAll(palette_.background);
    g.setColour(palette_.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1), 8);
}

void SampleBrowserModal::resized() {
    auto area = getLocalBounds().reduced(24, 14);
    auto heading = area.removeFromTop(46);
    clearCacheBtn_.setBounds(heading.removeFromRight(106).reduced(2,5));
    browseFolderBtn_.setBounds(heading.removeFromRight(128).reduced(2,5));
    titleLabel_.setBounds(heading);
    auto tools = area.removeFromTop(46).reduced(0,5);
    sortBox_.setBounds(tools.removeFromRight(235));
    tools.removeFromRight(8);
    searchBox_.setBounds(tools.removeFromRight(240));
    folderLabel_.setBounds(tools.reduced(2,0));
    auto actions = area.removeFromBottom(48).reduced(0,5);
    commitBtn_.setBounds(actions.removeFromRight(128).reduced(2,0));
    cancelBtn_.setBounds(actions.removeFromRight(88).reduced(2,0));
    autoPlayToggle_.setBounds(actions.removeFromLeft(105).reduced(2,0));
    playBtn_.setBounds(actions.removeFromLeft(94).reduced(2,0));
    midiLiveNoteLabel_.setBounds(actions.reduced(12,0));
    table_.setBounds(area.reduced(0,4));
    auto& header = table_.getHeader();
    const int w1 = table_.getWidth() * 44 / 100;
    const int w2 = table_.getWidth() * 38 / 100;
    header.setColumnWidth(1, w1);
    header.setColumnWidth(2, w2);
    header.setColumnWidth(3, std::max(0, table_.getWidth() - w1 - w2));
}

bool SampleBrowserModal::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        cancelAndClose();
        return true;
    }
    if (key == juce::KeyPress::returnKey) {
        if (table_.getSelectedRow() >= 0) {
            commitSelection();
        }
        return true;
    }
    return false;
}

} // namespace openfruit::ui
