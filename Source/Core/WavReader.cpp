#include "WavReader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

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

uint16_t readU16BE(const uint8_t* p) noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t readU32BE(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

int parseIeee80ExtendedSampleRate(const uint8_t* bytes) noexcept {
    const int exponent = ((bytes[0] & 0x7F) << 8) | bytes[1];
    const uint32_t hiMantissa = readU32BE(bytes + 2);
    const uint32_t loMantissa = readU32BE(bytes + 6);

    if (exponent == 0 && hiMantissa == 0 && loMantissa == 0) {
        return 0;
    }

    const double f = std::ldexp(static_cast<double>(hiMantissa), exponent - 16383 - 31) +
                     std::ldexp(static_cast<double>(loMantissa), exponent - 16383 - 63);
    if (!std::isfinite(f) || f <= 0.0) {
        return 0;
    }
    return static_cast<int>(std::round(f));
}

float decodeSampleLE(const uint8_t* p, int audioFormat, int bitsPerSample) noexcept {
    if (audioFormat == 1) {
        if (bitsPerSample == 8) {
            return (static_cast<int>(p[0]) - 128) / 128.0f;
        }
        if (bitsPerSample == 16) {
            const int16_t val = static_cast<int16_t>(readU16LE(p));
            return static_cast<float>(val) / 32768.0f;
        }
        if (bitsPerSample == 24) {
            int32_t val = static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 8) |
                                               (static_cast<uint32_t>(p[1]) << 16) |
                                               (static_cast<uint32_t>(p[2]) << 24));
            val >>= 8;
            return static_cast<float>(val) / 8388608.0f;
        }
        if (bitsPerSample == 32) {
            const int32_t val = static_cast<int32_t>(readU32LE(p));
            return static_cast<float>(static_cast<double>(val) / 2147483648.0);
        }
    } else if (audioFormat == 3) {
        if (bitsPerSample == 32) {
            float s = 0.0f;
            std::memcpy(&s, p, sizeof(float));
            return std::isfinite(s) ? s : 0.0f;
        }
        if (bitsPerSample == 64) {
            double d = 0.0;
            std::memcpy(&d, p, sizeof(double));
            return std::isfinite(d) ? static_cast<float>(d) : 0.0f;
        }
    }
    return 0.0f;
}

float decodeSampleBE(const uint8_t* p, int audioFormat, int bitsPerSample) noexcept {
    if (audioFormat == 1) {
        if (bitsPerSample == 8) {
            return static_cast<float>(static_cast<int8_t>(p[0])) / 128.0f;
        }
        if (bitsPerSample == 16) {
            const int16_t val = static_cast<int16_t>(readU16BE(p));
            return static_cast<float>(val) / 32768.0f;
        }
        if (bitsPerSample == 24) {
            int32_t val = static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 24) |
                                               (static_cast<uint32_t>(p[1]) << 16) |
                                               (static_cast<uint32_t>(p[2]) << 8));
            val >>= 8;
            return static_cast<float>(val) / 8388608.0f;
        }
        if (bitsPerSample == 32) {
            const int32_t val = static_cast<int32_t>(readU32BE(p));
            return static_cast<float>(static_cast<double>(val) / 2147483648.0);
        }
    } else if (audioFormat == 3 && bitsPerSample == 32) {
        const uint32_t bits = readU32BE(p);
        float s = 0.0f;
        std::memcpy(&s, &bits, sizeof(float));
        return std::isfinite(s) ? s : 0.0f;
    }
    return 0.0f;
}

