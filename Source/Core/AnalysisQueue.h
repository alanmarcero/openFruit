#pragma once

#include "SonicAnalyzer.h"
#include "ToneCache.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace openfruit::core {

class AnalysisQueue {
public:
    using Callback = std::function<void(const std::string& filePath, const ToneAnalysisResult& result)>;

    explicit AnalysisQueue(ToneCache& cache, int batchSize = 5);
    ~AnalysisQueue();

    // Set files for the currently active folder. Cancels previously queued items.
    void queueFolderFiles(const std::string& folderPath,
                          const std::vector<std::string>& filePaths,
                          Callback onResult);

    void clearQueue();
    void stop();

    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] bool isBusy() const;

private:
    void workerLoop();

    ToneCache& cache_;
    const int batchSize_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread workerThread_;

    std::atomic<bool> shouldStop_{ false };
    std::string currentFolder_;
    std::vector<std::string> pendingFiles_;
    Callback callback_;
    std::atomic<bool> isBusy_{ false };
};

} // namespace openfruit::core
