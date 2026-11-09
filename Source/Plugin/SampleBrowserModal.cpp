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
                                       openfruit::core::AnalysisQueue& queue)
    : targetTrackIdx_(targetTrackIdx),
      engine_(engine),
      cache_(cache),
      queue_(queue),
      palette_(openfruit::theme::getPalette(openfruit::core::ColorTheme::CyberpunkNeon)) {

    const int midiCh = engine_.getTrack(targetTrackIdx).midiChannel;

    // 1. Title Label
    titleLabel_.setText("Sample browser: track " + juce::String(targetTrackIdx + 1) + " (MIDI channel " + juce::String(midiCh) + ")",
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);

    // 2. Folder Navigation & Buttons
    folderLabel_.setText("Folder: (none)", juce::dontSendNotification);
    addAndMakeVisible(folderLabel_);

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
        sortItems();
    };
    addAndMakeVisible(sortBox_);

    // 4. Table List Box
    table_.setModel(this);
    table_.getHeader().addColumn("File", 1, 310, 150, 500);
    table_.getHeader().addColumn("Estimated Tone / Pitch", 2, 230, 140, 350);
    table_.getHeader().addColumn("Length", 3, 110, 80, 180);
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

    // Default to music directory if exists
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
    if (onFolderChanged) {
        onFolderChanged(currentFolder_);
    }
    refreshFileList();
}

void SampleBrowserModal::refreshFileList() {
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
        files_.push_back(std::move(item));
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
                for (auto& f : safeThis->files_) {
                    if (f.fullPath == path) {
                        f.toneResult = res;
                        f.hasTone = true;
                        break;
                    }
                }
                if (safeThis->sortMode_ != 3) {
                    safeThis->sortItems();
                    return;
                }
                safeThis->table_.repaint();
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
    const auto area = juce::Rectangle<float>(6.0f, 2.0f, static_cast<float>(width - 12), static_cast<float>(height - 4));

    if (columnId == 1) {
        // File Name with mini audio icon
        g.setColour(palette_.accentSecondary.withAlpha(0.6f));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(juce::CharPointer_UTF8("\xe2\x99\xaa"), static_cast<int>(area.getX()), static_cast<int>(area.getY()), 14, static_cast<int>(area.getHeight()), juce::Justification::centredLeft, false);

        g.setColour(rowIsSelected ? juce::Colours::white : palette_.text);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(item.fileName, area.withTrimmedLeft(18.0f).toNearestInt(), juce::Justification::centredLeft, true);
    } else if (columnId == 2) {
        // Estimated Tone & Frequency Badge
        if (item.hasTone) {
            // Note pill badge
            const std::string noteStr = item.toneResult.hasClearTone ? item.toneResult.noteName : ("~" + item.toneResult.noteName);
            const float badgeW = 44.0f;
            const auto badgeRect = juce::Rectangle<float>(area.getX(), area.getY() + 1.0f, badgeW, area.getHeight() - 2.0f);

            g.setColour(palette_.panel.darker(0.35f));
            g.fillRoundedRectangle(badgeRect, 3.0f);
            g.setColour(palette_.accentSecondary.withAlpha(0.85f));
            g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);

            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.setColour(palette_.accentSecondary);
            g.drawText(noteStr, badgeRect.toNearestInt(), juce::Justification::centred, false);

            // Frequency and detune text
            std::ostringstream ss;
            if (item.toneResult.detuneCents != 0) {
                if (item.toneResult.detuneCents > 0) ss << "+" << item.toneResult.detuneCents << "c ";
                else ss << item.toneResult.detuneCents << "c ";
            }
            ss << "(" << std::fixed << std::setprecision(1) << item.toneResult.dominantFrequencyHz << " Hz)";

            g.setFont(juce::FontOptions(11.0f));
            g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.9f) : palette_.text.withAlpha(0.85f));
            g.drawText(ss.str(), area.withTrimmedLeft(badgeW + 8.0f).toNearestInt(), juce::Justification::centredLeft, true);
        } else {
            g.setColour(palette_.textDim);
            g.setFont(juce::FontOptions(11.0f, juce::Font::italic));
            g.drawText("Estimating tone...", area.toNearestInt(), juce::Justification::centredLeft, true);
        }
    } else if (columnId == 3) {
        // Length
        if (item.hasTone) {
            const auto badgeRect = juce::Rectangle<float>(area.getX(), area.getY() + 1.0f, 58.0f, area.getHeight() - 2.0f);
            g.setColour(palette_.panel.darker(0.25f));
            g.fillRoundedRectangle(badgeRect, 3.0f);
            g.setColour(palette_.panelEdge.withAlpha(0.55f));
            g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);

            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.setColour(palette_.accentPrimary);
            g.drawText(item.toneResult.formattedDuration(), badgeRect.toNearestInt(), juce::Justification::centred, false);
        } else {
            g.setColour(palette_.textDim);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("---", area.toNearestInt(), juce::Justification::centredLeft, true);
        }
    }
}