WavAudioData populateAudioFrames(const uint8_t* pcmData,
                                 size_t availablePcmBytes,
                                 int64_t totalHeaderFrames,
                                 int channels,
                                 int sampleRate,
                                 int bitsPerSample,
                                 int audioFormat,
                                 bool isBigEndian,
                                 bool analysisOnly,
                                 int64_t maxFrames) {
    WavAudioData result{};
    const int bytesPerSample = (bitsPerSample + 7) / 8;
    if (bytesPerSample <= 0 || channels <= 0 || sampleRate <= 0 || pcmData == nullptr) {
        return result;
    }

    if ((audioFormat != 1 && audioFormat != 3) ||
        (audioFormat == 1 && bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) ||
        (audioFormat == 3 && bitsPerSample != 32 && bitsPerSample != 64)) {
        return result;
    }

    const int frameBytes = bytesPerSample * channels;
    const int64_t availableFrames = static_cast<int64_t>(availablePcmBytes / static_cast<size_t>(frameBytes));
    if (availableFrames <= 0) {
        return result;
    }

    const int64_t totalFrames = std::max(availableFrames, totalHeaderFrames);
    const int64_t framesToDecode = (maxFrames > 0) ? std::min(availableFrames, maxFrames) : availableFrames;

    result.numChannels = channels;
    result.sampleRate = sampleRate;
    result.bitsPerSample = bitsPerSample;
    result.numSamples = analysisOnly ? totalFrames : framesToDecode;
    result.monoChannel.resize(static_cast<size_t>(framesToDecode));

    if (!analysisOnly) {
        result.channels.resize(static_cast<size_t>(channels));
        for (auto& ch : result.channels) {
            ch.resize(static_cast<size_t>(framesToDecode));
        }
    }

    const float invChannels = 1.0f / static_cast<float>(channels);
    double sumSqMix = 0.0;
    double sumSqCh0 = 0.0;

    for (int64_t f = 0; f < framesToDecode; ++f) {
        float monoSum = 0.0f;
        float ch0Sample = 0.0f;
        const uint8_t* framePtr = pcmData + static_cast<size_t>(f * frameBytes);

        for (int ch = 0; ch < channels; ++ch) {
            const uint8_t* samplePtr = framePtr + static_cast<size_t>(ch * bytesPerSample);
            const float s = isBigEndian ? decodeSampleBE(samplePtr, audioFormat, bitsPerSample)
                                        : decodeSampleLE(samplePtr, audioFormat, bitsPerSample);
            if (!analysisOnly) {
                result.channels[static_cast<size_t>(ch)][static_cast<size_t>(f)] = s;
            }
            if (ch == 0) {
                ch0Sample = s;
            }
            monoSum += s;
        }

        const float mix = monoSum * invChannels;
        result.monoChannel[static_cast<size_t>(f)] = mix;
        sumSqMix += static_cast<double>(mix * mix);
        sumSqCh0 += static_cast<double>(ch0Sample * ch0Sample);
    }

    // Prevent out-of-phase stereo cancellation in monoChannel
    if (channels > 1 && sumSqMix < 0.15 * sumSqCh0) {
        for (int64_t f = 0; f < framesToDecode; ++f) {
            const uint8_t* samplePtr = pcmData + static_cast<size_t>(f * frameBytes);
            result.monoChannel[static_cast<size_t>(f)] = isBigEndian
                ? decodeSampleBE(samplePtr, audioFormat, bitsPerSample)
                : decodeSampleLE(samplePtr, audioFormat, bitsPerSample);
        }
    }

    result.isValid = true;
    return result;
}

WavAudioData readAiffFromMemory(const uint8_t* data,
                                size_t sizeBytes,
                                bool analysisOnly,
                                int64_t maxFrames) {
    if (sizeBytes < 46 || data == nullptr) {
        return {};
    }

    const bool isAifc = (std::memcmp(data + 8, "AIFC", 4) == 0);
    size_t offset = 12;
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    int64_t totalHeaderFrames = 0;
    int audioFormat = 1;
    bool isBigEndian = true;
    const uint8_t* pcmData = nullptr;
    size_t availablePcmBytes = 0;

    while (offset + 8 <= sizeBytes) {
        const uint32_t chunkSize = readU32BE(data + offset + 4);
        const size_t chunkDataOffset = offset + 8;

        if (std::memcmp(data + offset, "COMM", 4) == 0 && chunkSize >= 18 && chunkDataOffset + 18 <= sizeBytes) {
            channels = static_cast<int>(readU16BE(data + chunkDataOffset));
            totalHeaderFrames = static_cast<int64_t>(readU32BE(data + chunkDataOffset + 2));
            bitsPerSample = static_cast<int>(readU16BE(data + chunkDataOffset + 6));
            sampleRate = parseIeee80ExtendedSampleRate(data + chunkDataOffset + 8);

            if (isAifc && chunkSize >= 22 && chunkDataOffset + 22 <= sizeBytes) {
                const uint8_t* comp = data + chunkDataOffset + 18;
                if (std::memcmp(comp, "NONE", 4) == 0) {
                    audioFormat = 1;
                    isBigEndian = true;
                } else if (std::memcmp(comp, "sowt", 4) == 0) {
                    audioFormat = 1;
                    isBigEndian = false;
                } else if (std::memcmp(comp, "fl32", 4) == 0 || std::memcmp(comp, "FL32", 4) == 0) {
                    audioFormat = 3;
                    bitsPerSample = 32;
                    isBigEndian = true;
                } else {
                    audioFormat = 0;
                }
            }
        } else if (std::memcmp(data + offset, "SSND", 4) == 0 && chunkSize >= 8 && chunkDataOffset + 8 <= sizeBytes) {
            const uint32_t ssndOffset = readU32BE(data + chunkDataOffset);
            const size_t audioStart = chunkDataOffset + 8 + static_cast<size_t>(ssndOffset);
            if (audioStart < sizeBytes && chunkSize >= 8 + ssndOffset) {
                pcmData = data + audioStart;
                availablePcmBytes = std::min<size_t>(chunkSize - 8 - ssndOffset, sizeBytes - audioStart);
            }
        }

        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    return populateAudioFrames(pcmData, availablePcmBytes, totalHeaderFrames,
                               channels, sampleRate, bitsPerSample,
                               audioFormat, isBigEndian, analysisOnly, maxFrames);
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
    return readWavFromMemory(buffer.data(), buffer.size(), false, 0);
}

WavAudioData WavReader::readWavFileForAnalysis(const std::string& filePath, int64_t maxFrames) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    const std::streamsize fullSize = file.tellg();
    if (fullSize < 44) {
        return {};
    }

    // Read up to 768 KB initially (enough for headers + ~1.5s of stereo 32-bit audio)
    constexpr std::streamsize kInitialReadBytes = 768 * 1024;
    const std::streamsize readBytes = std::min(fullSize, kInitialReadBytes);
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(readBytes));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), readBytes)) {
        return {};
    }

    auto res = readWavFromMemory(buffer.data(), buffer.size(), true, maxFrames);
    if (res.isValid || readBytes == fullSize) {
        return res;
    }

    // Fallback if large metadata chunks preceded the audio data chunk
    file.seekg(0, std::ios::beg);
    buffer.resize(static_cast<size_t>(fullSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fullSize)) {
        return {};
    }
    return readWavFromMemory(buffer.data(), buffer.size(), true, maxFrames);
}

