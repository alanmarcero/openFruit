#include "WavReader.h"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace openfruit::core {

namespace {

uint16_t readU16LE(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t readU32LE(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

WavAudioData WavReader::readWavFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    const std::streamsize size = file.tellg();
    if (size < 44) {
        return {};
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }
    return readWavFromMemory(buffer.data(), buffer.size());
}

WavAudioData WavReader::readWavFromMemory(const uint8_t* data, size_t sizeBytes) {
    WavAudioData result{};
    if (sizeBytes < 44 || data == nullptr) {
        return result;
    }

    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return result;
    }

    size_t offset = 12;
    int audioFormat = 1; // 1 = PCM, 3 = IEEE float
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    const uint8_t* pcmData = nullptr;
    size_t pcmBytes = 0;

    while (offset + 8 <= sizeBytes) {
        const char chunkId[5] = {
            static_cast<char>(data[offset]),
            static_cast<char>(data[offset + 1]),
            static_cast<char>(data[offset + 2]),
            static_cast<char>(data[offset + 3]),
            '\0'
        };
        const uint32_t chunkSize = readU32LE(data + offset + 4);
        const size_t chunkDataOffset = offset + 8;

        if (std::strcmp(chunkId, "fmt ") == 0 && chunkSize >= 16 && chunkDataOffset + 16 <= sizeBytes) {
            audioFormat = readU16LE(data + chunkDataOffset);
            channels = readU16LE(data + chunkDataOffset + 2);
            sampleRate = static_cast<int>(readU32LE(data + chunkDataOffset + 4));
            bitsPerSample = readU16LE(data + chunkDataOffset + 14);
        } else if (std::strcmp(chunkId, "data") == 0) {
            pcmData = data + chunkDataOffset;
            pcmBytes = std::min<size_t>(chunkSize, sizeBytes > chunkDataOffset ? sizeBytes - chunkDataOffset : 0);
        }

        offset = chunkDataOffset + chunkSize;
        if (chunkSize % 2 != 0) {
            offset += 1; // Align to 2-byte boundary
        }
    }

    if (channels <= 0 || sampleRate <= 0 || pcmData == nullptr || pcmBytes == 0) {
        return result;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        return result;
    }
    const int frameBytes = bytesPerSample * channels;
    const int64_t numFrames = static_cast<int64_t>(pcmBytes / frameBytes);
    if (numFrames <= 0) {
        return result;
    }

    result.numChannels = channels;
    result.sampleRate = sampleRate;
    result.bitsPerSample = bitsPerSample;
    result.numSamples = numFrames;
    result.channels.resize(static_cast<size_t>(channels));
    for (auto& ch : result.channels) {
        ch.resize(static_cast<size_t>(numFrames));
    }
    result.monoChannel.resize(static_cast<size_t>(numFrames));

    if (audioFormat == 1 && bitsPerSample == 16) {
        for (int64_t f = 0; f < numFrames; ++f) {
            float monoSum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                const int16_t val = static_cast<int16_t>(readU16LE(pcmData + (f * frameBytes) + (ch * 2)));
                const float s = static_cast<float>(val) / 32768.0f;
                result.channels[static_cast<size_t>(ch)][static_cast<size_t>(f)] = s;
                monoSum += s;
            }
            result.monoChannel[static_cast<size_t>(f)] = monoSum / static_cast<float>(channels);
        }
        result.isValid = true;
    } else if (audioFormat == 1 && bitsPerSample == 24) {
        for (int64_t f = 0; f < numFrames; ++f) {
            float monoSum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                const size_t pos = static_cast<size_t>((f * frameBytes) + (ch * 3));
                int32_t val = static_cast<int32_t>((static_cast<uint32_t>(pcmData[pos]) << 8) |
                                                   (static_cast<uint32_t>(pcmData[pos + 1]) << 16) |
                                                   (static_cast<uint32_t>(pcmData[pos + 2]) << 24));
                val >>= 8;
                const float s = static_cast<float>(val) / 8388608.0f;
                result.channels[static_cast<size_t>(ch)][static_cast<size_t>(f)] = s;
                monoSum += s;
            }
            result.monoChannel[static_cast<size_t>(f)] = monoSum / static_cast<float>(channels);
        }
        result.isValid = true;
    } else if (audioFormat == 3 && bitsPerSample == 32) {
        // 32-bit float
        for (int64_t f = 0; f < numFrames; ++f) {
            float monoSum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                float s = 0.0f;
                std::memcpy(&s, pcmData + (f * frameBytes) + (ch * 4), sizeof(float));
                result.channels[static_cast<size_t>(ch)][static_cast<size_t>(f)] = s;
                monoSum += s;
            }
            result.monoChannel[static_cast<size_t>(f)] = monoSum / static_cast<float>(channels);
        }
        result.isValid = true;
    }

    return result;
}

} // namespace openfruit::core
