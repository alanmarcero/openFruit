#pragma once

#include "SonicAnalyzer.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <cstdint>

namespace openfruit::core {

struct CacheEntry {
    std::string filePath;
    int64_t modTime = 0;
    int64_t fileSize = 0;
    ToneAnalysisResult result;
};

class ToneCache {
public:
    ToneCache() = default;
    ~ToneCache() = default;

    std::optional<ToneAnalysisResult> get(const std::string& filePath, int64_t modTime, int64_t fileSize) const;
    void put(const std::string& filePath, int64_t modTime, int64_t fileSize, const ToneAnalysisResult& result);
    void clear();
    [[nodiscard]] size_t size() const;

    bool saveToFile(const std::string& filePath) const;
    bool loadFromFile(const std::string& filePath);

    static std::string getDefaultCachePath();
    static bool getFileStats(const std::string& filePath, int64_t& outModTime, int64_t& outFileSize);

private:
    static std::string makeKey(const std::string& filePath, int64_t modTime, int64_t fileSize);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> entries_;
};

} // namespace openfruit::core