WavAudioData WavReader::readWavFromMemory(const uint8_t* data,
                                          size_t sizeBytes,
                                          bool analysisOnly,
                                          int64_t maxFrames) {
    if (sizeBytes < 44 || data == nullptr) {
        return {};
    }

    if (std::memcmp(data, "FORM", 4) == 0 &&
        (std::memcmp(data + 8, "AIFF", 4) == 0 || std::memcmp(data + 8, "AIFC", 4) == 0)) {
        return readAiffFromMemory(data, sizeBytes, analysisOnly, maxFrames);
    }

    if ((std::memcmp(data, "RIFF", 4) != 0 && std::memcmp(data, "RF64", 4) != 0 && std::memcmp(data, "BW64", 4) != 0) ||
        std::memcmp(data + 8, "WAVE", 4) != 0) {
        return {};
    }

    size_t offset = 12;
    int audioFormat = 1; // 1 = PCM, 3 = IEEE float, 0xFFFE = WAVE_FORMAT_EXTENSIBLE
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    const uint8_t* pcmData = nullptr;
    size_t availablePcmBytes = 0;
    uint32_t dataChunkBytes = 0;

    while (offset + 8 <= sizeBytes) {
        const uint32_t chunkSize = readU32LE(data + offset + 4);
        const size_t chunkDataOffset = offset + 8;

        if (std::memcmp(data + offset, "fmt ", 4) == 0 && chunkSize >= 16 && chunkDataOffset + 16 <= sizeBytes) {
            audioFormat = readU16LE(data + chunkDataOffset);
            channels = readU16LE(data + chunkDataOffset + 2);
            sampleRate = static_cast<int>(readU32LE(data + chunkDataOffset + 4));
            bitsPerSample = readU16LE(data + chunkDataOffset + 14);

            // Handle WAVE_FORMAT_EXTENSIBLE (0xFFFE)
            if (audioFormat == 0xFFFE && chunkSize >= 40 && chunkDataOffset + 26 <= sizeBytes) {
                audioFormat = readU16LE(data + chunkDataOffset + 24);
            }
        } else if (std::memcmp(data + offset, "data", 4) == 0) {
            if (chunkDataOffset < sizeBytes) {
                pcmData = data + chunkDataOffset;
                dataChunkBytes = chunkSize;
                availablePcmBytes = std::min<size_t>(chunkSize, sizeBytes - chunkDataOffset);
            }
            break;
        }

        if (chunkSize > sizeBytes || chunkDataOffset > sizeBytes - chunkSize) {
            break;
        }
        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    const int bytesPerSample = (bitsPerSample + 7) / 8;
    const int frameBytes = bytesPerSample * channels;
    const int64_t totalHeaderFrames = (frameBytes > 0)
        ? static_cast<int64_t>(dataChunkBytes / static_cast<uint32_t>(frameBytes))
        : 0;

    return populateAudioFrames(pcmData, availablePcmBytes, totalHeaderFrames,
                               channels, sampleRate, bitsPerSample,
                               audioFormat, false, analysisOnly, maxFrames);
}

} // namespace openfruit::core
