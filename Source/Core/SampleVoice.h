#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <cstdint>
#include "SonicAnalyzer.h"

namespace openfruit::core {

struct AudioSampleBuffer {
    std::string filePath;
    std::string sampleName;
    int numChannels = 0;
    double sampleRate = 44100.0;
    int64_t numSamples = 0;
    std::vector<std::vector<float>> channels; // [ch][sample]
    ToneAnalysisResult toneResult;
    bool isLoaded = false;
};

class SampleVoice {
public:
    SampleVoice() = default;

    void start(std::shared_ptr<const AudioSampleBuffer> sample,
               float velocity,
               int semitoneOffset,
               int trackBaseTune,
               double hostSampleRate,
               float volumeGain = 1.0f,
               float pan = 0.0f);

    void stop();
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_; }

    // Renders up to numFrames into stereo left/right buffers, adding to existing contents.
    void renderNextBlock(float* leftOut, float* rightOut, int numFrames);

private:
    std::shared_ptr<const AudioSampleBuffer> sample_;
    double readPosition_ = 0.0;
    double speedRatio_ = 1.0;
    float gainLeft_ = 1.0f;
    float gainRight_ = 1.0f;
    bool isPlaying_ = false;

    // Cubic interpolation helper
    static float interpolateCubic(const float* buffer, int64_t length, double pos) noexcept;
};

} // namespace openfruit::core
