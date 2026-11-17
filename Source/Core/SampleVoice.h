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

struct VoiceSettings {
    float sampleStart = 0.0f;
    float sampleEnd = 1.0f;
    bool reverse = false;
    float attackSeconds = 0.0f;
    float decaySeconds = 0.0f;
    float sustainLevel = 1.0f;
    float releaseSeconds = 0.0f;
    float filterCutoffHz = 20000.0f;
    int chokeGroup = 0;
    int midiNote = -1;
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
               float pan = 0.0f,
               VoiceSettings settings = {});

    void stop();
    void noteOff() noexcept;
    void choke() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_; }
    [[nodiscard]] int chokeGroup() const noexcept { return chokeGroup_; }
    [[nodiscard]] int midiNote() const noexcept { return midiNote_; }

    // Renders up to numFrames into stereo left/right buffers, adding to existing contents.
    void renderNextBlock(float* leftOut, float* rightOut, int numFrames);

private:
    std::shared_ptr<const AudioSampleBuffer> sample_;
    double readPosition_ = 0.0;
    int integrationTaps_ = 1;
    double tapSpacing_ = 1.0;
    double speedRatio_ = 1.0;
    double hostSampleRate_ = 44100.0;
    double lowPassCoefficient_ = 1.0;
    double envelopeElapsedSamples_ = 0.0;
    double releaseElapsedSamples_ = 0.0;
    double releaseStartLevel_ = 1.0;
    int64_t sampleStart_ = 0;
    int64_t sampleEnd_ = 0;
    int direction_ = 1;
    float gainLeft_ = 1.0f;
    float gainRight_ = 1.0f;
    float filterLeft_ = 0.0f;
    float filterRight_ = 0.0f;
    float attackSeconds_ = 0.0f;
    float decaySeconds_ = 0.0f;
    float sustainLevel_ = 1.0f;
    float releaseSeconds_ = 0.0f;
    int chokeGroup_ = 0;
    int midiNote_ = -1;
    bool released_ = false;
    bool isPlaying_ = false;

    // Cubic interpolation helper
    static float interpolateCubic(const float* buffer,
                                  int64_t length,
                                  double pos,
                                  int64_t regionStart,
                                  int64_t regionEnd) noexcept;
    [[nodiscard]] float envelopeLevel() const noexcept;
};

} // namespace openfruit::core
