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
    static WavAudioData readWavFile(const std::string& filePath);
    static WavAudioData readWavFromMemory(const uint8_t* data, size_t sizeBytes);
};

} // namespace openfruit::core
