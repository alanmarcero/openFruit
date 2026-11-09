#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace openfruit::core {

struct WavAudioData {
    int numChannels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    int64_t numSamples = 0;
    std::vector<float> monoChannel;
    std::vector<std::vector<float>> channels;
    bool isValid = false;
};

class WavReader {
public:
    static bool isSupportedAudioFile(const std::string& filePath) noexcept;
    static WavAudioData readWavFile(const std::string& filePath);
    static WavAudioData readWavFileForAnalysis(const std::string& filePath, int64_t maxFrames = 65536);
    static WavAudioData readWavFromMemory(const uint8_t* data,
                                          size_t sizeBytes,
                                          bool analysisOnly = false,
                                          int64_t maxFrames = 0);
};

} // namespace openfruit::core