void SampleBrowserModal::selectedRowsChanged(int lastRowSelected) {
    if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(files_.size())) {
        return;
    }

    const auto& item = files_[static_cast<size_t>(lastRowSelected)];
    auto sample = decodeItemToSample(item);
    if (!sample) {
        return;
    }

    engine_.setAuditionSampleForTrack(targetTrackIdx_, sample);
    if (autoPlayToggle_.getToggleState()) {
        engine_.triggerTrackManual(targetTrackIdx_, 0.9f, 0);
    }
}

void SampleBrowserModal::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&) {
    if (rowNumber < 0 || rowNumber >= static_cast<int>(files_.size())) {
        return;
    }
    table_.selectRow(rowNumber);
    commitSelection();
}

void SampleBrowserModal::sortOrderChanged(int newSortColumnId, bool isForwards) {
    if (newSortColumnId == 3) {
        sortMode_ = isForwards ? 1 : 2;
    }
    if (newSortColumnId == 1) {
        sortMode_ = 3;
    }
    if (newSortColumnId == 2) {
        sortMode_ = 4;
    }
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
    const auto wav = openfruit::core::WavReader::readWavFile(item.fullPath);
    if (!wav.isValid || wav.channels.empty()) {
        return nullptr;
    }

    auto sample = std::make_shared<openfruit::core::AudioSampleBuffer>();
    sample->filePath = item.fullPath;
    sample->sampleName = juce::File(item.fullPath).getFileNameWithoutExtension().toStdString();
    if (sample->sampleName.empty()) {
        sample->sampleName = item.fileName;
    }
    sample->numChannels = wav.numChannels;
    sample->sampleRate = wav.sampleRate;
    sample->numSamples = wav.numSamples;
    sample->channels = wav.channels;
    sample->toneResult = item.toneResult;
    if (!item.hasTone && !wav.monoChannel.empty()) {
        sample->toneResult = openfruit::core::SonicAnalyzer::analyze(
            wav.monoChannel.data(),
            static_cast<int64_t>(wav.monoChannel.size()),
            static_cast<double>(wav.sampleRate)
        );
    }
    sample->isLoaded = true;
    return sample;
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

void SampleBrowserModal::commitSelection() {
    const int row = table_.getSelectedRow();
    if (row >= 0 && row < static_cast<int>(files_.size())) {
        if (!engine_.getTrack(targetTrackIdx_).auditionSample) {
            if (auto sample = decodeItemToSample(files_[static_cast<size_t>(row)])) {
                engine_.setAuditionSampleForTrack(targetTrackIdx_, sample);
            }
        }
        engine_.commitAuditionSample(targetTrackIdx_);
    }
    if (onClose) onClose();
}

void SampleBrowserModal::cancelAndClose() {
    engine_.clearAuditionSampleForTrack(targetTrackIdx_);
    if (onClose) onClose();
}

void SampleBrowserModal::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    // Dark semi-translucent backdrop
    g.fillAll(palette_.background.withAlpha(0.97f));

    // Outer cyber panel border with neon glow
    g.setColour(palette_.accentSecondary.withAlpha(0.35f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);

    g.setColour(palette_.panelEdge);
    g.drawRoundedRectangle(bounds.reduced(2.0f), 5.0f, 1.0f);

    // Header bar gradient backdrop
    const auto headerRect = juce::Rectangle<float>(2.0f, 2.0f, bounds.getWidth() - 4.0f, 36.0f);
    juce::ColourGradient hgrad(
        palette_.panel.brighter(0.2f), 0.0f, headerRect.getY(),
        palette_.panel.darker(0.15f), 0.0f, headerRect.getBottom(),
        false
    );
    g.setGradientFill(hgrad);
    g.fillRoundedRectangle(headerRect, 5.0f);

    // Header bottom line
    g.setColour(palette_.panelEdge.withAlpha(0.7f));
    g.drawLine(2.0f, 38.0f, bounds.getRight() - 2.0f, 38.0f, 1.0f);

    // Bottom action bar backdrop
    const auto bottomRect = juce::Rectangle<float>(2.0f, bounds.getBottom() - 44.0f, bounds.getWidth() - 4.0f, 42.0f);
    g.setColour(palette_.panel.darker(0.3f));
    g.fillRoundedRectangle(bottomRect, 4.0f);
    g.setColour(palette_.panelEdge.withAlpha(0.6f));
    g.drawLine(2.0f, bounds.getBottom() - 44.0f, bounds.getRight() - 2.0f, bounds.getBottom() - 44.0f, 1.0f);
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
