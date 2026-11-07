#include "ToneCache.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>

namespace openfruit::core {

namespace {

std::string escapeString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

} // namespace

std::string ToneCache::makeKey(const std::string& filePath, int64_t modTime, int64_t fileSize) {
    return filePath + "|" + std::to_string(modTime) + "|" + std::to_string(fileSize);
}

std::optional<ToneAnalysisResult> ToneCache::get(const std::string& filePath,
                                                 int64_t modTime,
                                                 int64_t fileSize) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::string key = makeKey(filePath, modTime, fileSize);
    const auto it = entries_.find(key);
    if (it != entries_.end()) {
        return it->second.result;
    }
    return std::nullopt;
}

void ToneCache::put(const std::string& filePath,
                    int64_t modTime,
                    int64_t fileSize,
                    const ToneAnalysisResult& result) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::string key = makeKey(filePath, modTime, fileSize);
    entries_[key] = CacheEntry{ filePath, modTime, fileSize, result };
}

void ToneCache::clear() {
    const std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

size_t ToneCache::size() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

std::string ToneCache::getDefaultCachePath() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        std::filesystem::path dir = std::filesystem::path(home) / ".openfruit";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "tone_cache.tsv").string();
    }
    return "openfruit_tone_cache.tsv";
}

bool ToneCache::saveToFile(const std::string& filePath) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "# openFruit Tone Cache TSV Format v1\n";
    for (const auto& [key, entry] : entries_) {
        out << escapeString(entry.filePath) << "\t"
            << entry.modTime << "\t"
            << entry.fileSize << "\t"
            << entry.result.dominantFrequencyHz << "\t"
            << entry.result.noteName << "\t"
            << entry.result.midiNote << "\t"
            << entry.result.detuneCents << "\t"
            << entry.result.durationSeconds << "\t"
            << entry.result.durationSamples << "\t"
            << entry.result.peakAmplitude << "\t"
            << entry.result.rmsAmplitude << "\t"
            << (entry.result.hasClearTone ? 1 : 0) << "\n";
    }
    return true;
}

bool ToneCache::loadFromFile(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return false;
    }

    const std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string rawPath;
        int64_t modTime = 0;
        int64_t fileSize = 0;
        double freq = 0.0;
        std::string noteName;
        int midiNote = 60;
        int cents = 0;
        double durSec = 0.0;
        int64_t durSamples = 0;
        float peak = 0.0f;
        float rms = 0.0f;
        int hasToneInt = 0;

        if (std::getline(ss, rawPath, '\t') &&
            (ss >> modTime) &&
            (ss >> fileSize) &&
            (ss >> freq) &&
            (ss >> noteName) &&
            (ss >> midiNote) &&
            (ss >> cents) &&
            (ss >> durSec) &&
            (ss >> durSamples) &&
            (ss >> peak) &&
            (ss >> rms) &&
            (ss >> hasToneInt)) {

            ToneAnalysisResult res{};
            res.dominantFrequencyHz = freq;
            res.noteName = noteName;
            res.midiNote = midiNote;
            res.detuneCents = cents;
            res.durationSeconds = durSec;
            res.durationSamples = durSamples;
            res.peakAmplitude = peak;
            res.rmsAmplitude = rms;
            res.hasClearTone = (hasToneInt != 0);

            const std::string key = makeKey(rawPath, modTime, fileSize);
            entries_[key] = CacheEntry{ rawPath, modTime, fileSize, res };
        }
    }
    return true;
}

} // namespace openfruit::core
