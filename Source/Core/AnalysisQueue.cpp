#include "AnalysisQueue.h"
#include "WavReader.h"
#include <filesystem>
#include <chrono>

namespace openfruit::core {

AnalysisQueue::AnalysisQueue(ToneCache& cache, int batchSize)
    : cache_(cache), batchSize_(std::max(1, batchSize)) {
    workerThread_ = std::thread(&AnalysisQueue::workerLoop, this);
}

AnalysisQueue::~AnalysisQueue() {
    stop();
}

void AnalysisQueue::stop() {
    shouldStop_ = true;
    cv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void AnalysisQueue::queueFolderFiles(const std::string& folderPath,
                                     const std::vector<std::string>& filePaths,
                                     Callback onResult) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        currentFolder_ = folderPath;
        pendingFiles_ = filePaths;
        callback_ = std::move(onResult);
    }
    cv_.notify_one();
}

void AnalysisQueue::clearQueue() {
    const std::lock_guard<std::mutex> lock(mutex_);
    pendingFiles_.clear();
}

int AnalysisQueue::pendingCount() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pendingFiles_.size());
}

bool AnalysisQueue::isBusy() const {
    return isBusy_.load();
}

void AnalysisQueue::workerLoop() {
    while (!shouldStop_) {
        std::vector<std::string> batch;
        Callback cb;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return shouldStop_ || !pendingFiles_.empty();
            });

            if (shouldStop_) {
                break;
            }

            const int count = std::min(batchSize_, static_cast<int>(pendingFiles_.size()));
            batch.reserve(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i) {
                batch.push_back(std::move(pendingFiles_.front()));
                pendingFiles_.erase(pendingFiles_.begin());
            }
            cb = callback_;
        }

        if (batch.empty()) {
            isBusy_ = false;
            continue;
        }

        isBusy_ = true;

        for (const auto& filePath : batch) {
            if (shouldStop_) {
                break;
            }

            std::error_code ec;
            const auto ftime = std::filesystem::last_write_time(filePath, ec);
            const auto fsize = std::filesystem::file_size(filePath, ec);
            if (ec) {
                continue;
            }

            const int64_t modTime = std::chrono::duration_cast<std::chrono::seconds>(
                ftime.time_since_epoch()).count();
            const int64_t fileSize = static_cast<int64_t>(fsize);

            // Check cache first
            auto cached = cache_.get(filePath, modTime, fileSize);
            if (cached.has_value()) {
                if (cb) {
                    cb(filePath, *cached);
                }
                continue;
            }

            // Not in cache: analyze
            const auto wavData = WavReader::readWavFile(filePath);
            if (wavData.isValid && !wavData.monoChannel.empty()) {
                const auto result = SonicAnalyzer::analyze(wavData.monoChannel.data(),
                                                           wavData.numSamples,
                                                           static_cast<double>(wavData.sampleRate));
                cache_.put(filePath, modTime, fileSize, result);
                if (cb) {
                    cb(filePath, result);
                }
            }
        }
    }
    isBusy_ = false;
}

} // namespace openfruit::core
